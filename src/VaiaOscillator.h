#pragma once

#include "Oscillator.h"
#include <cmath>

#include <format>
#include <windows.h>




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


    while (m_pwm > 1) {
      m_pwm--;
    }
    while (m_pwm < 0) {
      m_pwm++;
    }

    if (m_pwm == 0) return 0.0;

    int pos = mPhase > m_pwm ? 0 : 1;

    int temp = squareLookup[pos % 2];


    return temp;

  }
private:
  double squareLookup[2] = { -1.0, 1.0 };
  double m_pwm = 0.5;
};
