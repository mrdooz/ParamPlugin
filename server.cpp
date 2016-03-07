#include "server.hpp"
#include "ParamPlugin.hpp"

CRITICAL_SECTION g_cs;
CONDITION_VARIABLE g_cv;

static char g_sendbuffer[16 * 1024 * 1024];
static bool g_firstSend = true;
static DWORD g_threadId = 0;

static DWORD ServerThread(void* params);

//------------------------------------------------------------------------------
bool Init()
{
  InitializeCriticalSection(&g_cs);
  InitializeConditionVariable(&g_cv);
  CreateThread(NULL, 0, ServerThread, NULL, 0, &g_threadId);
  return true;
}

bool g_initialized = Init();

//------------------------------------------------------------------------------
static DWORD ServerThread(void* params)
{
  WSADATA wsaData;
  int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (res != NO_ERROR)
  {
    wprintf(L"WSAStartup() failed with error: %d\n", res);
    return 1;
  }

  // Create a socket for listening for incoming connection requests.
  SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket == INVALID_SOCKET)
  {
    wprintf(L"socket function failed with error: %ld\n", WSAGetLastError());
    WSACleanup();
    return 1;
  }

  // The sockaddr_in structure specifies the address family,
  // IP address, and port for the socket that is being bound.
  sockaddr_in service;
  service.sin_family = AF_INET;
#pragma warning(suppress: 4996)
  service.sin_addr.s_addr = inet_addr("127.0.0.1");
  service.sin_port = htons(1337);

  res = bind(listenSocket, (SOCKADDR*)&service, sizeof(service));
  if (res == SOCKET_ERROR)
  {
    // wprintf(L"bind function failed with error %d\n", WSAGetLastError());
    res = closesocket(listenSocket);
    if (res == SOCKET_ERROR)
      wprintf(L"closesocket function failed with error %d\n", WSAGetLastError());
    WSACleanup();
    return 1;
  }

  // Listen for incoming connection requests on the created socket
  while (true)
  {
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
      wprintf(L"listen function failed with error: %d\n", WSAGetLastError());
      return 1;
    }

    wprintf(L"Listening on socket...\n");

    // Accept a client socket
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET)
    {
      printf("accept failed: %d\n", WSAGetLastError());
      closesocket(listenSocket);
      WSACleanup();
      return 1;
    }

    while (true)
    {
      // check if there are any new keyframes to send
      EnterCriticalSection(&g_cs);

      char* buf = g_sendbuffer;
      // write a dummy 0 to the buffer that we'll fill in with the true payload
      // size when we know it
      int dummy = 0;
      buf += CopyToBuffer(buf, dummy);

      if (g_firstSend)
      {
        // if first, connect, send everything
        buf += CopyToBuffer(buf, (int)g_keyframes.size());
        for (auto kv : g_keyframes)
        {
          const Keyframes& keyframe = kv.second;
          buf += keyframe.ToBuffer(buf);
        }
      }
      else
      {
        while (g_newKeyframes.empty())
        {
          SleepConditionVariableCS(&g_cv, &g_cs, INFINITE);
        }

        // New keyframes. Copy them to a flat buffer, and send them to the client
        buf += CopyToBuffer(buf, (int)g_newKeyframes.size());
        for (const string& keyframes : g_newKeyframes)
        {
          const Keyframes& keyframe = g_keyframes[keyframes];
          buf += keyframe.ToBuffer(buf);
        }

        g_newKeyframes.clear();
      }

      LeaveCriticalSection(&g_cs);

      // write the final payload size to the start of the buffer
      int bytesToSend = (int)(buf - g_sendbuffer);
      CopyToBuffer(g_sendbuffer, bytesToSend);

      // send to the client
      int res = send(clientSocket, g_sendbuffer, bytesToSend, 0);
      if (res == SOCKET_ERROR)
      {
        // client dc:ed, so break out of the send loop, and wait for them to connect again
        break;
      }
    }
  }

  WSACleanup();
  return 0;
}
