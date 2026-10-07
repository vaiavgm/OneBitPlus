#pragma once
#include "IControl.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <vector>

using namespace iplug;
using namespace igraphics;

struct SpectrogramColumn
{
  std::vector<float> magnitudes;
};

inline IColor GetHeatMapColor(float power)
{
  power = std::clamp(power, 0.0f, 1.0f);
  float r = 0.0f, g = 0.0f, b = 0.0f;

  if (power < 0.25f)
  {
    b = power / 0.25f;
  }
  else if (power < 0.50f)
  {
    float norm = (power - 0.25f) / 0.25f;
    r = norm;
    g = norm;
    b = 1.0f - norm;
  }
  else if (power < 0.75f)
  {
    float norm = (power - 0.50f) / 0.25f;
    r = 1.0f;
    g = 1.0f - norm;
    b = 0.0f;
  }
  else
  {
    float norm = (power - 0.75f) / 0.25f;
    r = 1.0f;
    g = norm;
    b = norm;
  }

  return IColor(255, (int)(r * 255.0f), (int)(g * 255.0f), (int)(b * 255.0f));
}

class DragDropWaveformDisplay : public IControl
{
public:
  DragDropWaveformDisplay(const IRECT& bounds, std::function<void(const char*)> dropCallback, std::function<void()> mouseDownCallback = nullptr, std::function<void()> mouseUpCallback = nullptr)
    : IControl(bounds)
    , mDropCallback(dropCallback)
    , mMouseDownCallback(mouseDownCallback)
    , mMouseUpCallback(mouseUpCallback)
  {
  }

