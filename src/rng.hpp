#pragma once
#include <cstdint>

struct XorShift32 {
  std::uint32_t s = 0x12345678u;

  constexpr explicit XorShift32(std::uint32_t seed = 0x12345678u)
      : s(seed ? seed : 0x12345678u) {}

  constexpr std::uint32_t next_u32() {
    std::uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
  }

  constexpr int next_int(int lo, int hi) {
    const std::uint32_t r = next_u32();
    const std::uint32_t span = static_cast<std::uint32_t>(hi - lo + 1);
    return lo + static_cast<int>(r % span);
  }

  constexpr bool coin() { return (next_u32() & 1u) != 0; }
};