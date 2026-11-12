// The REAL plugin class (constructor, parameters, OnReset, ProcessMidiMsg, ProcessBlock, OnIdle), driven like iPlug2's VST3
// wrapper drives it, without a host and without the editor. See tests/headless/README.md.
//
// Scenarios: a REAPER-like session (armed track, stopped, then Play, then no MIDI), the controller messages a host sends around
// transport changes, ghost voices after a host stop/start with a release set, and a random-session fuzz with an oracle (sound
// while a key is held, silence once everything is released). Exits non-zero on a failure.
#include "src/VaiaOneBitPlus.h"
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace iplug;

struct Host
{
  VaiaOneBitPlus plug{InstanceInfo{}};
  double sr = 48000.; int maxBlock = 512;
  double ppq = 0, tempo = 120; bool running = false;
  std::vector<sample> inL, inR, L, R;

  Host() : inL(8192, 0.), inR(8192, 0.), L(8192), R(8192) {}
  void Setup() { plug.SetSampleRate(sr); plug.SetBlockSize(maxBlock); plug.OnReset(); }     // IPlugVST3ProcessorBase::SetupProcessing
  void SetProcessing(bool on) { if (!on) plug.OnReset(); }                                 // ... SetProcessing(false) -> OnReset()

  struct Stats { double rms = 0, mean = 0; int nonzero = 0; bool finite = true; double peak = 0; };
  Stats Process(int n, std::vector<IMidiMsg> midi = {})
  {
    ITimeInfo ti; ti.mTempo = tempo; ti.mPPQPos = ppq; ti.mTransportIsRunning = running;
    plug.SetTimeInfo(ti);                                                                   // PrepareProcessContext
    for (auto& m : midi) plug.ProcessMidiMsg(m);                                           // ProcessMidiIn (before the audio)
    std::memset(L.data(), 0xAB, n * sizeof(sample)); std::memset(R.data(), 0xAB, n * sizeof(sample)); // host buffers are not zeroed
    sample* ins[2] = {inL.data(), inR.data()}; sample* outs[2] = {L.data(), R.data()};
    plug.ProcessBlock(ins, outs, n);                                                       // ProcessAudio -> ProcessBlock
    plug.OnIdle();                                                                         // the editor timer
    if (running) ppq += n / sr * tempo / 60.;
    Stats s; double sum = 0, sum2 = 0;
    for (int i = 0; i < n; ++i) { s.finite &= std::isfinite(L[i]) && std::isfinite(R[i]); sum += L[i]; sum2 += L[i] * L[i]; s.nonzero += L[i] != 0; s.peak = std::max(s.peak, std::fabs((double)L[i])); }
    s.mean = sum / n; s.rms = std::sqrt(std::max(0., sum2 / n - s.mean * s.mean));
    return s;
  }
  Stats Run(double seconds, std::vector<IMidiMsg> firstBlockMidi = {})
  {
    Stats last; int blocks = (int)(seconds * sr / 480); bool first = true;
    for (int b = 0; b < blocks; ++b) { last = Process(480, first ? firstBlockMidi : std::vector<IMidiMsg>{}); first = false; }
    return last;
  }
};
static IMidiMsg NoteOn(int n, int v) { IMidiMsg m; m.MakeNoteOnMsg(n, v, 0); return m; }
static IMidiMsg NoteOff(int n) { IMidiMsg m; m.MakeNoteOffMsg(n, 0); return m; }
static void Print(const char* what, const Host::Stats& s) { std::printf("  %-46s rms %.3f  peak %.3f  nonzero %3d/480  finite %d\n", what, s.rms, s.peak, s.nonzero, s.finite); }

#include <random>
#include <set>
#include <algorithm>
static IMidiMsg CC(int cc, int v) { IMidiMsg m; m.MakeControlChangeMsg((IMidiMsg::EControlChangeMsg)cc, v / 127.); return m; }

static int gFailures = 0;
#define EXPECT(cond, what)                                                       \
  do                                                                             \
  {                                                                              \
    if (!(cond))                                                                 \
    {                                                                            \
      ++gFailures;                                                               \
      std::printf("  FAIL: %s\n", what);                                         \
    }                                                                            \
  } while (0)

