/**
 * @file world_pressure_gas.cpp
 * @brief Gas/smoke pressure target computation.
 */

#include "world_pressure_internal.hpp"

#include <algorithm>

namespace pressure_detail {
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

bool is_gas_like(CellType t) noexcept { return t == CellType::Smoke; }

bool is_fluid_like(CellType t) noexcept { return t == CellType::Water || t == CellType::Oil; }

bool is_open_for_smoke(CellType t) noexcept { return t == CellType::Empty || t == CellType::Smoke; }

}  // namespace

int compute_smoke_pressure_target(const World& world, int x, int y, const Cell& c) {
  int p = 0;

  const CellType up = world.at(x, y - 1).type;
  if (!is_open_for_smoke(up)) p += kBlockedAbovePenalty;
  else p -= kOpenRelief;

  const CellType down = world.at(x, y + 1).type;
  if (!is_open_for_smoke(down)) p += kBlockedSidePenalty;
  else p -= kOpenRelief / 2;

  const CellType left = world.at(x - 1, y).type;
  const CellType right = world.at(x + 1, y).type;

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
      !is_open_for_smoke(world.at(x - 1, y - 1).type) &&
      !is_open_for_smoke(world.at(x + 1, y - 1).type);
  if (no_escape) p += kNoEscapeBonus;

  if (c.temp > kAmbientTemp) {
    p += ((static_cast<int>(c.temp) - kAmbientTemp) / kHeatBonusStep) * kHeatBonusAmount;
  }

  return std::clamp(p, kPressureMin, kPressureMax);
}

}  // namespace pressure_detail

