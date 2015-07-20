#include "utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "ParamPlugin.hpp"

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

using namespace std;

AEGP_PluginID g_pluginIdController;
AEGP_PluginID g_pluginIdInstance;

CRITICAL_SECTION g_cs;
CONDITION_VARIABLE g_cv;

char g_sendbuffer[16 * 1024 * 1024];
DWORD g_threadId = 0;

DWORD ServerThread(void* params);

bool Init()
{
  InitializeCriticalSection(&g_cs);
  InitializeConditionVariable(&g_cv);
  CreateThread(NULL, 0, ServerThread, NULL, 0, &g_threadId);
  return true;
}

bool g_initialized = Init();

struct Value
{
  Value() {}
  Value(float x, float y = 0, float z = 0) : x(x), y(y), z(z) {}

  friend bool operator==(const Value& lhs, const Value& rhs)
  {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
  }

  float x, y, z;
};

struct Keyframes
{
  string name;
  int dims = 0;
  Value firstValue;
  Value lastValue;
  float firstTime = 0;
  float lastTime = 0;
  float sampleStep;
  vector<Value> values;

  friend bool operator==(const Keyframes& lhs, const Keyframes& rhs) { return lhs.values == rhs.values; }

  friend bool operator!=(const Keyframes& lhs, const Keyframes& rhs) { return !(lhs == rhs); }

  int ToBuffer(char* buf) const;
};

template <typename T>
int CopyToBuffer(char* buf, const T& t)
{
  memcpy(buf, &t, sizeof(t));
  return sizeof(t);
}

int ValueToBuffer(char* buf, const Value& v, int dims)
{
  char* org = buf;
  switch (dims)
  {
    case 3:
      buf += CopyToBuffer(buf, v.x);
      buf += CopyToBuffer(buf, v.y);
      buf += CopyToBuffer(buf, v.z);
      break;
    case 2:
      buf += CopyToBuffer(buf, v.x);
      buf += CopyToBuffer(buf, v.y);
      break;
    case 1: buf += CopyToBuffer(buf, v.x); break;
  }

  return (int)(buf - org);
}

int Keyframes::ToBuffer(char* buf) const
{
  const char* orgBuf = buf;

  int stringLen = (int)name.size();
  buf += CopyToBuffer(buf, stringLen);
  memcpy(buf, name.data(), stringLen);
  buf += stringLen;
  buf += CopyToBuffer(buf, dims);
  buf += ValueToBuffer(buf, firstValue, dims);
  buf += ValueToBuffer(buf, lastValue, dims);
  buf += CopyToBuffer(buf, firstTime);
  buf += CopyToBuffer(buf, lastTime);
  buf += CopyToBuffer(buf, sampleStep);

  buf += CopyToBuffer(buf, (int)values.size());

  switch (dims)
  {
    case 1:
      for (size_t i = 0; i < values.size(); ++i)
      {
        buf += CopyToBuffer(buf, values[i].x);
      }
      break;
    case 2:
      for (size_t i = 0; i < values.size(); ++i)
      {
        buf += CopyToBuffer(buf, values[i].x);
        buf += CopyToBuffer(buf, values[i].y);
      }
      break;
    case 3:
      for (size_t i = 0; i < values.size(); ++i)
      {
        buf += CopyToBuffer(buf, values[i]);
      }
      break;
  }

  return (int)(buf - orgBuf);
}

unordered_map<string, Keyframes> g_keyframes;
unordered_set<string> g_newKeyframes;

PF_State g_controllerParamStates[(int)ControllerItems::CONTROLLER_NUM_ITEMS] = {0};
PF_State g_instanceParamStates[(int)InstanceItems::INSTANCE_NUM_ITEMS] = {0};

unordered_map<string, AEGP_InstalledEffectKey> g_effectKeys;
bool g_firstSend = true;

string StringFromMemHandle(AEGP_SuiteHandler& suites, AEGP_MemHandle h)
{
  LPCWSTR w;
  suites.MemorySuite1()->AEGP_LockMemHandle(h, (void**)&w);
  string res = UTF16toUTF8(w);

  suites.MemorySuite1()->AEGP_UnlockMemHandle(h);
  suites.MemorySuite1()->AEGP_FreeMemHandle(h);
  return res;
}