  void OnDrop(const char* str) override
  {
    if (mDropCallback)
      mDropCallback(str);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mMouseDownCallback)
      mMouseDownCallback();
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if (mMouseUpCallback)
      mMouseUpCallback();
  }

  void SetFFTWindowSize(int newWindowSize)
  {
    mFFTSize = (int)std::pow(2, std::round(std::log2(newWindowSize)));
    mFFTSize = std::clamp(mFFTSize, 16, 8192);

    if (!mData.empty())
    {
      SetWaveformData(mData);
    }
  }

  void SetWaveformData(const std::vector<int8_t>& data)
  {
    mData = data;
    mEnvelope.clear();
    mSpectrogramMatrix.clear();

    if (mData.empty())
    {
      SetDirty(false);
      return;
    }

    // Only unpack the raw bits if AT LEAST one processing mode needs them
    if (!mShowEnvelope && !mShowSpectrogram)
    {
      SetDirty(true);
      return;
    }

    // --- Unpack bits to 1-bit float samples ---
    std::vector<float> rawSamples;
    int totalBits = (int)mData.size() * 8;
    rawSamples.reserve(totalBits);

    for (int i = 0; i < totalBits; ++i)
    {
      int byteIdx = i / 8;
      int bitIdx = 7 - (i % 8);
      int bit = ((uint8_t)mData[byteIdx] >> bitIdx) & 1;
      rawSamples.push_back(bit ? 1.0f : -1.0f);
    }

    // --- Conditional Waveform Envelope Data Calculation ---
    if (mShowEnvelope)
    {
      const int windowSize = 64;
      std::vector<float> smoothedSamples;
      smoothedSamples.reserve(totalBits);

      float currentSum = 0.0f;
      for (int i = 0; i < totalBits; ++i)
      {
        currentSum += rawSamples[i];
        if (i >= windowSize)
        {
          currentSum -= rawSamples[i - windowSize];
        }
        smoothedSamples.push_back(currentSum / std::min(i + 1, windowSize));
      }

      int numPixels = (int)mRECT.W();
      mEnvelope.resize(numPixels, {0.0f, 0.0f});
      float samplesPerPixel = (float)totalBits / (float)numPixels;

      for (int p = 0; p < numPixels; ++p)
      {
        int startIdx = (int)(p * samplesPerPixel);
        int endIdx = (int)((p + 1) * samplesPerPixel);
        endIdx = std::min(endIdx, totalBits);

        float minVal = 1.0f;
        float maxVal = -1.0f;

        for (int i = startIdx; i < endIdx; ++i)
        {
          float val = smoothedSamples[i];
          if (val < minVal)
            minVal = val;
          if (val > maxVal)
            maxVal = val;
        }

        mEnvelope[p].minAmp = minVal;
        mEnvelope[p].maxAmp = maxVal;
      }
    }

    // --- Conditional Static Spectrogram Matrix Calculation ---
    if (mShowSpectrogram)
    {
      const int numBins = mFFTSize / 2;

      // FIX: Force a 75% overlap (Hop size is 1/4 of the window size)
      // For a 2048 FFT, this calculates a fresh snapshot every 512 samples
      int hopSize = mFFTSize / 4;
      if (hopSize < 1)
        hopSize = 1;

      for (int startIdx = 0; startIdx + mFFTSize <= totalBits; startIdx += hopSize)
      {
        std::vector<std::complex<float>> fftBuffer(mFFTSize);

        // Use a Blackman-Harris or sharp Von Hann window to make harmonics narrower
        for (int i = 0; i < mFFTSize; ++i)
        {
          float hann = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (mFFTSize - 1)));
          fftBuffer[i] = std::complex<float>(rawSamples[startIdx + i] * hann, 0.0f);
        }

        ComputeFFT(fftBuffer);

        SpectrogramColumn column;
        column.magnitudes.resize(numBins);

        float maxMag = 0.0001f;
        for (int b = 1; b < numBins; ++b)
        {
          column.magnitudes[b] = std::abs(fftBuffer[b]);
          if (column.magnitudes[b] > maxMag)
            maxMag = column.magnitudes[b];
        }

        column.magnitudes[0] = std::abs(fftBuffer[0]);

        for (int b = 0; b < numBins; ++b)
        {
          column.magnitudes[b] /= maxMag;
        }

        mSpectrogramMatrix.push_back(column);

        // Safety cap to avoid infinite loops on giant files
        if (mSpectrogramMatrix.size() > 2000)
          break;
      }
    }
    SetDirty(true);
  }


  void RenderPixelatedSpectrogram(IGraphics& g, const IRECT& bounds, const std::vector<SpectrogramColumn>& spectrogramMatrix)
  {
    if (spectrogramMatrix.empty())
      return;

    const int viewW = (int)bounds.W();
    const int viewH = (int)bounds.H();
    const int numTimeSlices = (int)spectrogramMatrix.size();
    const int fftBins = (int)spectrogramMatrix[0].magnitudes.size();

    const float minFreq = 20.0f;
    const float maxFreq = 22050.0f;
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    for (int xPixel = 0; xPixel < viewW; ++xPixel)
    {
      float normX = (float)xPixel / (float)viewW;
      int t = std::clamp((int)(normX * numTimeSlices), 0, numTimeSlices - 1);

      float sliceLeft = bounds.L + (float)xPixel;
      float sliceRight = sliceLeft + 1.0f;

      for (int yPixel = 0; yPixel < viewH; ++yPixel)
      {
        float normY = (float)yPixel / (float)viewH;

        float targetLogFreq = logMin + (normY * (logMax - logMin));
        float targetFreq = std::pow(10.0f, targetLogFreq);

        float binWidthHz = 22050.0f / (float)fftBins;
        float preciseBinPosition = targetFreq / binWidthHz;

        float clmpBinIdx = std::clamp(preciseBinPosition, 0.0f, (float)(fftBins - 1));
        int binLower = (int)std::floor(clmpBinIdx);
        int binUpper = std::clamp(binLower + 1, 0, fftBins - 1);
        float fraction = clmpBinIdx - (float)binLower;

        float power = (1.0f - fraction) * spectrogramMatrix[t].magnitudes[binLower] + fraction * spectrogramMatrix[t].magnitudes[binUpper];

        // --- INTEGRATED TILT EQ FILTER ---
        // normY goes from 0.0 (20 Hz) to 1.0 (22 kHz).
        // We create a tilt slope that acts as a gradual gain multiplier for the upper half of the screen.
        // It leaves the bass completely raw (multiplier = 1.0) and scales up smoothly to 5.0x at the absolute top.
        float tiltPivot = 0.3f; // The vertical point where the boost starts taking effect (around 300Hz-500Hz)
        if (normY > tiltPivot)
        {
          // Calculate a smooth linear slope multiplier
          float tiltFactor = 4.0f; // Increase this to make overtones brighter, decrease to dim them
          float currentTiltGain = 1.0f + ((normY - tiltPivot) * tiltFactor);

          power *= currentTiltGain;
        }

        // Noise Floor Gate: Prevents the Tilt EQ from boosting tiny digital silence artifacts
        if (power < 0.01f)
        {
          power = 0.0f;
        }

        // Clamp the final value to make sure it doesn't overflow your color engine boundaries
        power = std::clamp(power, 0.0f, 1.0f);

        IColor pixelColor = GetHeatMapColor(power);

        float yCoord = bounds.B - (float)yPixel - 1.0f;
        IRECT pixelRect(sliceLeft, yCoord, sliceRight, yCoord + 1.0f);

        g.FillRect(pixelColor, pixelRect);
      }
    }
  }


  void Draw(IGraphics& g) override
  {
    g.FillRect(COLOR_DARK_GRAY, mRECT);

    // 1. Draw Spectrogram only if active and data exists
    if (mShowSpectrogram && !mSpectrogramMatrix.empty())
    {
      RenderPixelatedSpectrogram(g, mRECT, mSpectrogramMatrix);
    }

    g.DrawRect(COLOR_LIGHT_GRAY, mRECT);

    // 2. Draw Waveform Outline only if active
    if (mShowEnvelope && !mEnvelope.empty())
    {
      auto renderColor = COLOR_GREEN;
      renderColor.SetOpacity(0.5f);
      float centerY = mRECT.MH();
      int numPixels = (int)mEnvelope.size();
      float halfHeight = (mRECT.H() / 2.0f) - 4.0f;
      //float prevAvg = centerY;

      for (int p = 0; p < numPixels; ++p)
      {
        float x = mRECT.L + (float)p;
        float yTop = centerY - (mEnvelope[p].maxAmp * halfHeight);
        float yBottom = centerY - (mEnvelope[p].minAmp * halfHeight);

        //if (std::abs(yBottom - yTop) < 1.0f)
        //  yBottom = yTop + 1.0f;

        //float avg = (yTop + yBottom) / 2;
        //g.DrawLine(COLOR_GREEN, x - 1, prevAvg, x, avg);
        //prevAvg = avg;
        g.DrawLine(renderColor, x, yTop, x, yBottom);
      }

      g.DrawLine(COLOR_GRAY, mRECT.L, centerY, mRECT.R, centerY);
    }
  }

