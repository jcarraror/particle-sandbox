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

struct GasPressureCoeffs {
  int blocked_above_penalty{};
  int blocked_side_penalty{};
  int fluid_side_penalty{};
  int gas_neighbor_penalty{};
  int open_relief{};
  int no_escape_bonus{};
  int heat_bonus_step{};
  int heat_bonus_amount{};
};

constexpr GasPressureCoeffs kSmokePressureCoeffs{
    .blocked_above_penalty = 55,
    .blocked_side_penalty = 22,
    .fluid_side_penalty = 14,
    .gas_neighbor_penalty = 4,
    .open_relief = 12,
    .no_escape_bonus = 30,
    .heat_bonus_step = 40,
    .heat_bonus_amount = 6,
};

bool is_gas_like(CellType t) noexcept { return t == CellType::Smoke; }

bool is_fluid_like(CellType t) noexcept { return t == CellType::Water || t == CellType::Oil; }

bool is_open_for_smoke(CellType t) noexcept { return t == CellType::Empty || t == CellType::Smoke; }

}  // namespace

int compute_smoke_pressure_target(const World& world, int x, int y, const Cell& c) {
  const GasPressureCoeffs& cfg = kSmokePressureCoeffs;
  int p = 0;

  const CellType up = world.at(x, y - 1).type;
  if (!is_open_for_smoke(up)) p += cfg.blocked_above_penalty;
  else p -= cfg.open_relief;

  const CellType down = world.at(x, y + 1).type;
  if (!is_open_for_smoke(down)) p += cfg.blocked_side_penalty;
  else p -= cfg.open_relief / 2;

  const CellType left = world.at(x - 1, y).type;
  const CellType right = world.at(x + 1, y).type;

  auto accumulate_side = [&](CellType t) {
    if (t == CellType::Empty) {
      p -= cfg.open_relief;
    } else if (is_gas_like(t)) {
      p += cfg.gas_neighbor_penalty;
    } else if (is_fluid_like(t)) {
      p += cfg.fluid_side_penalty;
    } else {
      p += cfg.blocked_side_penalty;
    }
  };

  accumulate_side(left);
  accumulate_side(right);

  const bool no_escape =
      !is_open_for_smoke(up) &&
      !is_open_for_smoke(world.at(x - 1, y - 1).type) &&
      !is_open_for_smoke(world.at(x + 1, y - 1).type);
  if (no_escape) p += cfg.no_escape_bonus;

  if (c.temp > kAmbientTemp) {
    p += ((static_cast<int>(c.temp) - kAmbientTemp) / cfg.heat_bonus_step) * cfg.heat_bonus_amount;
  }

  return std::clamp(p, kPressureMin, kPressureMax);
}

}  // namespace pressure_detail
