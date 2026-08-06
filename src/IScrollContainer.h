#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControl.h"
#include <algorithm>

using namespace iplug;
using namespace igraphics;

class IScrollContainer : public IControl
{
private:
  IMultiLineTextControl* mChild = nullptr;
  float mScrollY = 0.0f;
  float mVirtualHeight = 0.0f;

public:
  IScrollContainer(const IRECT& bounds, float virtualHeight)
    : IControl(bounds)
    , mVirtualHeight(virtualHeight)
  {
  }

  void AddChildControl(IMultiLineTextControl* pChild) { mChild = pChild; }

  // FIX: Force both the child text data AND the parent container to refresh together
  void UpdateChildText(const char* text)
  {
    if (mChild)
    {
      mChild->SetStr(text);
      mChild->SetDirty(false); // Flags child internally
    }
    this->SetDirty(true); // Forces the parent window to re-render on the main canvas!
  }

  void Draw(IGraphics& g) override
  {
    if (!mChild)
      return;

    g.PathClipRegion(mRECT);

    IRECT childRect = IRECT(mRECT.L, mRECT.T + mScrollY, mRECT.R, mRECT.T + mScrollY + mVirtualHeight);
    mChild->SetTargetRECT(childRect);
    mChild->SetRECT(childRect);

    mChild->Draw(g);

    g.PathClipRegion(IRECT());
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    float maxScroll = std::min(0.0f, mRECT.H() - mVirtualHeight);
    mScrollY += (d * 20.0f);
    mScrollY = std::max(maxScroll, std::min(0.0f, mScrollY));

    SetDirty(false);
  }
};
