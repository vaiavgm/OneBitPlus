#pragma once

// Host transport values can't be trusted: iPlug2's VST3 wrapper copies the host's process context without looking at
// its "tempo valid" / "music position valid" flags, so a host that doesn't fill them in while stopped leaves zeros or
// garbage. A tempo around 1e30 sends the LFO's phase-wrap loop (x -= 1 while x >= 1) into an endless loop on the audio
// thread, which silences the plugin until a valid context arrives (e.g. when playback starts).

#include <cmath>

namespace HostTime
{

constexpr double kDefaultTempo = 120.0;
constexpr double kMinTempo = 1.0;
constexpr double kMaxTempo = 10000.0; // far above anything real, far below anything harmful

// A usable tempo in BPM: the host's if it is plausible, otherwise 120.
inline double SanitizeTempo(double tempo)
{
  return (std::isfinite(tempo) && tempo >= kMinTempo && tempo <= kMaxTempo) ? tempo : kDefaultTempo;
}

// A usable position in quarter notes: the host's if it is a number, otherwise 0.
inline double SanitizePpq(double ppq) { return std::isfinite(ppq) ? ppq : 0.0; }

} // namespace HostTime
