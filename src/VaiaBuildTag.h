#pragma once

// Dev-build marker. Compile-time only: no runtime cost, nothing is checked at startup.
//
// VAIA_DEV_BUILD is defined via EXTRA_ALL_DEFS in config/<Name>-win.props unless the MSBuild
// property / environment variable VAIA_RELEASE is set (use that for release/installer builds:
//   msbuild ... /p:VAIA_RELEASE=1   or   set VAIA_RELEASE=1).
// __DATE__ / __TIME__ are evaluated when the .cpp that includes this header is compiled.
//
//   VAIA_BUILD_TAG                  "DEV Oct  7 2026 19:03:12" (empty string in release builds)
//   VAIA_BUILD_SUFFIX               "  |  DEV ..." to append to an existing version label (empty in release)
//   VAIA_ATTACH_BUILD_TAG(g)        red label in the bottom-left corner; call last so it is on top
#ifdef VAIA_DEV_BUILD
#include <algorithm>
#include "IControls.h"
#define VAIA_BUILD_TAG "DEV " __DATE__ " " __TIME__
#define VAIA_BUILD_SUFFIX "  |  " VAIA_BUILD_TAG
#define VAIA_ATTACH_BUILD_TAG(pGraphics) \
  do { \
    auto* pVaiaTag = new iplug::igraphics::ITextControl( \
      (pGraphics)->GetBounds().GetFromBottom(12.f).GetFromLeft(std::min(190.f, (pGraphics)->GetBounds().W())), \
      VAIA_BUILD_TAG, \
      iplug::igraphics::IText(10.f, iplug::igraphics::IColor(255, 255, 70, 70), nullptr, iplug::igraphics::EAlign::Near, iplug::igraphics::EVAlign::Middle)); \
    pVaiaTag->SetIgnoreMouse(true); \
    (pGraphics)->AttachControl(pVaiaTag); \
  } while (0)
#else
#define VAIA_BUILD_TAG ""
#define VAIA_BUILD_SUFFIX ""
#define VAIA_ATTACH_BUILD_TAG(pGraphics) ((void)0)
#endif