//------------------------------------------------------------------------------
static PF_Err About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_SPRINTF(out_data->return_msg, "%s, v%d.%d\r%s", NAME, MAJOR_VERSION, MINOR_VERSION, DESCRIPTION);

  return PF_Err_NONE;
}

//------------------------------------------------------------------------------
DWORD ServerThread(void* params)
{
  WSADATA wsaData;
  int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (res != NO_ERROR)
  {
    wprintf(L"WSAStartup() failed with error: %d\n", res);
    return 1;
  }

  // Create a SOCKET for listening for incoming connection requests.
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

  // Listen for incoming connection requests
  // on the created socket
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

//------------------------------------------------------------------------------
static PF_Err GlobalSetupController(
    PF_InData* in_dataP, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;

  out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);

  out_data->out_flags = PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY;

  out_data->out_flags2 = 0;

  // This appears to be the recommended way to get a plugin id..
  AEGP_GlobalRefcon g = NULL;
  const A_char* s = NAME;
  AEGP_UtilitySuite5* utilitySuite = NULL;
  AEFX_AcquireSuite(
      in_dataP, out_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5, NULL, (void**)&utilitySuite);
  utilitySuite->AEGP_RegisterWithAEGP(g, s, &g_pluginIdController);
  AEFX_ReleaseSuite(in_dataP, out_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5, NULL);

  // Iterate all installed effects, so we can grab the ParamInstance effect keys
  AEGP_EffectSuite3* effectSuite = NULL;
  AEFX_AcquireSuite(
      in_dataP, out_data, kAEGPEffectSuite, kAEGPEffectSuiteVersion3, NULL, (void**)&effectSuite);
  AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
  while (true)
  {
    effectSuite->AEGP_GetNextInstalledEffect(key, &key);
    if (key == AEGP_InstalledEffectKey_NONE)
      break;

    A_char buf[256];
    effectSuite->AEGP_GetEffectName(key, buf);
    g_effectKeys[buf] = key;
  }

  AEFX_ReleaseSuite(in_dataP, out_data, kAEGPEffectSuite, kAEGPEffectSuiteVersion3, NULL);

  return err;
}

//------------------------------------------------------------------------------
static PF_Err GlobalSetupInstance(
    PF_InData* in_dataP, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;

  out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);

  out_data->out_flags = PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY |
                        PF_OutFlag_I_AM_OBSOLETE;

  out_data->out_flags2 = 0;

  // This appears to be the recommended way to get a plugin id..
  AEGP_GlobalRefcon g = NULL;
  const A_char* s = NAME;
  AEGP_UtilitySuite5* utilitySuite = NULL;
  AEFX_AcquireSuite(
      in_dataP, out_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5, NULL, (void**)&utilitySuite);
  utilitySuite->AEGP_RegisterWithAEGP(g, s, &g_pluginIdInstance);
  AEFX_ReleaseSuite(in_dataP, out_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion5, NULL);

  return err;
}

//------------------------------------------------------------------------------
static PF_Err ParamsSetupController(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;
  AEFX_CLR_STRUCT(def);
  def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_INTERP;

  PF_ADD_BUTTON("AddParam1d", "Add Param 1d", 0, PF_ParamFlag_SUPERVISE, (A_long)ControllerItems::CONTROLLER_ADD_1D);
  PF_ADD_BUTTON("AddParam2d", "Add Param 2d", 0, PF_ParamFlag_SUPERVISE, (A_long)ControllerItems::CONTROLLER_ADD_2D);
  PF_ADD_BUTTON("AddParam3d", "Add Param 3d", 0, PF_ParamFlag_SUPERVISE, (A_long)ControllerItems::CONTROLLER_ADD_3D);

  out_data->num_params = (A_long)ControllerItems::CONTROLLER_NUM_ITEMS;

  return err;
}

