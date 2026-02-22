/**
 * @file world_pressure_dense.cpp
 * @brief Dense-material pressure target computation (sand/water/oil/lava).
 */

#include "world_pressure_internal.hpp"

#include <algorithm>
#include <array>

namespace pressure_detail {
namespace {

constexpr int kAmbientTemp = 20;
constexpr int kPressureMin = 0;
constexpr int kPressureMax = 240;

constexpr int kLiquidDepthPressurePerCell = 18;
constexpr int kLiquidBlockedBelowBonus = 28;
constexpr int kLiquidWallSideBonus = 10;
constexpr int kLiquidOpenSideRelief = 8;
constexpr int kLiquidHeatBonusStep = 60;
constexpr int kLiquidHeatBonusAmount = 4;

constexpr int kSandDepthPressurePerCell = 20;
constexpr int kSandColumnLoadPressurePerUnit = 6;
constexpr int kSandBlockedBelowBonus = 18;
constexpr int kSandWallSideBonus = 6;
constexpr int kSandOpenSideRelief = 4;
constexpr int kSandHeatBonusStep = 100;
constexpr int kSandHeatBonusAmount = 0;

constexpr int kLavaDepthPressurePerCell = 22;
constexpr int kLavaColumnLoadPressurePerUnit = 10;
constexpr int kLavaBlockedBelowBonus = 34;
constexpr int kLavaWallSideBonus = 16;
constexpr int kLavaOpenSideRelief = 5;
constexpr int kLavaHeatBonusStep = 80;
constexpr int kLavaHeatBonusAmount = 8;
constexpr int kLavaWaterContactBonus = 26;
constexpr int kLavaCrustContactBonus = 12;

constexpr int kMaxDenseOverburdenSample = 8;
constexpr int kMaxVerticalLoadSample = kMaxDenseOverburdenSample + 4;
constexpr std::size_t kCellTypeCount = 8;

constexpr std::size_t cell_type_index(CellType t) noexcept {
  return static_cast<std::size_t>(t);
}

struct DensePressureCoeffs {
  int same_material_depth_per_cell{};
  int overburden_per_unit{};
  int blocked_below_bonus{};
  int wall_side_bonus{};
  int open_side_relief{};
  int heat_bonus_step{};
  int heat_bonus_amount{};
  int water_contact_bonus{};
  int crust_contact_bonus{};
};

struct VerticalLoadProps {
  bool transmits{};
  std::uint8_t load_units{};
};

// Vertical load transmission used by dense-pressure overburden.
// heuristic "load units", not physical masses.
constexpr std::array<VerticalLoadProps, kCellTypeCount> kVerticalLoadProps{{
    {false, 0},  // Empty
    {true, 2},   // Wall / crust
    {true, 2},   // Sand
    {true, 2},   // Water
    {true, 2},   // Oil
    {true, 1},   // Fire (light contribution)
    {false, 0},  // Smoke
    {true, 3},   // Lava (heaviest dense fluid so far)
}};

constexpr VerticalLoadProps vertical_load_props(CellType t) noexcept {
  return kVerticalLoadProps[cell_type_index(t)];
}

bool supports_dense(CellType t) noexcept {
  return t != CellType::Empty && t != CellType::Smoke;
}

int vertical_overburden_load(const World& world, int x, int y) {
  int load = 0;
  int sampled = 0;
  for (int ny = y - 1; ny >= 1 && sampled < kMaxVerticalLoadSample; --ny, ++sampled) {
    const VerticalLoadProps props = vertical_load_props(world.at(x, ny).type);
    if (!props.transmits) break;
    load += props.load_units;
  }
  return load;
}

int same_material_overburden_depth(const World& world, int x, int y, CellType material) {
  int depth = 0;
  for (int ny = y - 1; ny >= 1 && depth < kMaxDenseOverburdenSample; --ny) {
    if (world.at(x, ny).type != material) break;
    ++depth;
  }
  return depth;
}

DensePressureCoeffs coeffs_for_dense(CellType t) {
  switch (t) {
    case CellType::Sand:
      return DensePressureCoeffs{
          .same_material_depth_per_cell = kSandDepthPressurePerCell,
          .overburden_per_unit = kSandColumnLoadPressurePerUnit,
          .blocked_below_bonus = kSandBlockedBelowBonus,
          .wall_side_bonus = kSandWallSideBonus,
          .open_side_relief = kSandOpenSideRelief,
          .heat_bonus_step = kSandHeatBonusStep,
          .heat_bonus_amount = kSandHeatBonusAmount,
      };
    case CellType::Water:
    case CellType::Oil:
      return DensePressureCoeffs{
          .same_material_depth_per_cell = kLiquidDepthPressurePerCell,
          .overburden_per_unit = 0,
          .blocked_below_bonus = kLiquidBlockedBelowBonus,
          .wall_side_bonus = kLiquidWallSideBonus,
          .open_side_relief = kLiquidOpenSideRelief,
          .heat_bonus_step = kLiquidHeatBonusStep,
          .heat_bonus_amount = kLiquidHeatBonusAmount,
      };
    case CellType::Lava:
      return DensePressureCoeffs{
          .same_material_depth_per_cell = kLavaDepthPressurePerCell,
          .overburden_per_unit = kLavaColumnLoadPressurePerUnit,
          .blocked_below_bonus = kLavaBlockedBelowBonus,
          .wall_side_bonus = kLavaWallSideBonus,
          .open_side_relief = kLavaOpenSideRelief,
          .heat_bonus_step = kLavaHeatBonusStep,
          .heat_bonus_amount = kLavaHeatBonusAmount,
          .water_contact_bonus = kLavaWaterContactBonus,
          .crust_contact_bonus = kLavaCrustContactBonus,
      };
    default:
      return {};
  }
}

}  // namespace

bool is_dense_pressure_material(CellType t) noexcept {
  return t == CellType::Sand || t == CellType::Water || t == CellType::Oil || t == CellType::Lava;
}

int compute_dense_pressure_target(const World& world, int x, int y, const Cell& c) {
  const DensePressureCoeffs coeffs = coeffs_for_dense(c.type);

  int p = 0;
  p += same_material_overburden_depth(world, x, y, c.type) * coeffs.same_material_depth_per_cell;
  p += vertical_overburden_load(world, x, y) * coeffs.overburden_per_unit;

  const CellType below = world.at(x, y + 1).type;
  if (supports_dense(below)) p += coeffs.blocked_below_bonus;

  auto side_pressure = [&](CellType t) {
    if (t == CellType::Empty || t == CellType::Smoke) return -coeffs.open_side_relief;
    if (t == CellType::Wall) return coeffs.wall_side_bonus;
    if (coeffs.water_contact_bonus > 0 && t == CellType::Water) return coeffs.water_contact_bonus;
    return 0;
  };

  p += side_pressure(world.at(x - 1, y).type);
  p += side_pressure(world.at(x + 1, y).type);

  const CellType up = world.at(x, y - 1).type;
  if (coeffs.water_contact_bonus > 0 && (up == CellType::Water || below == CellType::Water)) {
    p += coeffs.water_contact_bonus;
  }
  if (coeffs.crust_contact_bonus > 0 && (up == CellType::Wall || below == CellType::Wall)) {
    p += coeffs.crust_contact_bonus;
  }

  if (c.temp > kAmbientTemp) {
    p += ((static_cast<int>(c.temp) - kAmbientTemp) / coeffs.heat_bonus_step) * coeffs.heat_bonus_amount;
  }

  return std::clamp(p, kPressureMin, kPressureMax);
}

}  // namespace pressure_detail
