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

// 1 = log what the host does (see VaiaOneBitPlus::LogDiagnostics). Read the lines starting with "OneBitPlus:" with
// DebugView (Sysinternals) or a debugger. Costs nothing when 0.
#ifndef ONEBIT_DIAGNOSTICS
#define ONEBIT_DIAGNOSTICS 1 // ON during testing; set to 0 for a release build
#endif

inline void DiagnosticsLog(const std::string& line)
{
  OutputDebugStringA((line + "\n").c_str()); // unlike MY_PRINTF this also reaches DebugView, not only an attached debugger
}

#else
#ifndef ONEBIT_DIAGNOSTICS
#define ONEBIT_DIAGNOSTICS 1 // ON during testing; set to 0 for a release build
#endif
inline void DiagnosticsLog(const std::string& line) { std::fprintf(stderr, "%s\n", line.c_str()); }

// Non-Windows: debug output is compiled out.
#define MY_PRINTF(fmt, ...) \
  do                        \
  {                         \
  } while (0)

#include <string>
inline void DebugPrintLong(const std::string&) {}
#endif
