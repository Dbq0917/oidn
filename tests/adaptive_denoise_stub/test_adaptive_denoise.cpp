// Standalone unit tests for common/adaptive_denoise.cpp.
//
// This file is intentionally independent of the OIDN CMake/ISPC build: it
// supplies minimal stub headers (see ./stub) and a local fp16 conversion so
// the adaptive-strength math and the f32/f16 blending paths can be verified
// with a plain C++17 compiler.

#include "stub_half.h" // test stub half type
#include "adaptive_denoise.h"
#include "common.h" // test stub

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace oidn;

static bool approx(float a, float b, float eps = 1e-5f)
{
  return std::fabs(a - b) <= eps * (1.f + std::fabs(b));
}

int main()
{
  // --- parameter validation ------------------------------------------------
  AdaptiveDenoiseParams good;
  assert(good.validate());

  AdaptiveDenoiseParams bad = good;
  bad.strengthMin = 1.2f;
  assert(!bad.validate());
  bad = good; bad.strengthMax = 0.1f; // max < min
  assert(!bad.validate());
  bad = good; bad.highlightLum = bad.shadowLum; // non-increasing range
  assert(!bad.validate());
  bad = good; bad.exponent = 0.f;
  assert(!bad.validate());

  // --- strength curve endpoints and monotonicity ---------------------------
  AdaptiveDenoiseParams p;
  assert(approx(adaptiveDenoiseStrength(0.0f, p), p.strengthMin));
  assert(approx(adaptiveDenoiseStrength(p.shadowLum, p), p.strengthMin));
  assert(approx(adaptiveDenoiseStrength(p.highlightLum, p), p.strengthMax));
  assert(approx(adaptiveDenoiseStrength(100.f, p), p.strengthMax));

  float prev = -1.f;
  for (int i = 0; i <= 100; ++i)
  {
    float lum = p.shadowLum + (p.highlightLum - p.shadowLum) * (i / 100.f);
    float s = adaptiveDenoiseStrength(lum, p);
    assert(s >= p.strengthMin - 1e-6f && s <= p.strengthMax + 1e-6f);
    assert(s >= prev - 1e-6f);
    prev = s;
  }
  // Midpoint of a generalized smoothstep (exponent 2 -> classic smoothstep)
  // is exactly 0.5 of the strength range.
  float midLum = 0.5f * (p.shadowLum + p.highlightLum);
  assert(approx(adaptiveDenoiseStrength(midLum, p),
                0.5f * (p.strengthMin + p.strengthMax), 1e-5f));

  // --- f32 blend: dark pixel keeps detail, bright pixel fully denoised -----
  const size_t W = 2, H = 1, C = 3;
  std::vector<float> noisy = {
    0.010f, 0.020f, 0.030f, // dark pixel (below shadowLum)
    5.000f, 5.000f, 5.000f  // bright pixel (above highlightLum)
  };
  std::vector<float> denoised = {
    0.012f, 0.022f, 0.032f, // network over-smoothed the dark detail
    5.100f, 5.100f, 5.100f
  };
  std::vector<float> dst = denoised;
  adaptiveDenoiseBlend(dst.data(), noisy.data(), W, H, C, 4, p);

  // Dark: strength == strengthMin => mostly the original noisy input.
  for (int c = 0; c < 3; ++c)
  {
    float expect = noisy[c] + (denoised[c] - noisy[c]) * p.strengthMin;
    assert(approx(dst[c], expect));
  }
  // Bright: strength == strengthMax (1) => fully the denoised value.
  for (int c = 3; c < 6; ++c)
    assert(approx(dst[c], denoised[c]));

  // --- explicit strength map overrides luminance ---------------------------
  std::vector<float> dst2 = denoised;
  const float map[2] = {0.0f, 1.0f};
  adaptiveDenoiseBlend(dst2.data(), noisy.data(), W, H, C, 4, p, map);
  for (int c = 0; c < 3; ++c)
    assert(approx(dst2[c], noisy[c])); // strength 0 keeps raw input
  for (int c = 3; c < 6; ++c)
    assert(approx(dst2[c], denoised[c])); // strength 1 keeps denoised

  // --- f16 path round-trips and blends correctly ---------------------------
  std::vector<half> hNoisy(6), hDst(6);
  for (int i = 0; i < 6; ++i)
  {
    hNoisy[i] = noisy[i];
    hDst[i]   = denoised[i];
  }
  adaptiveDenoiseBlend(hDst.data(), hNoisy.data(), W, H, C, 2, p);
  for (int c = 0; c < 3; ++c)
  {
    float expect = noisy[c] + (denoised[c] - noisy[c]) * p.strengthMin;
    assert(approx(float(hDst[c]), expect, 2e-3f));
  }
  for (int c = 3; c < 6; ++c)
    assert(approx(float(hDst[c]), denoised[c], 2e-3f));

  // --- single-channel (grayscale) does not over-read -----------------------
  std::vector<float> gNoisy = {0.01f, 5.0f};
  std::vector<float> gDst   = {0.02f, 5.2f};
  adaptiveDenoiseBlend(gDst.data(), gNoisy.data(), 2, 1, 1, 4, p);
  assert(approx(gDst[0], gNoisy[0] + (0.02f - gNoisy[0]) * p.strengthMin));
  assert(approx(gDst[1], 5.2f));

  // --- degenerate calls are no-ops -----------------------------------------
  adaptiveDenoiseBlend(nullptr, nullptr, 0, 0, 3, 4, p);
  adaptiveDenoiseBlend(dst.data(), noisy.data(), W, H, C, 3, p); // bad element size

  std::cout << "all adaptive denoise tests passed" << std::endl;
  return 0;
}