static void SetRelease(Host& h, double ms)
{
  for (int t = 0; t < kNumTypes; ++t)
    for (int idx : {TypeParam(t, kParamPwmRelease1), TypeParam(t, kParamPitchRelease1)})
    {
      h.plug.GetParam(idx)->Set(ms);
      h.plug.OnParamChange(idx);
    }
}

static void RealSession()
{
  std::printf("REAPER-like session, fresh instance, armed track:\n");
  Host h; h.Setup();
  EXPECT(h.Run(0.2).rms < 0.001, "silent while idle");
  EXPECT(h.Run(0.3, {NoteOn(60, 80)}).rms > 0.2, "a key pressed while the transport is STOPPED must sound");
  EXPECT(h.Run(0.3, {NoteOff(60)}).rms < 0.001, "silent after the key is released");
  h.Run(0.3, {NoteOn(64, 100)});
  h.running = true;
  EXPECT(h.Run(0.3).rms > 0.2, "pressing Play must not cut a held note");
  h.Run(0.1, {NoteOff(64)});
  EXPECT(h.Run(0.5).rms < 0.001, "playing with no MIDI at all is silent");
  h.running = false; h.SetProcessing(false); h.SetProcessing(true);
  EXPECT(h.Run(0.3).rms < 0.001, "silent after the host's stop/start");
  EXPECT(h.Run(0.3, {NoteOn(60, 80)}).rms > 0.2, "a key pressed after the stop/start sounds");
}

static void ControllerMessages()
{
  std::printf("controller messages around transport changes:\n");
  Host h; h.Setup();
  h.Run(0.3, {NoteOn(60, 80)});
  EXPECT(h.Run(0.4, {CC(123, 0)}).rms < 0.001, "CC123 all notes off silences the voice");
  for (int cc : {1, 7, 10, 11, 64, 91, 120, 121, 123})
    EXPECT(h.Run(0.4, {CC(cc, 64)}).rms < 0.001 && h.Run(0.1).peak < 0.01, ("controller " + std::to_string(cc) + " alone makes no sound").c_str());
}

static void GhostVoices()
{
  std::printf("no ghost voices after a host stop/start:\n");
  for (double release : {0., 50., 300., 500.})
  {
    Host h; h.Setup();
    SetRelease(h, release);
    h.plug.OnReset();
    EXPECT(h.Run(0.4, {CC(1, 0)}).peak < 0.01, "the very first MIDI message of a fresh instance makes no sound");
    for (int note : {60, 62, 64, 65, 67, 69}) h.Run(0.05, {NoteOn(note, 20), NoteOff(note)});
    h.SetProcessing(false); h.SetProcessing(true);
    double worst = 0;
    for (int b = 0; b < 60; ++b) worst = std::max(worst, h.Process(480, b == 0 ? std::vector<IMidiMsg>{CC(1, 0)} : std::vector<IMidiMsg>{}).peak);
    EXPECT(worst < 0.01, ("a controller after a stop/start makes no sound (release " + std::to_string((int)release) + " ms)").c_str());
  }
}

// mean of the output over a while (a single block is not a whole number of periods of the tone)
static double MeanOver(Host& h, double seconds, std::vector<IMidiMsg> firstMidi = {})
{
  double sum = 0; int blocks = std::max(1, (int)(seconds * h.sr / 480));
  for (int b = 0; b < blocks; ++b) sum += h.Process(480, b == 0 ? firstMidi : std::vector<IMidiMsg>{}).mean;
  return sum / blocks;
}

static void SetDcFilter(Host& h, bool on)
{
  h.plug.GetParam(kParamDcFilter)->Set(on ? 1. : 0.);
  h.plug.OnParamChange(kParamDcFilter);
}

