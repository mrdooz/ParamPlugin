#pragma once

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <string>
#include <windows.h>

using namespace std;

string UTF16toUTF8(LPCWSTR pszTextUTF16);
wstring UTF8toUTF16(const char* utf8);
