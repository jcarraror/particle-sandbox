/**
 * @file world_pressure_dense.cpp
 * @brief Dense-material pressure target computation (sand/water/oil/lava).
 */

#include "world_pressure_internal.hpp"

#include <algorithm>
#include <array>
#include <vector>

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
constexpr int kDenseRelaxationPasses = 2;
constexpr int kDenseRelaxSelfWeight = 5;
constexpr int kDenseRelaxUpWeight = 4;
constexpr int kDenseRelaxSideWeight = 2;
constexpr int kDenseRelaxDownWeight = 1;
constexpr int kDenseRelaxBlendNumerator = 1;
constexpr int kDenseRelaxBlendDenominator = 3;
constexpr int kDenseLoadPropagationPasses = 3;
constexpr int kDenseLoadSeedScale = 8;
constexpr int kDenseLoadMax = 480;
constexpr int kDenseLoadSelfDecay = 2;
constexpr int kDenseLoadVerticalLoss = 8;
constexpr int kDenseLoadLateralLoss = 18;
constexpr int kDenseLoadUpwardLoss = 28;
constexpr int kDenseLoadWallTransmitBonus = 6;
constexpr int kDenseLoadLavaTransmitBonus = 4;
constexpr int kDenseLoadWaterTransmitPenalty = 4;
constexpr int kDenseLoadOilTransmitPenalty = 6;
constexpr int kDenseLoadPropagationBlendNumerator = 1;
constexpr int kDenseLoadPropagationBlendDenominator = 3;
constexpr int kDenseLoadToPressureDivisor = 14;
constexpr int kDenseLoadLateralSpillPasses = 2;
constexpr int kDenseLoadLateralSpillLoss = 24;
constexpr int kDenseLoadLateralSpillBlendNumerator = 1;
constexpr int kDenseLoadLateralSpillBlendDenominator = 4;
constexpr std::size_t cell_type_index(CellType t) noexcept {
  return is_valid_cell_type(t) ? static_cast<std::size_t>(t) : 0u;
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

struct DenseRelaxProps {
  std::uint8_t source_weight{};   // How strongly this material contributes pressure to neighbors.
  std::uint8_t receive_weight{};  // 0 means no relaxation writeback for this material.
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

// Dense-pressure relaxation properties (field propagation, not movement behavior).
// Any loaded dense-ish material can contribute source pressure; only selected materials
// receive smoothed pressure updates to preserve sandbox liquid motion feel.
constexpr std::array<DenseRelaxProps, kCellTypeCount> kDenseRelaxProps{{
    {0, 0},  // Empty
    {3, 2},  // Wall / crust: strong bridge and receives to transmit into lava
    {2, 2},  // Sand
    {2, 0},  // Water: contributes load but no relaxed writeback
    {2, 0},  // Oil: contributes load but no relaxed writeback
    {0, 0},  // Fire
    {0, 0},  // Smoke
    {3, 3},  // Lava: strongest dense receiver/transmitter
}};

constexpr DenseRelaxProps dense_relax_props(CellType t) noexcept {
  return kDenseRelaxProps[cell_type_index(t)];
}

int dense_load_material_adjust(CellType t) noexcept {
  switch (t) {
    case CellType::Wall: return -kDenseLoadWallTransmitBonus;
    case CellType::Lava: return -kDenseLoadLavaTransmitBonus;
    case CellType::Water: return kDenseLoadWaterTransmitPenalty;
    case CellType::Oil: return kDenseLoadOilTransmitPenalty;
    default: return 0;
  }
}

bool transmits_dense_load(CellType t) noexcept {
  return dense_relax_props(t).source_weight > 0;
}

bool receives_dense_load(CellType t) noexcept {
  return dense_relax_props(t).receive_weight > 0;
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

void relax_dense_pressure(World& world) {
  if (world.w < 3 || world.h < 3) return;

  std::vector<std::int16_t> next(world.cells.size());
  std::vector<int> load_seed(world.cells.size(), 0);
  std::vector<int> load_curr(world.cells.size(), 0);
  std::vector<int> load_next(world.cells.size(), 0);

  auto idx_of = [&](int x, int y) -> std::size_t {
    return static_cast<std::size_t>(y * world.w + x);
  };

  auto add_load_seed = [&](int x, int y, int amount) {
    const std::size_t idx = idx_of(x, y);
    load_seed[idx] = std::clamp(load_seed[idx] + amount, 0, kDenseLoadMax);
  };

  // Seed separate load field from material weight and local overburden so added top mass
  // can propagate through connected dense structures even when pressure saturate.
  for (int y = 1; y < world.h - 1; ++y) {
    for (int x = 1; x < world.w - 1; ++x) {
      const Cell& c = world.at(x, y);
      const DenseRelaxProps rp = dense_relax_props(c.type);
      if (rp.source_weight == 0) continue;

      const int base_mass = static_cast<int>(vertical_load_props(c.type).load_units) * kDenseLoadSeedScale;
      add_load_seed(x, y, base_mass * static_cast<int>(rp.source_weight));

      // Additional load from vertical stack above
      add_load_seed(x, y, vertical_overburden_load(world, x, y) * 3);

      // Supported dense cells transmit more load than free-falling/exposed cells.
      if (supports_dense(world.at(x, y + 1).type)) add_load_seed(x, y, 10);
    }
  }

  // Structural load pass:
  // 1) top-down accumulation (primary gravity-driven behavior)
  // 2) conservative lateral spill through connected dense structures (organic spread)
  load_curr.assign(world.cells.size(), 0);
  for (int y = 1; y < world.h - 1; ++y) {
    for (int x = 1; x < world.w - 1; ++x) {
      const Cell& c = world.at(x, y);
      if (!transmits_dense_load(c.type)) continue;

      const std::size_t idx = idx_of(x, y);
      int value = load_seed[idx];

      const Cell& up = world.at(x, y - 1);
      if (transmits_dense_load(up.type)) {
        int carried = load_curr[idx_of(x, y - 1)];
        carried -= kDenseLoadVerticalLoss;
        carried -= dense_load_material_adjust(up.type);
        carried = (carried * static_cast<int>(dense_relax_props(up.type).source_weight)) / 3;
        if (supports_dense(world.at(x, y + 1).type)) carried += 6;
        value = std::max(value, carried);
      }

      load_curr[idx] = std::clamp(value, 0, kDenseLoadMax);
    }
  }

  for (int pass = 0; pass < kDenseLoadLateralSpillPasses; ++pass) {
    load_next = load_curr;
    for (int y = 1; y < world.h - 1; ++y) {
      for (int x = 1; x < world.w - 1; ++x) {
        const Cell& c = world.at(x, y);
        if (!receives_dense_load(c.type)) continue;

        const std::size_t idx = idx_of(x, y);
        int target = load_curr[idx];

        auto absorb_side = [&](int nx) {
          const Cell& n = world.at(nx, y);
          if (!transmits_dense_load(n.type)) return;
          int candidate = load_curr[idx_of(nx, y)];
          candidate -= kDenseLoadLateralSpillLoss;
          candidate -= dense_load_material_adjust(n.type);
          if (n.type == CellType::Wall) candidate += 4;
          target = std::max(target, candidate);
        };

        absorb_side(x - 1);
        absorb_side(x + 1);

        const int blended =
            (load_curr[idx] * (kDenseLoadLateralSpillBlendDenominator - kDenseLoadLateralSpillBlendNumerator) +
             std::max(0, target) * kDenseLoadLateralSpillBlendNumerator) /
            kDenseLoadLateralSpillBlendDenominator;
        load_next[idx] = std::clamp(blended, 0, kDenseLoadMax);
      }
    }
    load_curr.swap(load_next);
  }

  // attenuation pass to avoid hard plateaus in very large masses.
  for (int pass = 0; pass < kDenseLoadPropagationPasses; ++pass) {
    load_next = load_curr;
    for (int y = 1; y < world.h - 1; ++y) {
      for (int x = 1; x < world.w - 1; ++x) {
        const Cell& c = world.at(x, y);
        if (!receives_dense_load(c.type)) continue;
        const std::size_t idx = idx_of(x, y);
        int target = std::max(load_seed[idx], load_curr[idx] - kDenseLoadSelfDecay);
        const Cell& up = world.at(x, y - 1);
        if (transmits_dense_load(up.type)) {
          int candidate = load_curr[idx_of(x, y - 1)] - kDenseLoadVerticalLoss;
          target = std::max(target, candidate);
        }
        const int blended =
            (load_curr[idx] * (kDenseLoadPropagationBlendDenominator - kDenseLoadPropagationBlendNumerator) +
             std::max(0, target) * kDenseLoadPropagationBlendNumerator) /
            kDenseLoadPropagationBlendDenominator;
        load_next[idx] = std::clamp(blended, 0, kDenseLoadMax);
      }
    }
    load_curr.swap(load_next);
  }

  // Persist the propagated dense load field for debug
  for (int y = 1; y < world.h - 1; ++y) {
    for (int x = 1; x < world.w - 1; ++x) {
      Cell& c = world.at(x, y);
      const DenseRelaxProps rp = dense_relax_props(c.type);
      const std::size_t idx = idx_of(x, y);
      c.load = static_cast<std::int16_t>(rp.source_weight > 0 ? std::clamp(load_curr[idx], 0, kDenseLoadMax) : 0);
    }
  }

  for (int pass = 0; pass < kDenseRelaxationPasses; ++pass) {
    for (std::size_t i = 0; i < world.cells.size(); ++i) {
      next[i] = world.cells[i].pressure;
    }

    auto maybe_add = [&](int nx, int ny, int directional_weight, int& accum, int& total_w) {
      const Cell& n = world.at(nx, ny);
      const DenseRelaxProps rp = dense_relax_props(n.type);
      if (rp.source_weight == 0) return;
      const int w = directional_weight * static_cast<int>(rp.source_weight);
      accum += static_cast<int>(n.pressure) * w;
      total_w += w;
    };

    for (int y = 1; y < world.h - 1; ++y) {
      for (int x = 1; x < world.w - 1; ++x) {
        Cell& c = world.at(x, y);
        const DenseRelaxProps self_props = dense_relax_props(c.type);
        if (self_props.receive_weight == 0) continue;

        const int self_w = kDenseRelaxSelfWeight * static_cast<int>(self_props.receive_weight);
        int accum = static_cast<int>(c.pressure) * self_w;
        int total_w = self_w;

        // Downstream cells should feel load from above more strongly than the inverse.
        maybe_add(x, y - 1, kDenseRelaxUpWeight, accum, total_w);
        maybe_add(x - 1, y, kDenseRelaxSideWeight, accum, total_w);
        maybe_add(x + 1, y, kDenseRelaxSideWeight, accum, total_w);
        maybe_add(x, y + 1, kDenseRelaxDownWeight, accum, total_w);

        const int neighbor_avg = (total_w > 0) ? (accum / total_w) : static_cast<int>(c.pressure);
        const int propagated_load_pressure =
            load_curr[idx_of(x, y)] / kDenseLoadToPressureDivisor;
        const int blended =
            (static_cast<int>(c.pressure) * (kDenseRelaxBlendDenominator - kDenseRelaxBlendNumerator) +
             (neighbor_avg + propagated_load_pressure) * kDenseRelaxBlendNumerator) /
            kDenseRelaxBlendDenominator;

        next[static_cast<std::size_t>(y * world.w + x)] =
            static_cast<std::int16_t>(std::clamp(blended, kPressureMin, kPressureMax));
      }
    }

    for (int y = 1; y < world.h - 1; ++y) {
      for (int x = 1; x < world.w - 1; ++x) {
        Cell& c = world.at(x, y);
        if (dense_relax_props(c.type).receive_weight == 0) continue;
        c.pressure = next[static_cast<std::size_t>(y * world.w + x)];
      }
    }
  }
}

}  // namespace pressure_detail
