// A fresh plugin instance must make sound, whatever the host reports.
//
// Builds every parameter exactly like the plugin constructor does (src/ParamInit.h), pushes the values to the DSP exactly
// like VaiaOneBitPlus::OnReset does, plays one held note and checks that the output is a lively signal. It does that
// with the transport stopped and running and with hostile host values (tempo 0, negative, NaN, infinity, 1e30...; position
// NaN / infinity / 1e300), because iPlug2's VST3 wrapper hands the host's process context on without checking it.
// A watchdog turns a hung audio thread into a failure.
//
// Build: see tests/render/README.md (needs IPlugParameter.cpp too).

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

#include "VaiaOneBitPlus_DSP.h"
#include "src/ParamInit.h"

using namespace iplug;

struct Outcome
{
  double std = 0, duty = 0;
  bool finite = true;
};

static Outcome Play(std::vector<IParam>& params, double sr, int note, int vel, bool running, double tempo, double ppq, bool legacy)
{
  auto* dsp = new OneBitPlusDSP<sample>(32);
  dsp->mLegacyTiming = legacy;
  const int bs = 256;
  dsp->Reset(sr, bs);
  for (int p = 0; p < kNumParams; ++p) // what VaiaOneBitPlus::OnReset does
    dsp->SetParam(p, params[p].Value());

  std::vector<sample> L(bs), R(bs);
  sample* outs[2] = {L.data(), R.data()};
  Outcome o;
  double sum = 0, sum2 = 0;
  long n = 0, positive = 0;
  const int blocks = static_cast<int>(0.6 * sr / bs);
  double qn = ppq;
  for (int blk = 0; blk < blocks; ++blk)
  {
    if (blk == 2)
    {
      IMidiMsg m(0, 0x90, static_cast<uint8_t>(note), static_cast<uint8_t>(vel));
      dsp->ProcessMidiMsg(m);
    }
    std::memset(L.data(), 0, bs * sizeof(sample));
    std::memset(R.data(), 0, bs * sizeof(sample));
    dsp->ProcessBlock(nullptr, outs, 2, bs, qn, running, tempo);
    qn += bs / sr * 2.0;
    if (blk >= blocks / 3)
      for (int i = 0; i < bs; ++i)
      {
        o.finite &= std::isfinite(L[i]) && L[i] == R[i];
        sum += L[i];
        sum2 += L[i] * static_cast<double>(L[i]);
        positive += L[i] > 0;
        ++n;
      }
  }
  const double mean = sum / n;
  o.std = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
  o.duty = static_cast<double>(positive) / n;
  delete dsp;
  return o;
}

int main()
{
  std::atomic<bool> done{false};
  std::thread watchdog([&] {
    for (int i = 0; i < 600 && !done; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!done)
    {
      std::printf("FAIL: the audio thread hung (watchdog)\n");
      std::fflush(stdout);
      std::_Exit(2);
    }
  });

  std::vector<IParam> params(kNumParams);
  InitParams([&](int i) { return &params[i]; });

  const int t0 = 0;
  std::printf("defaults of type I: PWM sustain %.0f%%  PWM offset %.2f  PWM mod %.2f  PWM attack/decay/release %.0f/%.0f/%.0f ms  gain %.0f%%  unison %d\n", params[TypeParam(t0, kParamPwmSustain1)].Value(),
              params[TypeParam(t0, kParamPwmOffset1)].Value(), params[TypeParam(t0, kParamPwmModPow1)].Value(), params[TypeParam(t0, kParamPwmAttack1)].Value(), params[TypeParam(t0, kParamPwmDecay1)].Value(),
              params[TypeParam(t0, kParamPwmRelease1)].Value(), params[kParamGain].Value(), static_cast<int>(params[TypeParam(t0, kParamExtraUnison1)].Value()));

  const double inf = std::numeric_limits<double>::infinity(), nan = std::numeric_limits<double>::quiet_NaN();
  const double tempos[] = {120., 60., 200., 0., -5., nan, inf, 1e-300, 1e30, 1e200};
  const double ppqs[] = {0., -1., 1234.5, nan, inf, 1e300};
  int scenarios = 0, failures = 0;
  double worstStd = 1e9, minDuty = 1, maxDuty = 0;
  for (double sr : {44100., 48000.})
    for (bool running : {false, true})
      for (bool legacy : {false, true})
        for (double tempo : tempos)
          for (double ppq : ppqs)
            for (int note : {48, 72})
              for (int vel : {20, 64, 110})
              {
                const Outcome o = Play(params, sr, note, vel, running, tempo, ppq, legacy);
                ++scenarios;
                worstStd = std::min(worstStd, o.std);
                minDuty = std::min(minDuty, o.duty);
                maxDuty = std::max(maxDuty, o.duty);
                // a 50 % square wave centred on zero: clearly there, balanced, finite
                if (!o.finite || o.std < 0.1 || o.duty < 0.4 || o.duty > 0.6)
                {
                  if (++failures <= 8)
                    std::printf("FAIL: sr=%.0f running=%d legacy=%d tempo=%g ppq=%g note=%d vel=%d  std=%.3f duty=%.2f finite=%d\n", sr, running, legacy, tempo, ppq, note, vel, o.std, o.duty, o.finite);
                }
              }
  done = true;
  watchdog.join();
  std::printf("%d scenarios, %d failures. Lowest signal level (std) %.3f, share of positive samples %.2f-%.2f (50 %% square = 0.50)\n", scenarios, failures, worstStd, minDuty, maxDuty);
  return failures == 0 ? 0 : 1;
}
