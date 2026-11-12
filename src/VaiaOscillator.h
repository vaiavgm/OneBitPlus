#pragma once

// iPlug2's Oscillator.h uses std::sin and PI without including what defines them: provide both first (newer MSVC
// standard libraries no longer include <cmath> implicitly).
#include <cmath>
#include <IPlugConstants.h>
#include "Oscillator.h"
#include <cmath>





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

    while (IOscillator<T>::mPhase > 1.0) {
      IOscillator<T>::mPhase -= 1.0;
    }


    // Wrap the pulse width into [0, 1] without looping (the key-tracked / modulated width can be far outside).
    // Same result as subtracting 1 while > 1 and adding 1 while < 0: exactly 1.0 stays 1.0 (100 % = constant high).
    if (m_pwm > 1.0)
      m_pwm -= std::ceil(m_pwm) - 1.0;
    else if (m_pwm < 0.0)
      m_pwm += std::ceil(-m_pwm);

    if (m_pwm == 0) return 0.0;

    int pos = IOscillator<T>::mPhase > m_pwm ? 0 : 1;

    int temp = squareLookup[pos % 2];


    return temp;

  }
private:
  double squareLookup[2] = { -1.0, 1.0 };
  double m_pwm = 0.5;
};