static void DcFilterSwitch()
{
  std::printf("DC filter switch:\n");
  Host h; h.Setup();
  EXPECT(h.plug.GetParam(kParamDcFilter)->Value() == 1.0, "the DC filter defaults to ON");
  h.Run(0.4, {NoteOn(60, 80)});
  EXPECT(std::fabs(MeanOver(h, 0.3)) < 0.01 && h.Run(0.05).rms > 0.2, "ON: the offset is removed (mean about 0) and the tone is there");

  SetDcFilter(h, false);
  h.Run(0.1);
  const double offMean = MeanOver(h, 0.3);
  EXPECT(offMean > 0.2 && offMean < 0.3 && h.Run(0.05).peak > 0.49 && h.Run(0.05).peak < 0.51, "OFF: the raw 0/+0.5 pulse train, offset and all");

  SetDcFilter(h, true); // switching it on again must not click
  double peak = 0;
  for (int b = 0; b < 30; ++b) peak = std::max(peak, h.Process(480).peak);
  EXPECT(peak < 0.31, "switching ON again does not click (no overshoot)");
  EXPECT(std::fabs(MeanOver(h, 0.3)) < 0.01, "ON again: zero offset");
}

static void DcFilterMigration()
{
  std::printf("loading saved states:\n");
  // a state from the original version (dspVersion 0), where that slot was the retired "Input Protection" knob (0 in almost every project)
  { Host a; a.plug.GetParam(kParamDcFilter)->Set(0.);
    IByteChunk chunk; a.plug.SerializeParams(chunk); OneBitState::Write(chunk, {}, 0, 0);
    Host b; b.Setup(); SetDcFilter(b, false); b.plug.UnserializeState(chunk, 0);
    EXPECT(b.plug.GetParam(kParamDcFilter)->Value() == 1.0, "an original project (dspVersion 0) loads with the DC filter ON");
    EXPECT(std::fabs(MeanOver(b, 0.6, {NoteOn(60, 80)})) < 0.01, "...and it is on in the DSP too"); }
  // a project from the original version: parameters only, no custom block at all
  { Host a; a.plug.GetParam(kParamDcFilter)->Set(37.); // a stray value of the old slot
    IByteChunk chunk; a.plug.SerializeParams(chunk);
    Host b; b.Setup(); SetDcFilter(b, false); b.plug.UnserializeState(chunk, 0);
    EXPECT(b.plug.GetParam(kParamDcFilter)->Value() == 1.0, "a project from the original version loads with the DC filter ON"); }
  // this version's own state keeps the user's choice, both ways
  for (bool wanted : {false, true})
  { Host a; a.Setup(); SetDcFilter(a, wanted);
    IByteChunk chunk; a.plug.SerializeState(chunk);
    Host b; b.Setup(); SetDcFilter(b, !wanted); b.plug.UnserializeState(chunk, 0);
    EXPECT(b.plug.GetParam(kParamDcFilter)->Value() == (wanted ? 1.0 : 0.0), "the DC filter choice is saved with the project");
    const double mean = MeanOver(b, 0.6, {NoteOn(60, 80)});
    EXPECT(wanted ? std::fabs(mean) < 0.01 : mean > 0.2, "...and the DSP follows it after loading"); }
}

static int gOnlySession = -1; // replay one session verbosely

