#pragma once

#if defined(_WIN32)

// Forward-declare OutputDebugStringA and IsDebuggerPresent to avoid #include <windows.h> namespace pollution entirely
extern "C" {
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
__declspec(dllimport) int __stdcall IsDebuggerPresent(void);
}

#include <cstdio>

#define MY_PRINTF(fmt, ...)                                                                                                                                                                            \
  do                                                                                                                                                                                                   \
  {                                                                                                                                                                                                    \
    if (IsDebuggerPresent())                                                                                                                                                                           \
    {                                                                                                                                                                                                  \
      char buf[512];                                                                                                                                                                                   \
      snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__);                                                                                                                                                  \
      OutputDebugStringA(buf);                                                                                                                                                                         \
    }                                                                                                                                                                                                  \
  } while (0)

#else
// Non-Windows: debug output is compiled out.
#define MY_PRINTF(fmt, ...) \
  do                        \
  {                         \
  } while (0)
#endif
