#pragma once

#if defined(_WIN32)

// Forward-declare OutputDebugStringA and IsDebuggerPresent to avoid #include <windows.h> namespace pollution entirely
extern "C" {
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
__declspec(dllimport) int __stdcall IsDebuggerPresent(void);
}

#include <cstdio>
#include <string>

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


// Prints arbitrarily long text (MY_PRINTF truncates at 512 bytes).
inline void DebugPrintLong(const std::string& text)
{
  if (!IsDebuggerPresent())
    return;
  constexpr size_t kChunk = 500;
  char buf[kChunk + 1];
  for (size_t pos = 0; pos < text.size(); pos += kChunk)
  {
    const size_t n = text.copy(buf, kChunk, pos);
    buf[n] = '\0';
    OutputDebugStringA(buf);
  }
  OutputDebugStringA("\n");
}

#else
// Non-Windows: debug output is compiled out.
#define MY_PRINTF(fmt, ...) \
  do                        \
  {                         \
  } while (0)

#include <string>
inline void DebugPrintLong(const std::string&) {}
#endif