static void RandomSessions(int sessions)
{
  std::printf("random sessions (%d): block sizes, rates, transport, stop/start, one key at a time:\n", sessions);
  int violations = 0;
  for (int sess = 0; sess < sessions; ++sess)
  {
    if (gOnlySession >= 0 && sess != gOnlySession)
      continue;
    std::mt19937 rng(1000 + sess);
    auto rnd = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    Host h;
    h.sr = std::vector<double>{44100., 48000., 96000.}[rnd(0, 2)];
    h.maxBlock = std::vector<int>{64, 256, 512, 1024, 4096, 8192}[rnd(0, 5)];
    h.tempo = rnd(40, 240);
    h.Setup();
    double releaseSec = 0.;
    if (rnd(0, 1)) { releaseSec = rnd(0, 1) ? 0.3 : 0.05; SetRelease(h, releaseSec * 1000.); }
    std::set<int> held; double lastRelease = -10, lastNoteOn = -10, t = 0;
    for (int step = 0; step < 120; ++step)
    {
      if (rnd(0, 60) == 0) { h.SetProcessing(false); held.clear(); lastRelease = t; h.SetProcessing(true); } // host stop: OnReset, notes cut (before this block's events)
      const int n = rnd(1, h.maxBlock);
      struct Ev { int off, kind, key, vel; };
      std::vector<Ev> evs;
      for (int e = rnd(0, 6) == 0 ? rnd(1, 3) : 0; e > 0; --e) evs.push_back({rnd(0, n - 1), rnd(0, 9), rnd(48, 84), rnd(1, 127)});
      std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.off < b.off; }); // hosts deliver events in order
      std::vector<IMidiMsg> midi;
      for (auto& e : evs)
      {
        IMidiMsg m;
        if (e.kind <= 3)
        {
          for (int k : held) { IMidiMsg off; off.MakeNoteOffMsg(k, e.off); midi.push_back(off); lastRelease = t; }
          held.clear();
          m.MakeNoteOnMsg(e.key, e.vel, e.off); held.insert(e.key); lastNoteOn = t; midi.push_back(m);
        }
        else if (e.kind <= 6 && !held.empty()) { m.MakeNoteOffMsg(*held.begin(), e.off); held.clear(); lastRelease = t; midi.push_back(m); }
        else if (e.kind <= 8) { m = CC(std::vector<int>{7, 11, 91, 93}[e.vel % 4], e.key); m.mOffset = e.off; /* volume, expression, reverb, chorus: no sound of their own; the mod wheel is left out because it can legitimately mute (PWM at the extremes) */ midi.push_back(m); }
      }
      if (rnd(0, 40) == 0) h.running = !h.running;
      if (rnd(0, 50) == 0) h.tempo = rnd(40, 240);
      const auto s = h.Process(n, midi);
      if (gOnlySession >= 0)
      {
        std::printf("  step %3d n=%5d t=%.2f running=%d held=%zu rms %.3f peak %.3f", step, n, t, h.running, held.size(), s.rms, s.peak);
        for (auto& m : midi) std::printf(" [%02x %d %d @%d]", m.StatusMsg(), m.mData1, m.mData2, m.mOffset);
        std::printf("\n");
      }
      t += n / h.sr;
      bool bad = false;
      const bool modWheelSent = std::any_of(midi.begin(), midi.end(), [](const IMidiMsg& m) { return m.StatusMsg() == IMidiMsg::kControlChange && m.mData1 == 1; });
      if (!s.finite) { bad = true; std::printf("  session %d: NaN/inf\n", sess); }
      // a long enough block (a low note's half period is longer than a few dozen samples), a key held for a while and the previous note's
      // release over (two notes OR together into an almost constant level), no mod wheel
      // (it can legitimately mute: PWM at the extremes)
      if (!bad && midi.empty() && held.size() == 1 && t - lastNoteOn > 0.15 && t - lastRelease > releaseSec + 0.3 && n >= 512 && s.rms < 0.02 && !modWheelSent) { bad = true; std::printf("  session %d step %d: key held but silent (sr %.0f, max block %d, block %d)\n", sess, step, h.sr, h.maxBlock, n); }
      if (!bad && held.empty() && t - lastRelease > 1.0 && t - lastNoteOn > 1.0 && n >= 64 && (s.rms > 0.002 || s.peak > 0.01)) { bad = true; std::printf("  session %d step %d: nothing held but sound (rms %.3f, sr %.0f, max block %d, block %d)\n", sess, step, s.rms, h.sr, h.maxBlock, n); }
      violations += bad;
    }
  }
  EXPECT(violations == 0, "random sessions: no stuck or ghost sound, no silent held key, no NaN");
}

int main(int argc, char** argv)
{
  RealSession();
  ControllerMessages();
  GhostVoices();
  DcFilterSwitch();
  DcFilterMigration();
  if (argc > 2) gOnlySession = std::atoi(argv[2]);
  RandomSessions(argc > 1 ? std::atoi(argv[1]) : 150);
  std::printf("%s\n", gFailures ? "FAILED" : "all headless scenarios passed");
  return gFailures ? 1 : 0;
}
