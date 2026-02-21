#pragma once

#include <cstdint>

/**
 * @file rng.hpp
 * @brief Small deterministic pseudo-random number generator utilities.
 */

/**
 * @brief XorShift32 PRNG.
 *
 * Lightweight PRNG used by the simulation for randomized movement decisions.
 */
struct XorShift32 {
  /** @brief Internal RNG state. */
  std::uint32_t s = 0x12345678u;

  /**
   * @brief Constructs the generator with a non-zero seed.
   * @param seed Initial state value. If zero, a default non-zero seed is used.
   */
  constexpr explicit XorShift32(std::uint32_t seed = 0x12345678u)
      : s(seed ? seed : 0x12345678u) {}

  /**
   * @brief Generates the next 32-bit random value.
   * @return Next pseudo-random unsigned integer.
   */
  constexpr std::uint32_t next_u32() {
    std::uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
  }

  /**
   * @brief Generates a random integer in an inclusive range.
   * @param lo Lower bound (inclusive).
   * @param hi Upper bound (inclusive).
   * @return Random integer in `[lo, hi]`.
   */
  constexpr int next_int(int lo, int hi) {
    const std::uint32_t r = next_u32();
    const std::uint32_t span = static_cast<std::uint32_t>(hi - lo + 1);
    return lo + static_cast<int>(r % span);
  }

  /**
   * @brief Flips a random coin.
   * @return `true` or `false` with approximately equal probability.
   */
  constexpr bool coin() { return (next_u32() & 1u) != 0; }
};
