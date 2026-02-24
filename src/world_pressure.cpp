/**
 * @file world_pressure.cpp
 * @brief Pressure pass orchestration and shared damping.
 */

#include "world_pressure_internal.hpp"

#include <algorithm>

namespace pressure_detail {

constexpr int kPressureMin = 0;
constexpr int kPressureMax = 240;
constexpr int kPressureDampingNumerator = 1;
constexpr int kPressureDampingDenominator = 3;

std::int16_t damp_pressure(std::int16_t current, int target) noexcept {
  const int blended =
      (static_cast<int>(current) * kPressureDampingNumerator +
       target * (kPressureDampingDenominator - kPressureDampingNumerator)) /
      kPressureDampingDenominator;
  return static_cast<std::int16_t>(std::clamp(blended, kPressureMin, kPressureMax));
}

}  // namespace pressure_detail

namespace {
constexpr std::uint64_t kDensePressureRelaxEveryNTicks = 2;
}

void World::pass_pressure_update() {
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);

      if (c.type == CellType::Smoke) {
        c.load = 0;
        c.pressure = pressure_detail::damp_pressure(
            c.pressure, pressure_detail::compute_smoke_pressure_target(*this, x, y, c));
        continue;
      }

      if (pressure_detail::is_dense_pressure_material(c.type)) {
        c.pressure = pressure_detail::damp_pressure(
            c.pressure, pressure_detail::compute_dense_pressure_target(*this, x, y, c));
        continue;
      }

      c.pressure = 0;
      c.load = 0;
    }
  }

  if ((tick_count % kDensePressureRelaxEveryNTicks) == 0) {
    pressure_detail::relax_dense_pressure(*this);
  }
}
