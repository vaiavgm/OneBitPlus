#pragma once
#include <vector>
#include <memory>
#include <random>
#include "src/TrellisQuantizer.h"
#include "src/DeltaSigmaQuantizer.h"

#define MY_PRINTF(...)                                                                                                                                                                                 \
  {                                                                                                                                                                                                    \
    char buf[512];                                                                                                                                                                                     \
    sprintf(buf, __VA_ARGS__);                                                                                                                                                                         \
    OutputDebugString(buf);                                                                                                                                                                            \
  }

// --- Abstract Base Class for Pipeline Effects ---
class IAudioEffect {
public:
    virtual ~IAudioEffect() = default;
    
    // Process audio buffer in-place (normalized [-1.0, 1.0] range)
    virtual void Process(std::vector<double>& buffer, uint32_t sampleRate) = 0;
};





////////////////////////
// Preprocessing Effects
////////////////////////



class GainEffect : public IAudioEffect
{
private:
  double m_gain; // Linear gain multiplier (1.0 = unity, 2.0 = +6dB, 0.5 = -6dB)

public:
  // Linear gain multiplier (1.0 = unity, 2.0 = +6dB, 0.5 = -6dB)
  explicit GainEffect(double gain = 1.0)
    : m_gain(gain)
  {
  }

  // Optional helper constructor for decibels if you prefer working in dB
  static GainEffect FromDecibels(double dB) { return GainEffect(std::pow(10.0, dB / 20.0)); }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty())
      return;

    for (double& sample : buffer)
    {
      // 1. Apply gain boost or attenuation
      sample *= m_gain;

      // 2. Hard clip digitally if it hits the [-1.0, 1.0] ceiling
      sample = std::clamp(sample, -1.0, 1.0);
    }
  }
};



// --- 1. Normalization Effect ---
class NormalizeEffect : public IAudioEffect
{
public:
  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty())
      return;

    double maxVal = 0.0;
    for (double s : buffer)
    {
      maxVal = std::max(maxVal, std::abs(s));
    }

    if (maxVal > 0.0)
    {
      double scale = 1.0 / maxVal;
      for (double& s : buffer)
      {
        s *= scale;
      }
    }
  }
};



// --- 2. Low-Pass Filter Effect ---
class LowPassFilterEffect : public IAudioEffect
{
private:
  double m_cutoffFreq;

public:
  explicit LowPassFilterEffect(double cutoffFreq = 16000.0)
    : m_cutoffFreq(cutoffFreq)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty() || m_cutoffFreq <= 0.0 || sampleRate <= 0.0)
      return;

    const double pi = 3.14159265358979323846;
    double rc = 1.0 / (2.0 * pi * m_cutoffFreq);
    double dt = 1.0 / sampleRate;
    double alpha = dt / (rc + dt);

    // Initialize filter memory with the first floating-point sample
    double prevOutput = buffer[0];

    for (double& val : buffer)
    {
      prevOutput = prevOutput + alpha * (val - prevOutput);
      val = prevOutput;
    }
  }
};


class HighPassFilterEffect : public IAudioEffect
{
private:
  double m_cutoffFreq;

public:
  explicit HighPassFilterEffect(double cutoffFreq = 80.0) // Default cutoff to remove DC offset / sub-rumble
    : m_cutoffFreq(cutoffFreq)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty() || m_cutoffFreq <= 0.0 || sampleRate <= 0.0)
      return;

    const double pi = 3.14159265358979323846;
    double rc = 1.0 / (2.0 * pi * m_cutoffFreq);
    double dt = 1.0 / sampleRate;
    double alpha = dt / (rc + dt);

    // Initialize the internal LPF state with the first sample
    double lpfOutput = buffer[0];

    for (double& val : buffer)
    {
      double input = val;

      // Compute 1st-order LPF state
      lpfOutput = lpfOutput + alpha * (input - lpfOutput);

      // Subtract LPF result from original input to extract high frequencies
      val = input - lpfOutput;
    }
  }
};

class BiquadFilterEffect : public IAudioEffect
{
public:
  enum class FilterType
  {
    LowPass,
    HighPass
  };

private:
  FilterType m_type;
  double m_cutoffFreq;
  double m_Q;       // Quality factor (0.7071 = Butterworth / maximally flat)
  uint32_t m_order; // Filter order (must be even: 2, 4, 6, 8...)

  // Coefficients for a single biquad stage
  struct BiquadCoeffs
  {
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
  };

  // State memory for a single biquad stage (Direct Form I)
  struct BiquadState
  {
    double x1 = 0.0, x2 = 0.0; // Past inputs
    double y1 = 0.0, y2 = 0.0; // Past outputs
  };