//------------------------------------------------------------------------------
static PF_Err ParamsSetupInstance1d(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;
  AEFX_CLR_STRUCT(def);

  PF_ADD_FLOAT_SLIDERX("Value",
      -FLT_MAX,
      +FLT_MAX,
      SLIDER_MIN,
      SLIDER_MAX,
      1,
      SLIDER_PRECISION,
      0,
      PF_ParamFlag_SUPERVISE,
      (A_long)InstanceItems::INSTANCE_SLIDER_1);

  out_data->num_params = (A_long)InstanceItems::INSTANCE_NUM_ITEMS;

  return err;
}

//------------------------------------------------------------------------------
static PF_Err ParamsSetupInstance2d(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;
  AEFX_CLR_STRUCT(def);

  PF_ADD_POINT("Value", 0, 0, FALSE, (A_long)InstanceItems::INSTANCE_SLIDER_1);

  out_data->num_params = (A_long)InstanceItems::INSTANCE_NUM_ITEMS;

  return err;
}

//------------------------------------------------------------------------------
static PF_Err ParamsSetupInstance3d(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;
  AEFX_CLR_STRUCT(def);

  PF_ADD_POINT_3D("Value", 0, 0, 0, (A_long)InstanceItems::INSTANCE_SLIDER_1);

  out_data->num_params = (A_long)InstanceItems::INSTANCE_NUM_ITEMS;

  return err;
}

//------------------------------------------------------------------------------
static PF_Err UserChangedParamController(PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* outputP,
    PF_UserChangedParamExtra* extra)
{
  PF_Err err = PF_Err_NONE;

  AEGP_SuiteHandler suites(in_data->pica_basicP);

  AEGP_LayerH layer;
  suites.LayerSuite7()->AEGP_GetActiveLayer(&layer);
  if (!layer)
  {
    return err;
  }

  if (extra->param_index == (int)ControllerItems::CONTROLLER_ADD_1D)
  {
    AEGP_EffectRefH effect;
    AEGP_InstalledEffectKey key = g_effectKeys["ParamInstance1d"];
    suites.EffectSuite3()->AEGP_ApplyEffect(g_pluginIdController, layer, key, &effect);
    suites.EffectSuite3()->AEGP_DisposeEffect(effect);
  }
  else if (extra->param_index == (int)ControllerItems::CONTROLLER_ADD_2D)
  {
    AEGP_EffectRefH effect;
    AEGP_InstalledEffectKey key = g_effectKeys["ParamInstance2d"];
    suites.EffectSuite3()->AEGP_ApplyEffect(g_pluginIdController, layer, key, &effect);
    suites.EffectSuite3()->AEGP_DisposeEffect(effect);
  }

  else if (extra->param_index == (int)ControllerItems::CONTROLLER_ADD_3D)
  {
    AEGP_EffectRefH effect;
    AEGP_InstalledEffectKey key = g_effectKeys["ParamInstance3d"];
    suites.EffectSuite3()->AEGP_ApplyEffect(g_pluginIdController, layer, key, &effect);
    suites.EffectSuite3()->AEGP_DisposeEffect(effect);
  }

  return err;
}

//------------------------------------------------------------------------------
static PF_Err UIParamChangedController(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* outputP, void* extra)
{
  PF_Err err = PF_Err_NONE;

  AEGP_SuiteHandler suites(in_data->pica_basicP);

  // Check which parameters have changed
  PF_ProgPtr effect = in_data->effect_ref;
  PF_ParamUtilsSuite3* paramUtils = suites.ParamUtilsSuite3();
  for (int i = 0; i < (int)ControllerItems::CONTROLLER_NUM_ITEMS - 1; ++i)
  {
    PF_State curState;
    paramUtils->PF_GetCurrentState(effect, i + 1, NULL, NULL, &curState);
    A_Boolean res;
    paramUtils->PF_AreStatesIdentical(effect, &g_controllerParamStates[i], &curState, &res);
    if (!res)
    {
      // parameter changed
    }
    g_controllerParamStates[i] = curState;
  }

  return err;
}

