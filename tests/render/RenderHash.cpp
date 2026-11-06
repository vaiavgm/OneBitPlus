// Offline render regression harness.
// Drives OneBitPlusDSP (the real iPlug2 MidiSynth/VoiceAllocator) with a fixed patch and MIDI script and prints
// an FNV-1a hash of the rendered output. A change that should not affect the sound must not change the hash.
// Build: see tests/render/README.md
//
// Reference: the ORIGINAL code (main @ 2a93f6b), built with -DHOST_TEMPO=120.0 -DONEBIT_NO_LEGACY_API
// (the original plugin never passed the host tempo on, so 120 BPM is what it effectively used):
//   48000 Hz: 0bf6817515ca0ac7      44100 Hz: b61090932549d1df
// The current code rendering "legacy" at HOST_TEMPO 140 must print exactly these. That is the proof that
// projects loaded in legacy mode keep their sound.
// Current code, non-legacy (HOST_TEMPO 140):  48000 Hz: 434fe5a91a646863      44100 Hz: 8fdb21257b0ef52f
// Non-legacy hashes change whenever the sound is changed on purpose: update them in the commit that does it.
#include "VaiaOneBitPlus_DSP.h"
#include <cstdio>
#include <vector>
#include <cstring>

#ifndef HOST_TEMPO
#define HOST_TEMPO 140.0
#endif
using namespace iplug;

static unsigned long long gHash = 1469598103934665603ULL;
static long gNonZero = 0;
static void HashBuf(const sample* b, int n)
{
  for (int i = 0; i < n; ++i)
    gNonZero += (b[i] != 0);
  for (int i = 0; i < n; ++i) { unsigned long long u; double d = b[i]; std::memcpy(&u, &d, 8); gHash ^= u; gHash *= 1099511628211ULL; } }

