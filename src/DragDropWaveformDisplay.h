#pragma once
#include "IControl.h"
#include <algorithm>
#include <functional>
#include <vector>

using namespace iplug;
using namespace igraphics;

class DragDropWaveformDisplay : public IControl
{
public:
  // ADDED: clickCallback parameter (defaults to nullptr so it's optional)
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

  void SetWaveformData(const std::vector<int8_t>& data)
  {
    mData = data;
    mEnvelope.clear();

    if (mData.empty())
    {
      SetDirty(false);
      return;
    }

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

    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(COLOR_DARK_GRAY, mRECT);
    g.DrawRect(COLOR_LIGHT_GRAY, mRECT);

    if (mEnvelope.empty())
      return;

    float centerY = mRECT.MH();
    g.DrawLine(COLOR_GRAY, mRECT.L, centerY, mRECT.R, centerY);

    int numPixels = (int)mEnvelope.size();
    float halfHeight = (mRECT.H() / 2.0f) - 4.0f;

    for (int p = 0; p < numPixels; ++p)
    {
      float x = mRECT.L + (float)p;

      float yTop = centerY - (mEnvelope[p].maxAmp * halfHeight);
      float yBottom = centerY - (mEnvelope[p].minAmp * halfHeight);

      if (std::abs(yBottom - yTop) < 1.0f)
      {
        yBottom = yTop + 1.0f;
      }

      g.DrawLine(COLOR_WHITE, x, yTop, x, yBottom);
    }
  }

private:
  struct PixelEnvelope
  {
    float minAmp;
    float maxAmp;
  };

  std::vector<int8_t> mData;
  std::vector<PixelEnvelope> mEnvelope;
  std::function<void(const char*)> mDropCallback;
  std::function<void()> mMouseDownCallback;
  std::function<void()> mMouseUpCallback;
};