// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>

// Self-contained public surface for the adaptive-strength blend. It lives in
// the core `oidn` namespace and does not pull in the generated config.h /
// ISPC build, so it can be unit-tested with a plain host compiler.
namespace oidn {

  // Forward declaration; the full half type is defined in common/half.h and is
  // already visible wherever this translation unit is built into the library.
  class half;

  // Adaptive denoise strength control.
  //
  // A fixed, global denoise strength cannot cope with the non-uniform noise
  // distribution of Monte Carlo renderings: dark regions are low-SNR and get
  // over-blurred into a flat smear, while bright (high-SNR) regions still
  // retain visible grain. Instead of increasing the render sample count, the
  // denoised network output is blended per-pixel with the original noisy input:
  //
  //   result = lerp(input, denoised, strength(x))
  //
  // where strength(x) in [0,1] is 0 = keep the untouched noisy input (no
  // denoising) and 1 = fully accept the denoised network output. The strength
  // is driven either by an automatically estimated luminance map or by a
  // user-provided per-pixel strength map, so that detail in dark areas is
  // preserved while residual grain in bright areas is still suppressed.
  struct AdaptiveDenoiseParams
  {
    // Base strength applied to every pixel before the luminance modulation:
    // 0 = never denoise, 1 = fixed/full strength (legacy behaviour).
    float strengthMin = 0.35f;
    float strengthMax = 1.00f;

    // Luminance (in the working/linear scale) between which the strength
    // ramps from strengthMin to strengthMax. Below shadowLum the darkest
    // pixels keep the most original detail; above highlightLum the brightest
    // pixels receive the strongest denoising.
    float shadowLum    = 0.05f;
    float highlightLum = 2.00f;

    // Exponent of the smoothstep ramp. 1 = linear interpolation, >1 shifts the
    // transition toward the highlights (denoise a larger share of the image).
    float exponent     = 2.0f;

    bool validate() const;
  };

  // Computes the Rec.709 luminance of an RGB pixel.
  float adaptiveDenoiseLuminance(float r, float g, float b);

  // Maps a single luminance value to a denoise strength in [0,1]. Exposed so
  // callers can reason about or precompute the response curve.
  float adaptiveDenoiseStrength(float luminance, const AdaptiveDenoiseParams& params);

  // Blends the noisy input and the denoised output per pixel on the host.
  //
  // Both images must have identical width/height/channel count and interleaved
  // channel layout. Supported data types are float32 (elementSize == 4) and
  // float16 (elementSize == 2, OIDN's native half). The result is written in
  // place into dst (which normally holds the denoised output). If strengthMap
  // is non-null it must contain width*height float32 values in [0,1] and
  // overrides the automatic luminance-based strength.
  void adaptiveDenoiseBlend(void* dst, const void* src,
                            size_t width, size_t height, size_t channels,
                            size_t elementSize,
                            const AdaptiveDenoiseParams& params,
                            const float* strengthMap = nullptr);

} // namespace oidn