private:
  struct PixelEnvelope
  {
    float minAmp;
    float maxAmp;
  };

  // --- Toggle Optimization Flags ---
  bool mShowEnvelope = false;    // Flip to false to instantly stop Waveform processing/rendering
  bool mShowSpectrogram = true; // Flip to false to instantly stop heavy FFT processing/rendering

  std::vector<int8_t> mData;
  std::vector<PixelEnvelope> mEnvelope;
  std::vector<SpectrogramColumn> mSpectrogramMatrix;
  int mFFTSize = 2048;

  std::function<void(const char*)> mDropCallback;
  std::function<void()> mMouseDownCallback;
  std::function<void()> mMouseUpCallback;

  void ComputeFFT(std::vector<std::complex<float>>& data)
  {
    size_t n = data.size();
    if (n <= 1)
      return;

    for (size_t i = 1, j = 0; i < n; ++i)
    {
      size_t bit = n >> 1;
      for (; j & bit; bit >>= 1)
        j ^= bit;
      j ^= bit;
      if (i < j)
        std::swap(data[i], data[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
      float angle = -2.0f * 3.14159265f / len;
      std::complex<float> wlen(std::cos(angle), std::sin(angle));
      for (size_t i = 0; i < n; i += len)
      {
        std::complex<float> w(1.0f, 0.0f);
        for (size_t j = 0; j < len / 2; ++j)
        {
          std::complex<float> u = data[i + j];
          std::complex<float> v = data[i + j + len / 2] * w;
          data[i + j] = u + v;
          data[i + j + len / 2] = u - v;
          w *= wlen;
        }
      }
    }
  }
};