//------------------------------------------------------------------------------
Value ValueFromStreamValue(AEGP_StreamType streamType, const AEGP_StreamValue2& val)
{
  switch (streamType)
  {
    case AEGP_StreamType_OneD: return Value((float)val.val.one_d);
    case AEGP_StreamType_TwoD: return Value((float)val.val.two_d.x, (float)val.val.two_d.y);
    case AEGP_StreamType_ThreeD:
      return Value((float)val.val.three_d.x, (float)val.val.three_d.y, (float)val.val.three_d.z);
  }

  return Value();
}

//------------------------------------------------------------------------------
static PF_Err UIParamChangedInstance(
    PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* outputP, void* extra)
{
  PF_Err err = PF_Err_NONE;

  AEGP_SuiteHandler suites(in_data->pica_basicP);

  // Sadly I don't have a good way of determining if anything has changed just from the parameters
  // to the function, so now I just compute all the keyframes and compare

  A_long numProjects;
  suites.ProjSuite6()->AEGP_GetNumProjects(&numProjects);
  AEGP_ProjectH project;
  suites.ProjSuite6()->AEGP_GetProjectByIndex(0, &project);

  AEGP_TimeDisplay3 timeDisplay;
  suites.ProjSuite6()->AEGP_GetProjectTimeDisplay(project, &timeDisplay);

  AEGP_ItemH item;
  suites.ItemSuite8()->AEGP_GetFirstProjItem(project, &item);

  while (item)
  {
    AEGP_ItemType itemType;
    suites.ItemSuite8()->AEGP_GetItemType(item, &itemType);
    if (itemType == AEGP_ItemType_COMP)
    {
      AEGP_CompH comp;
      suites.CompSuite9()->AEGP_GetCompFromItem(item, &comp);

      // The work area duration is the full duration of the composition
      A_Time duration;
      suites.CompSuite9()->AEGP_GetCompWorkAreaDuration(comp, &duration);

      // samples / second
      int sampleRate = 20;

      A_long numLayers = 0;
      suites.LayerSuite7()->AEGP_GetCompNumLayers(comp, &numLayers);
      for (int layerIdx = 0; layerIdx < numLayers; ++layerIdx)
      {
        AEGP_LayerH layer;
        suites.LayerSuite7()->AEGP_GetCompLayerByIndex(comp, layerIdx, &layer);

        AEGP_MemHandle hLayerName, hSourceName;
        suites.LayerSuite7()->AEGP_GetLayerName(g_pluginIdInstance, layer, &hLayerName, &hSourceName);
        string layerName = StringFromMemHandle(suites, hLayerName);
        string sourceName = StringFromMemHandle(suites, hSourceName);

        A_long numEffects = 0;
        suites.EffectSuite3()->AEGP_GetLayerNumEffects(layer, &numEffects);
        for (int effectIdx = 0; effectIdx < numEffects; ++effectIdx)
        {
          AEGP_EffectRefH effectRef;
          suites.EffectSuite3()->AEGP_GetLayerEffectByIndex(g_pluginIdInstance, layer, effectIdx, &effectRef);

          AEGP_InstalledEffectKey key;
          A_char effectName[AEGP_MAX_ITEM_NAME_SIZE] = {'\0'};
          A_char effectMatchName[AEGP_MAX_ITEM_NAME_SIZE] = {'\0'};
          suites.EffectSuite3()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &key);
          suites.EffectSuite3()->AEGP_GetEffectName(key, effectName);
          suites.EffectSuite3()->AEGP_GetEffectMatchName(key, effectMatchName);

          // We only care about exporting 1d parameter effects (for now)
          if (!strstr(effectMatchName, "ADBE ParamInstance1d"))
          {
            suites.EffectSuite3()->AEGP_DisposeEffect(effectRef);
            continue;
          }

          A_long numParams = 0;
          suites.StreamSuite4()->AEGP_GetEffectNumParamStreams(effectRef, &numParams);

          for (int paramIdx = 1; paramIdx < numParams; ++paramIdx)
          {
            PF_ParamType paramType;
            PF_ParamDefUnion paramDef;
            suites.EffectSuite3()->AEGP_GetEffectParamUnionByIndex(
                g_pluginIdInstance, effectRef, paramIdx, &paramType, &paramDef);

            AEGP_StreamRefH streamRef;
            suites.StreamSuite4()->AEGP_GetNewEffectStreamByIndex(
                g_pluginIdInstance, effectRef, paramIdx, &streamRef);

            AEGP_StreamType streamType;
            suites.StreamSuite4()->AEGP_GetStreamType(streamRef, &streamType);
            if (streamType == AEGP_StreamType_NO_DATA)
            {
              suites.StreamSuite4()->AEGP_DisposeStream(streamRef);
              continue;
            }

            AEGP_StreamRefH parentStream;
            suites.DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(
                g_pluginIdInstance, streamRef, &parentStream);

            // Jump through some hoops to get the renamed stream name..
            AEGP_MemHandle h;
            suites.StreamSuite4()->AEGP_GetStreamName(g_pluginIdInstance, parentStream, TRUE, &h);
            string streamName = StringFromMemHandle(suites, h);

            // If the name starts with "-", use it verbatim, otherwise append the layer name
            if (!streamName.empty())
            {
              if (streamName[0] == '-')
              {
                streamName.assign(&streamName[1]);
              }
              else
              {
                streamName = layerName + "." + streamName;
              }
            }

            suites.StreamSuite4()->AEGP_DisposeStream(parentStream);

            // sample the values at the given sample rate
            float totalTime = (float)duration.value / duration.scale;

            Keyframes keyframes;
            keyframes.name = streamName;
            keyframes.values.reserve((int)(totalTime / 1.0f / sampleRate));
            keyframes.sampleStep = 1.0f / sampleRate;

            // get the first and last keyframes
            A_long numKeyframes;
            suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(streamRef, &numKeyframes);
            A_Time firstKeyTime, lastKeyTime;
            int firstIdx = 0;
            int lastIdx = max(0, numKeyframes - 1);
            suites.KeyframeSuite4()->AEGP_GetKeyframeTime(
                streamRef, firstIdx, AEGP_LTimeMode_LayerTime, &firstKeyTime);
            suites.KeyframeSuite4()->AEGP_GetKeyframeTime(
                streamRef, lastIdx, AEGP_LTimeMode_LayerTime, &lastKeyTime);

            AEGP_StreamValue2 firstKeyValue, lastKeyValue;
            suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(
                g_pluginIdInstance, streamRef, firstIdx, &firstKeyValue);
            suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(
                g_pluginIdInstance, streamRef, lastIdx, &lastKeyValue);

            keyframes.firstTime = firstKeyTime.value / (float)firstKeyTime.scale;
            keyframes.lastTime = lastKeyTime.value / (float)lastKeyTime.scale;
            keyframes.firstValue = ValueFromStreamValue(streamType, firstKeyValue);
            keyframes.lastValue = ValueFromStreamValue(streamType, lastKeyValue);

            float lastTime = (float)lastKeyTime.value / lastKeyTime.scale;

            suites.StreamSuite4()->AEGP_DisposeStreamValue(&firstKeyValue);
            suites.StreamSuite4()->AEGP_DisposeStreamValue(&lastKeyValue);

            switch (streamType)
            {
              case AEGP_StreamType_OneD: keyframes.dims = 1; break;
              case AEGP_StreamType_TwoD: keyframes.dims = 2; break;
              case AEGP_StreamType_ThreeD: keyframes.dims = 3; break;
            }

            A_Time curTime;
            // num is just a scale factor, used in both scale and
            // value
            int num = 1000;
            curTime.scale = sampleRate * num;
            curTime.value = curTime.scale * firstKeyTime.value / firstKeyTime.scale;

            while ((float)curTime.value / curTime.scale <= lastTime)
            {
              AEGP_StreamValue2 val;
              suites.StreamSuite4()->AEGP_GetNewStreamValue(
                  g_pluginIdInstance, streamRef, AEGP_LTimeMode_LayerTime, &curTime, TRUE, &val);

              keyframes.values.push_back(ValueFromStreamValue(streamType, val));

              suites.StreamSuite4()->AEGP_DisposeStreamValue(&val);

              curTime.value += num;
            }

            bool newFrames = false;

            EnterCriticalSection(&g_cs);
            auto it = g_keyframes.find(streamName);
            if (it == g_keyframes.end() || it->second != keyframes)
            {
              g_keyframes[streamName] = keyframes;
              g_newKeyframes.insert(streamName);
              newFrames = true;
            }
            else
            {
              g_newKeyframes.erase(streamName);
            }
            LeaveCriticalSection(&g_cs);

            if (newFrames)
            {
              WakeConditionVariable(&g_cv);
            }

            suites.StreamSuite4()->AEGP_DisposeStream(streamRef);
          }

          suites.EffectSuite3()->AEGP_DisposeEffect(effectRef);
        }
      }
    }
    suites.ItemSuite8()->AEGP_GetNextProjItem(project, item, &item);
  }

  return err;
}

