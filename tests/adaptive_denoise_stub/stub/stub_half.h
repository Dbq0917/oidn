// Test-only stub of common/half.h.
#pragma once

#include <cstdint>
#include <cstring>

namespace oidn {

  inline uint32_t test_half_to_float_bits(int16_t h)
  {
    uint32_t s = (uint32_t(uint16_t(h)) & 0x8000) << 16;
    uint32_t e = (uint32_t(uint16_t(h)) >> 10) & 0x1f;
    uint32_t m = uint32_t(uint16_t(h)) & 0x3ff;
    if (e == 0)
    {
      if (m == 0) return s;
      e = 127 - 15 + 1;
      while ((m & 0x400) == 0) { m <<= 1; --e; }
      m &= 0x3ff;
      return s | (e << 23) | (m << 13);
    }
    if (e == 31)
      return s | 0x7f800000 | (m << 13);
    return s | ((e + (127 - 15)) << 23) | (m << 13);
  }

  inline int16_t test_float_to_half_bits(uint32_t f)
  {
    uint32_t s = (f >> 16) & 0x8000;
    int32_t e = int32_t((f >> 23) & 0xff) - 127 + 15;
    uint32_t m = f & 0x7fffff;
    if (e <= 0)
    {
      if (e < -10) return int16_t(s);
      m |= 0x800000;
      uint32_t shift = uint32_t(14 - e);
      uint32_t r = m >> shift;
      if ((m & ((1u << shift) - 1)) > (1u << (shift - 1))) ++r;
      return int16_t(s | r);
    }
    if (e == 0xff - (127 - 15))
      return int16_t(s | 0x7c00);
    uint32_t r = m >> 13;
    if ((m & 0x1fff) > 0x1000) { ++r; if (r == 0x400) { r = 0; ++e; } }
    return int16_t(s | (uint32_t(e) << 10) | r);
  }

  class half
  {
  public:
    half() = default;
    half(float f) { *this = f; }
    half& operator =(float f)
    {
      uint32_t u;
      std::memcpy(&u, &f, sizeof(u));
      x = test_float_to_half_bits(u);
      return *this;
    }
    operator float() const
    {
      uint32_t u = test_half_to_float_bits(x);
      float f;
      std::memcpy(&f, &u, sizeof(f));
      return f;
    }

    int16_t x = 0;
  };

} // namespace oidn
