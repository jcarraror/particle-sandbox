/**
 * @file world_pressure.cpp
 * @brief Pressure heuristic pass for gas-like materials.
 */

#include "world.hpp"

#include <algorithm>

namespace {

constexpr int kAmbientTemp = 20;
constexpr int kPressureMin = 0;
constexpr int kPressureMax = 240;

constexpr int kBlockedAbovePenalty = 55;
constexpr int kBlockedSidePenalty = 22;
constexpr int kFluidSidePenalty = 14;
constexpr int kGasNeighborPenalty = 4;
constexpr int kOpenRelief = 12;
constexpr int kNoEscapeBonus = 30;
constexpr int kHeatBonusStep = 40;
constexpr int kHeatBonusAmount = 6;

bool is_gas_like(CellType t) {
  return t == CellType::Smoke;
}

bool is_fluid_like(CellType t) {
  return t == CellType::Water || t == CellType::Oil;
}

bool is_open_for_smoke(CellType t) {
  return t == CellType::Empty || t == CellType::Smoke;
}

}  // namespace

void World::pass_pressure_update() {
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);

      if (c.type != CellType::Smoke) {
        c.pressure = 0;
        continue;
      }

      int p = 0;

      const CellType up = at(x, y - 1).type;
      if (!is_open_for_smoke(up)) p += kBlockedAbovePenalty;
      else p -= kOpenRelief;

      const CellType down = at(x, y + 1).type;
      if (!is_open_for_smoke(down)) p += kBlockedSidePenalty;
      else p -= kOpenRelief / 2;

      const CellType left = at(x - 1, y).type;
      const CellType right = at(x + 1, y).type;

      auto accumulate_side = [&](CellType t) {
        if (t == CellType::Empty) {
          p -= kOpenRelief;
        } else if (is_gas_like(t)) {
          p += kGasNeighborPenalty;
        } else if (is_fluid_like(t)) {
          p += kFluidSidePenalty;
        } else {
          p += kBlockedSidePenalty;
        }
      };

      accumulate_side(left);
      accumulate_side(right);

      const bool no_escape =
          !is_open_for_smoke(up) &&
          !is_open_for_smoke(at(x - 1, y - 1).type) &&
          !is_open_for_smoke(at(x + 1, y - 1).type);
      if (no_escape) p += kNoEscapeBonus;

      if (c.temp > kAmbientTemp) {
        p += ((static_cast<int>(c.temp) - kAmbientTemp) / kHeatBonusStep) * kHeatBonusAmount;
      }

      c.pressure = static_cast<std::int16_t>(std::clamp(p, kPressureMin, kPressureMax));
    }
  }
}