  BiquadCoeffs CalculateCoefficients(double sampleRate) const
  {
    const double pi = 3.14159265358979323846;
    double omega = 2.0 * pi * m_cutoffFreq / sampleRate;
    double cosW = std::cos(omega);
    double sinW = std::sin(omega);
    double alpha = sinW / (2.0 * m_Q);

    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosW;
    double a2 = 1.0 - alpha;

    if (m_type == FilterType::LowPass)
    {
      b0 = (1.0 - cosW) / 2.0;
      b1 = 1.0 - cosW;
      b2 = (1.0 - cosW) / 2.0;
    }
    else
    { // HighPass
      b0 = (1.0 + cosW) / 2.0;
      b1 = -(1.0 + cosW);
      b2 = (1.0 + cosW) / 2.0;
    }

    // Normalize coefficients by a0
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
  }

public:
  explicit BiquadFilterEffect(FilterType type,
                              double cutoffFreq,
                              uint32_t order = 2,
                              double Q = 0.7071067811865475) // 1 / sqrt(2)
    : m_type(type)
    , m_cutoffFreq(cutoffFreq)
    , m_Q(Q)
    , m_order(std::max<uint32_t>(2, (order / 2) * 2)) // Force even order
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty() || m_cutoffFreq <= 0.0 || sampleRate <= 0.0)
      return;

    // 1. Calculate Biquad Coefficients for current sample rate
    BiquadCoeffs coeffs = CalculateCoefficients(static_cast<double>(sampleRate));

    // 2. Prepare state memory for cascaded stages (order / 2 biquads)
    size_t numStages = m_order / 2;
    std::vector<BiquadState> stages(numStages);

    // 3. Process buffer through cascaded stages in series
    for (double& sample : buffer)
    {
      double stageInput = sample;

      for (size_t s = 0; s < numStages; ++s)
      {
        BiquadState& st = stages[s];

        // Direct Form I difference equation
        double stageOutput = coeffs.b0 * stageInput + coeffs.b1 * st.x1 + coeffs.b2 * st.x2 - coeffs.a1 * st.y1 - coeffs.a2 * st.y2;

        // Shift state registers
        st.x2 = st.x1;
        st.x1 = stageInput;
        st.y2 = st.y1;
        st.y1 = stageOutput;

        // Feed output of current stage as input into the next stage
        stageInput = stageOutput;
      }

      sample = stageInput;
    }
  }
};


// --- 3. Saturation Effect ---
class SaturateEffect : public IAudioEffect
{
private:
  double m_drive;

public:
  explicit SaturateEffect(double drive = 2.0)
    : m_drive(drive)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty())
      return;

    for (double& sample : buffer)
    {
      // Sample is already in floating-point [-1.0, 1.0]
      sample = std::tanh(sample * m_drive);
    }
  }
};

class ClippingEffect : public IAudioEffect
{
private:
  double m_threshold;    // Clipping threshold [0.0, 1.0]
  bool m_normalizeAfter; // Whether to normalize the buffer after clipping

public:
  explicit ClippingEffect(double threshold = 1.0, bool normalizeAfter = false)
    : m_threshold(std::clamp(threshold, 0.0, 1.0))
    , m_normalizeAfter(normalizeAfter)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty())
      return;

    // 1. Apply Hard Clipping based on threshold
    for (double& sample : buffer)
    {
      sample = std::clamp(sample, -m_threshold, m_threshold);
    }

    // 2. Optionally chain a normalization pass right after
    if (m_normalizeAfter)
    {
      double maxVal = 0.0;
      for (double s : buffer)
      {
        maxVal = std::max(maxVal, std::abs(s));
      }

      if (maxVal > 0.0)
      {
        double scale = 1.0 / maxVal;
        for (double& s : buffer)
        {
          s *= scale;
        }
      }
    }
  }
};

class SampleRateReductionEffect : public IAudioEffect
{
private:
  double m_targetSampleRate; // Target lo-fi sample rate (e.g., 8000.0 Hz)

public:
  explicit SampleRateReductionEffect(double targetSampleRate = 8000.0)
    : m_targetSampleRate(targetSampleRate)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty() || sampleRate == 0 || m_targetSampleRate <= 0.0)
      return;

    double hostRate = static_cast<double>(sampleRate);

    // If target rate is equal to or greater than the host rate, do nothing
    if (m_targetSampleRate >= hostRate)
      return;

    double phase = 1.0; // Start at 1.0 to capture the very first sample
    double phaseInc = m_targetSampleRate / hostRate;
    double heldSample = buffer[0];

    for (double& s : buffer)
    {
      phase += phaseInc;
      if (phase >= 1.0)
      {
        phase -= 1.0;
        heldSample = s; // Latch a new sample
      }

      // Output the latched sample
      s = heldSample;
    }
  }
};

class DitherEffect : public IAudioEffect
{
private:
  // Quantization step size for 1-bit or low-bit depth
  // (For 1-bit normalized [-1.0, 1.0], scale matches output resolution)
  double m_ditherScale;
  mutable std::mt19937 m_rng;
  mutable std::uniform_real_distribution<double> m_dist;

public:
  // recommended: e.g. 0.02 (2%)
  explicit DitherEffect(double scale = 1.0 / 32768.0)
    : m_ditherScale(scale)
    , m_rng(1337) // Seed for determinism or use std::random_device{}
    , m_dist(-0.5, 0.5)
  {
  }

  void Process(std::vector<double>& buffer, uint32_t sampleRate) override
  {
    if (buffer.empty())
      return;

    // TPDF Dither = sum of two uniform random variables (triangular distribution)
    for (double& sample : buffer)
    {
      double noise = (m_dist(m_rng) + m_dist(m_rng)) * m_ditherScale;
      sample += noise;
    }
  }
};


struct AudioPipeline
{
  std::vector<std::shared_ptr<IAudioEffect>> effects; // 0 to N pre-processing effects
  std::shared_ptr<IQuantizer> quantizer = std::make_shared<DeltaSigmaQuantizer>(); // Quantizer, defaults to Delta-Sigma

  // Fluent builder helpers
  AudioPipeline& AddEffect(std::shared_ptr<IAudioEffect> effect)
  {
    if (effect)
      effects.push_back(effect);
    return *this;
  }

  AudioPipeline& SetQuantizer(std::shared_ptr<IQuantizer> q)
  {
    quantizer = q;
    return *this;
  }
};

// Default Pipeline Factory: Normalize -> Gentle Saturation -> Delta-Sigma
inline static AudioPipeline CreateDefaultPipeline()
{
  AudioPipeline pipeline;
  pipeline.effects.push_back(std::make_shared<NormalizeEffect>());
  pipeline.effects.push_back(std::make_shared<SaturateEffect>(2.0));
  pipeline.quantizer = std::make_shared<TrellisQuantizer>();
  return pipeline;
}