//------------------------------------------------------------------------------
PF_Err EntryPointFuncController(PF_Cmd cmd,
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
  PF_Err err = PF_Err_NONE;

  switch (cmd)
  {
    case PF_Cmd_ABOUT: err = About(in_dataP, out_data, params, output); break;
    case PF_Cmd_GLOBAL_SETUP: err = GlobalSetupController(in_dataP, out_data, params, output); break;
    case PF_Cmd_PARAMS_SETUP: err = ParamsSetupController(in_dataP, out_data, params, output); break;
    case PF_Cmd_USER_CHANGED_PARAM:
      err = UserChangedParamController(in_dataP, out_data, params, output, (PF_UserChangedParamExtra*)extra);
      break;

      // case PF_Cmd_UPDATE_PARAMS_UI:
      //  err = UIParamChangedController(in_dataP, out_data, params, output, extra);
      //  break;
  }
  return err;
}

//------------------------------------------------------------------------------
PF_Err EntryPointFuncInstance1d(PF_Cmd cmd,
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
  PF_Err err = PF_Err_NONE;

  switch (cmd)
  {
    case PF_Cmd_ABOUT: err = About(in_dataP, out_data, params, output); break;
    case PF_Cmd_GLOBAL_SETUP: err = GlobalSetupInstance(in_dataP, out_data, params, output); break;
    case PF_Cmd_PARAMS_SETUP: err = ParamsSetupInstance1d(in_dataP, out_data, params, output); break;
    case PF_Cmd_UPDATE_PARAMS_UI:
      err = UIParamChangedInstance(in_dataP, out_data, params, output, extra);
      break;
  }
  return err;
}