int main(int argc, char** argv)
{
  const double sr = argc > 1 ? atof(argv[1]) : 48000.0;
  // "legacy" renders like a project saved by the original version (see src/StateFormat.h, kCompatLegacyTiming)
  // "nodc" switches the output DC blocker off (to compare with the original code, which has none)
  bool legacy = false, noDc = false;
  for (int a = 2; a < argc; ++a)
  {
    legacy |= std::strcmp(argv[a], "legacy") == 0;
    noDc |= std::strcmp(argv[a], "nodc") == 0;
  }
  const int bs = 256;
  auto* dsp = new OneBitPlusDSP<sample>(32);
  dsp->Reset(sr, bs);
  // defaults like the plugin constructor (values of a fresh instance)
  auto setDefaults = [&]() {
    dsp->SetParam(kParamGain, 50.); dsp->SetParam(kParamNoteGlideTime, 0.);
    for (int t = 0; t < 4; ++t) {
      int o = t * (kParamPwmAttack2 - kParamPwmAttack1);
      double v[] = {0,0,0,0, 1,(double)LFO<>::kTriangle,1.,(double)LFO<>::k1,0, 0,1.0,0.5, 0,0,0,0, 1,(double)LFO<>::kTriangle,1.,(double)LFO<>::k1,0, 0.0,1,0.0, 1,0.0};
      for (int k = 0; k < 26; ++k) dsp->SetParam(kParamPwmAttack1 + o + k, v[k]);
    }
  };
  setDefaults();
#ifndef ONEBIT_NO_LEGACY_API // defined when building against the original code, which has no legacy switch
  dsp->mLegacyTiming = legacy;
  dsp->mDcBlockerEnabled = !noDc;
#endif
  FILE* dump = nullptr;
  for (int a = 2; a + 1 < argc; ++a)
    if (std::strcmp(argv[a], "dump") == 0)
      dump = std::fopen(argv[a + 1], "wb");
  // optional: raw output (left channel, doubles)
  // a characterful patch on types 1 and 3
  dsp->SetParam(kParamPwmAttack1, 120.); dsp->SetParam(kParamPwmDecay1, 300.); dsp->SetParam(kParamPwmSustain1, 40.); dsp->SetParam(kParamPwmRelease1, 200.);
  dsp->SetParam(kParamPwmLFODepth1, 30.); dsp->SetParam(kParamPwmLFORateMode1, 0.); dsp->SetParam(kParamPwmLFORateHz1, 5.);
  dsp->SetParam(kParamPitchLFODepth1, 5.); dsp->SetParam(kParamPitchLFORateMode1, 1.);
  dsp->SetParam(kParamExtraUnison1, 4.); dsp->SetParam(kParamExtraDetune1, 25.);
  dsp->SetParam(kParamPitchAttack3, 50.); dsp->SetParam(kParamPitchDecay3, 80.); dsp->SetParam(kParamPitchModPow3, 2.); dsp->SetParam(kParamPitchSustain3, 10.);
  dsp->SetParam(kParamPwmKeyTrack3, 1.); dsp->SetParam(kParamPwmOffset3, 0.2);
  dsp->SetParam(kParamPitchKeyTrack3, 0.5); dsp->SetParam(kParamPitchKeyTrack4, -0.25); // inert in the original version
  dsp->SetParam(kParamExtraUnison2, 8.); dsp->SetParam(kParamExtraDetune2, 60.);
  // tempo-synced LFOs on types 2 (pitch) and 4 (PWM), played while the transport runs and after it has stopped
  dsp->SetParam(kParamPitchLFODepth2, 8.); dsp->SetParam(kParamPitchLFORateMode2, 1.); dsp->SetParam(kParamPitchLFORateTempo2, (double)LFO<>::k16th);
  dsp->SetParam(kParamPwmLFODepth4, 40.); dsp->SetParam(kParamPwmLFORateMode4, 1.); dsp->SetParam(kParamPwmLFORateTempo4, (double)LFO<>::k32nd);
  if (legacy) // what VaiaOneBitPlus::ApplyMigrations does for states saved by the original version
  {
    for (int t = 0; t < 4; ++t)
    {
      dsp->SetParam(TypeParam(t, kParamPwmRelease1), 0.);
      dsp->SetParam(TypeParam(t, kParamPitchRelease1), 0.);
      dsp->SetParam(TypeParam(t, kParamPitchKeyTrack1), 0.);
    }
  }

  // one sample at velocity 1 (sampler note 3)
  std::vector<int8_t> smp(4000); for (size_t i = 0; i < smp.size(); ++i) smp[i] = (int8_t)((i * 37) ^ (i >> 3));
  dsp->mSampleManager.AddSample(smp.data(), smp.size(), (uint32_t)sr);

  struct Ev { int block, offset, status, note, vel; };
  std::vector<Ev> evs = {
    {2, 10, 0x90, 60, 20}, {2, 11, 0x90, 64, 20}, {5, 0, 0x90, 67, 70}, {10, 100, 0x90, 48, 100},
    {12, 3, 0xB0, 1, 64}, {20, 0, 0x80, 60, 0}, {22, 50, 0x90, 0, 90}, {23, 0, 0x90, 1, 40}, {24, 0, 0x90, 2, 120},
    {26, 7, 0x90, 3, 1}, {30, 0, 0xE0, 0, 96}, {35, 0, 0x80, 64, 0}, {36, 0, 0x80, 0, 0}, {36, 1, 0x80, 1, 0}, {36, 2, 0x80, 2, 0},
    {40, 0, 0x80, 67, 0}, {41, 0, 0x80, 48, 0}, {45, 0, 0x80, 3, 0}, {50, 0, 0x90, 72, 127}, {55, 0, 0x80, 72, 0},
    {60, 0, 0x90, 36, 127}, {60, 0, 0x90, 43, 33}, {90, 0, 0x80, 36, 0}, {90, 0, 0x80, 43, 0},
    {75, 0, 0x90, 55, 50}, {75, 0, 0x90, 62, 120}, {110, 0, 0x80, 55, 0}, {110, 0, 0x80, 62, 0}};
  std::vector<sample> L(bs), R(bs); sample* outs[2] = {L.data(), R.data()};
  double qn = 0;
  for (int blk = 0; blk < 140; ++blk) {
    for (auto& e : evs) if (e.block == blk) { IMidiMsg m(e.offset, (uint8_t)e.status, (uint8_t)e.note, (uint8_t)e.vel); dsp->ProcessMidiMsg(m); }
    std::memset(L.data(), 0, bs * sizeof(sample)); std::memset(R.data(), 0, bs * sizeof(sample));
    // host tempo (not 120 on purpose); the transport stops halfway so both LFO sync paths are exercised
    dsp->ProcessBlock(nullptr, outs, 2, bs, qn, blk < 70, HOST_TEMPO);
    qn += bs / sr * 2.0; // 120 bpm
    HashBuf(L.data(), bs); HashBuf(R.data(), bs);
    if (dump) std::fwrite(L.data(), sizeof(sample), bs, dump);
  }
  if (dump) std::fclose(dump);
  std::printf("sr=%.0f %s%s hash=%016llx nonzero=%ld\n", sr, legacy ? "legacy" : "new   ", noDc ? " nodc" : "     ", gHash, gNonZero);
}
