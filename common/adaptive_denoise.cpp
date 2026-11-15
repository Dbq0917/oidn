// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "adaptive_denoise.h"

#include <algorithm>
#include <cmath>

#if defined(OIDN_ADAPTIVE_TEST_STUB)
  #include "stub_half.h"
#else
  #include "half.h" // full definition of oidn::half (library build)
#endif

namespace oidn {

  bool AdaptiveDenoiseParams::validate() const
  {
    return std::isfinite(strengthMin) && std::isfinite(strengthMax) &&
           std::isfinite(shadowLum) && std::isfinite(highlightLum) &&
           std::isfinite(exponent) &&
           strengthMin >= 0.f && strengthMin <= 1.f &&
           strengthMax >= 0.f && strengthMax <= 1.f &&
           strengthMin <= strengthMax &&
           shadowLum >= 0.f && highlightLum > shadowLum &&
           exponent > 0.f;
  }

  float adaptiveDenoiseLuminance(float r, float g, float b)
  {
    // Rec.709 luma coefficients, matching the luminance() helper used by the
    // autoexposure pass.
    return 0.212671f * r + 0.715160f * g + 0.072169f * b;
  }

  float adaptiveDenoiseStrength(float luminance, const AdaptiveDenoiseParams& params)
  {
    // Normalize the luminance into the [0,1] shadow -> highlight range.
    float t = (luminance - params.shadowLum) /
              (params.highlightLum - params.shadowLum);
    t = std::clamp(t, 0.f, 1.f);

    // Smooth, C1-continuous monotonic ramp with an adjustable exponent. The
    // exponent biases where the transition happens without introducing a hard
    // threshold that would produce a visible band.
    const float e = params.exponent;
    float s = t * t * (e + 1.f - e * t); // generalized smoothstep

    return params.strengthMin + (params.strengthMax - params.strengthMin) * s;
  }

  namespace
  {
    inline float loadElement(const void* data, size_t index, size_t elementSize)
    {
      if (elementSize == 4)
        return static_cast<const float*>(data)[index];
      else
        return static_cast<float>(static_cast<const half*>(data)[index]);
    }

    inline void storeElement(void* data, size_t index, size_t elementSize, float value)
    {
      if (elementSize == 4)
        static_cast<float*>(data)[index] = value;
      else
        static_cast<half*>(data)[index] = half(value);
    }
  }

  void adaptiveDenoiseBlend(void* dst, const void* src,
                            size_t width, size_t height, size_t channels,
                            size_t elementSize,
                            const AdaptiveDenoiseParams& params,
                            const float* strengthMap)
  {
    if (!dst || !src || width == 0 || height == 0 || channels == 0)
      return;
    if (elementSize != 4 && elementSize != 2)
      return;

    const size_t pixelCount = width * height;

    for (size_t p = 0; p < pixelCount; ++p)
    {
      const size_t base = p * channels;

      float strength;
      if (strengthMap)
      {
        strength = std::clamp(strengthMap[p], 0.f, 1.f);
      }
      else
      {
        // Drive the strength from the (sanitized) luminance of the denoised
        // output, which is the stable estimate of the true signal and does
        // not fluctuate with the per-pixel noise of the raw input.
        float r = loadElement(dst, base + 0, elementSize);
        float g = channels > 1 ? loadElement(dst, base + 1, elementSize) : r;
        float b = channels > 2 ? loadElement(dst, base + 2, elementSize) : g;
        r = std::isfinite(r) ? std::max(r, 0.f) : 0.f;
        g = std::isfinite(g) ? std::max(g, 0.f) : 0.f;
        b = std::isfinite(b) ? std::max(b, 0.f) : 0.f;
        strength = adaptiveDenoiseStrength(adaptiveDenoiseLuminance(r, g, b), params);
      }

      // Blend in linear radiance. Blending after the inverse transfer function
      // (i.e. on the values actually stored) preserves energy correctly for
      // both LDR and HDR images.
      for (size_t c = 0; c < channels; ++c)
      {
        const size_t idx = base + c;
        const float noisy   = loadElement(src, idx, elementSize);
        const float denoised = loadElement(dst, idx, elementSize);
        const float blended = noisy + (denoised - noisy) * strength;
        storeElement(dst, idx, elementSize, blended);
      }
    }
  }

} // namespace oidn
