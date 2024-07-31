#pragma once

#include "Oscillator.h"
#include <cmath>

#define MY_PRINTF(...) {char buf[512]; sprintf(buf, __VA_ARGS__);  OutputDebugString(buf);}

// #include <windows.h>
// char debugBuf[32];
// 
// sprintf(debugBuf, "S#%06f: %02.8f\n", std::sin(IOscillator<T>::mPhase * PI * 2.), IOscillator<T>::mPhase);
// OutputDebugString(debugBuf);

using namespace iplug;


template <typename T>
class VaiaOscillator : public IOscillator<T>
{

public:
  VaiaOscillator(double startPhase = 0., double startFreq = 1.)
    : IOscillator<T>(startPhase, startFreq)
  {
  }

  void SetPWM(double pwm) {
     m_pwm = pwm;
  }

  inline T Process(double freqHz) override
  {
    IOscillator<T>::SetFreqCPS(freqHz);
    IOscillator<T>::mPhase = IOscillator<T>::mPhase + IOscillator<T>::mPhaseIncr;

    while (mPhase > 1.0) {
      mPhase -= 1.0;
    }

    bool phaseFlip = false;

    while (m_pwm > 1) {
      m_pwm--;
      phaseFlip = true;
    }
    while (m_pwm < 0) {
      m_pwm++;
      phaseFlip = true;
    }

    if (m_pwm == 0) return 0.0;

    int pos = mPhase > m_pwm ? 0 : 1;

     // m_pwm+=0.000005f; // "phase" the pulse
    

    int temp = squareLookup[pos % 2];

    return temp;

  }
private:
  double squareLookup[2] = { -1.0, 1.0 };
  double m_pwm = 0.5;
};
