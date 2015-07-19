#include "utils.hpp"

//------------------------------------------------------------------------------
string UTF16toUTF8(LPCWSTR pszTextUTF16)
{
  string utf8;
  int len = WideCharToMultiByte(CP_UTF8, 0, pszTextUTF16, -1, NULL, 0, 0, 0);
  if (len > 1)
  {
    utf8.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, pszTextUTF16, -1, (LPSTR)utf8.data(), len, 0, 0);
    utf8.pop_back();
  }
  return utf8;
}

//------------------------------------------------------------------------------
wstring UTF8toUTF16(const char* utf8)
{
  wstring utf16;
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (len > 1)
  {
    utf16.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, (LPWSTR)utf16.data(), len);
  }
  return utf16;
}