//------------------------------------------------------------------------------
PF_Err EntryPointFuncInstance2d(PF_Cmd cmd,
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
  PF_Err err = PF_Err_NONE;

  switch (cmd)
  {
    case PF_Cmd_ABOUT: err = About(in_dataP, out_data, params, output); break;
    case PF_Cmd_GLOBAL_SETUP: err = GlobalSetupInstance(in_dataP, out_data, params, output); break;
    case PF_Cmd_PARAMS_SETUP: err = ParamsSetupInstance2d(in_dataP, out_data, params, output); break;
    case PF_Cmd_UPDATE_PARAMS_UI:
      err = UIParamChangedInstance(in_dataP, out_data, params, output, extra);
      break;
  }
  return err;
}

//------------------------------------------------------------------------------
PF_Err EntryPointFuncInstance3d(PF_Cmd cmd,
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
  PF_Err err = PF_Err_NONE;

  switch (cmd)
  {
    case PF_Cmd_ABOUT: err = About(in_dataP, out_data, params, output); break;
    case PF_Cmd_GLOBAL_SETUP: err = GlobalSetupInstance(in_dataP, out_data, params, output); break;
    case PF_Cmd_PARAMS_SETUP: err = ParamsSetupInstance3d(in_dataP, out_data, params, output); break;
    case PF_Cmd_UPDATE_PARAMS_UI:
      err = UIParamChangedInstance(in_dataP, out_data, params, output, extra);
      break;
  }
  return err;
}
