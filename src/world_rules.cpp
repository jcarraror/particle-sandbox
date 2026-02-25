/**
 * @file world_rules.cpp
 * @brief World simulation rule dispatch and per-material behavior implementation.
 */

#include "world.hpp"
#include "material_props.hpp"
#include "sim_tuning.hpp"

#include <algorithm>
#include <array>

/**
 * @brief Clamps an integer to an inclusive range.
 * @param v Value to clamp.
 * @param lo Lower bound.
 * @param hi Upper bound.
 * @return Clamped value.
 */
static constexpr int clampi(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

namespace {

struct NeighborOffset {
  int dx;
  int dy;
};

constexpr std::array<NeighborOffset, 8> kMooreOffsets{{
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1},
}};

constexpr std::array<NeighborOffset, 4> kCardinalOffsets{{
    {0, -1}, {1, 0}, {0, 1}, {-1, 0},
}};

template <class Fn>
void for_each_neighbor(World& world, int x, int y, Fn&& fn) {
  for (const auto [dx, dy] : kMooreOffsets) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (!world.in_bounds(nx, ny)) continue;
    fn(nx, ny, world.at(nx, ny));
  }
}

inline std::int8_t clamp_impulse(int v) {
  return static_cast<std::int8_t>(clampi(v, -8, 8));
}

bool is_liquid_cell(CellType t) {
  return sim::material_behavior(t).is_liquid();
}

void emit_liquid_splash_droplets(World& world, int x, int y, CellType liquid_type) {
  const int droplet_budget = 3 + static_cast<int>(world.rng.next_u32() % 4u);  // 3..6
  for (int n = 0; n < droplet_budget; ++n) {
    const int dir = world.rng.coin() ? -1 : 1;
    const int lift = 1 + static_cast<int>(world.rng.next_u32() % 2u);    // 1..2
    const int reach = 1 + static_cast<int>(world.rng.next_u32() % 3u);   // 1..3
    const std::array<std::pair<int, int>, 3> candidates{{
        {x + dir, y - 1},
        {x + dir * reach, y - 1},
        {x + dir * reach, y - lift},
    }};
    bool emitted = false;
    for (const auto& [tx, ty] : candidates) {
      if (!world.in_bounds(tx, ty) || !world.in_bounds(tx, ty + 1)) continue;
      Cell& dst = world.at(tx, ty);
      if (dst.type != CellType::Empty) continue;

      // Find a source liquid cell near the impact rim.
      const std::array<std::pair<int, int>, 2> sources{{{x + dir, y}, {x, y}}};
      for (const auto& [sx, sy] : sources) {
        if (!world.in_bounds(sx, sy)) continue;
        Cell& src = world.at(sx, sy);
        if (src.type != liquid_type) continue;

        std::swap(src, dst);
        dst.updated = world.stamp;
        dst.impulse_x = clamp_impulse(static_cast<int>(dst.impulse_x) + ((dir < 0) ? -2 : 2));
        dst.impulse_y = clamp_impulse(static_cast<int>(dst.impulse_y) - (2 + lift));
        src.updated = world.stamp;
        emitted = true;
        break;
      }
      if (emitted) break;
    }
  }
}

void ignite_oil_neighbors(World& world, int x, int y) {
  for_each_neighbor(world, x, y, [&](int, int, Cell& n) {
    if (n.type != CellType::Oil) return;
    if ((world.rng.next_u32() % simcfg::kOilIgniteOddsDivisor) != 0u) return;

    n.type = CellType::Fire;
    n.temp = simcfg::kIgnitedFireTemp;
    n.updated = world.stamp;
  });
}

void absorb_heat_from_hot_neighbors(World& world, int x, int y) {
  Cell& water = world.at(x, y);
  int absorbed = 0;

  for_each_neighbor(world, x, y, [&](int, int, Cell& n) {
    if (n.type == CellType::Fire) {
      n.temp = static_cast<std::int16_t>(clampi(
          n.temp - simcfg::kWaterHeatAbsorbFromFire, simcfg::kMinCellTemp, simcfg::kFireMaxTemp));
      absorbed += simcfg::kWaterHeatAbsorbFromFire;
      return;
    }
    if (n.type == CellType::Lava) {
      n.temp = static_cast<std::int16_t>(clampi(
          n.temp - simcfg::kWaterHeatAbsorbFromLava, simcfg::kMinCellTemp, simcfg::kLavaMaxTemp));
      absorbed += simcfg::kWaterHeatAbsorbFromLava;
    }
  });

  if (absorbed <= 0) return;
  water.temp = static_cast<std::int16_t>(clampi(
      water.temp + absorbed, simcfg::kAmbientTemp, simcfg::kMaxNeighborHeatTemp));
}

bool try_evaporate_water(World& world, int x, int y) {
  Cell& c = world.at(x, y);
  if (c.type != CellType::Water) return false;
  if (c.temp < simcfg::kWaterSteamTempThreshold) return false;

  int direct_lava = 0;
  int direct_fire = 0;
  int hot_wall = 0;
  for (const auto [dx, dy] : std::array<NeighborOffset, 4>{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}}) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (!world.in_bounds(nx, ny)) continue;
    const Cell& n = world.at(nx, ny);
    if (n.type == CellType::Lava) ++direct_lava;
    else if (n.type == CellType::Fire) ++direct_fire;
    else if (sim::material_behavior(n.type).rigid_support &&
             sim::material_behavior(n.type).cooling_surface &&
             n.temp >= simcfg::kHotWallSteamTempThreshold) ++hot_wall;
  }
  if (direct_lava == 0 && direct_fire == 0 && hot_wall == 0) return false;

  const int overheat = std::max(0, static_cast<int>(c.temp) - simcfg::kWaterSteamTempThreshold);
  const std::uint32_t bonus = static_cast<std::uint32_t>(overheat / 40);
  std::uint32_t divisor =
      std::max<std::uint32_t>(1u, simcfg::kWaterSteamOddsDivisor - std::min<std::uint32_t>(3u, bonus));
  if (direct_lava > 0) divisor = std::max<std::uint32_t>(1u, divisor / simcfg::kWaterSteamDirectLavaBonusDivisor);
  else if (direct_fire > 0) divisor = std::max<std::uint32_t>(1u, divisor / simcfg::kWaterSteamDirectFireBonusDivisor);
  else if (hot_wall > 0) divisor = std::max(divisor, simcfg::kWaterSteamHotWallOddsDivisor);
  if ((world.rng.next_u32() % divisor) != 0u) return false;

  c.type = CellType::Smoke;
  c.temp = simcfg::kSteamTemp;
  c.pressure = simcfg::kExtremeSmokePressure;
  c.updated = world.stamp;
  return true;
}

void quench_fire_from_water(World& world, int x, int y) {
  Cell& fire = world.at(x, y);
  int adjacent_water = 0;

  for_each_neighbor(world, x, y, [&](int, int, Cell& n) {
    if (n.type != CellType::Water) return;
    ++adjacent_water;
    n.temp = static_cast<std::int16_t>(clampi(
        n.temp + simcfg::kWaterHeatAbsorbFromFire, simcfg::kAmbientTemp, simcfg::kMaxNeighborHeatTemp));
  });

  if (adjacent_water == 0) return;

  fire.temp = static_cast<std::int16_t>(clampi(
      fire.temp - adjacent_water * simcfg::kFireWaterQuenchPerNeighbor,
      simcfg::kMinCellTemp,
      simcfg::kFireMaxTemp));
}

bool try_swap_smoke_with_fluid(World& world, int x, int y, int nx, int ny) {
  if (!world.in_bounds(nx, ny)) return false;

  Cell& src = world.at(x, y);
  Cell& dst = world.at(nx, ny);
  if (src.type != CellType::Smoke) return false;
  if (!sim::material_behavior(dst.type).is_liquid()) return false;

  const bool steam_like = src.temp >= simcfg::kSteamSmokeTempThreshold;
  std::uint32_t divisor =
      steam_like ? simcfg::kSteamBubbleSwapOddsDivisor : simcfg::kSmokeBubbleSwapOddsDivisor;
  if (src.pressure >= simcfg::kHighSmokePressure && divisor > 1u) divisor -= 1u;
  if (divisor > 1u && (world.rng.next_u32() % divisor) != 0u) return false;

  std::swap(src, dst);
  world.at(nx, ny).updated = world.stamp;
  return true;
}

bool try_swap_liquid_with_smoke(World& world, int x, int y, int nx, int ny) {
  if (!world.in_bounds(nx, ny)) return false;

  Cell& src = world.at(x, y);
  Cell& dst = world.at(nx, ny);
  if (dst.type != CellType::Smoke) return false;

  const bool liquid_src = sim::material_behavior(src.type).is_liquid();
  if (!liquid_src) return false;

  std::swap(src, dst);
  src.updated = world.stamp;  // displaced smoke should not re-step this tick
  dst.updated = world.stamp;  // moved liquid already consumed its step
  return true;
}

bool try_swap_stone_with_fluid(World& world, int x, int y, int nx, int ny) {
  if (!world.in_bounds(nx, ny)) return false;

  Cell& src = world.at(x, y);
  Cell& dst = world.at(nx, ny);
  if (src.type != CellType::Stone) return false;
  const auto& dstb = sim::material_behavior(dst.type);
  if (!(dstb.is_liquid() || dstb.is_gas())) return false;
  const CellType displaced_type = dst.type;
  const bool vertical_entry = (nx == x && ny == y + 1);

  if (vertical_entry && (displaced_type == CellType::Water || displaced_type == CellType::Oil)) {
    const std::array<std::pair<int, int>, 4> eject_targets{{{x - 1, y}, {x + 1, y}, {x - 1, y - 1}, {x + 1, y - 1}}};
    for (const auto& [ex, ey] : eject_targets) {
      if (!world.in_bounds(ex, ey)) continue;
      Cell& e = world.at(ex, ey);
      if (e.type != CellType::Empty) continue;

      Cell stone = src;
      Cell displaced = dst;

      dst = stone;
      dst.updated = world.stamp;
      dst.flow_dir = 0;
      dst.flow_strength = 0;

      src = Cell{};
      src.updated = world.stamp;

      e = displaced;
      e.updated = world.stamp;
      e.flow_dir = 0;
      e.flow_strength = 0;
      if (is_liquid_cell(e.type)) {
        e.impulse_x = clamp_impulse(static_cast<int>(e.impulse_x) + ((ex < x) ? -2 : 2));
        e.impulse_y = clamp_impulse(static_cast<int>(e.impulse_y) - ((ey < y) ? 2 : 1));
        emit_liquid_splash_droplets(world, x, y, e.type);
      }
      return true;
    }
  }

  std::swap(src, dst);
  src.updated = world.stamp;
  dst.updated = world.stamp;
  dst.flow_dir = 0;
  dst.flow_strength = 0;

  if (vertical_entry && (displaced_type == CellType::Water || displaced_type == CellType::Oil)) {
    if (is_liquid_cell(src.type)) {
      src.impulse_x = clamp_impulse(static_cast<int>(src.impulse_x) + (world.rng.coin() ? 1 : -1));
      emit_liquid_splash_droplets(world, x, y, src.type);
    }
  }
  return true;
}

bool smoke_has_escape_route(World& world, int x, int y) {
  static constexpr std::array<NeighborOffset, 5> kEscapeOffsets{{
      {0, -1}, {-1, -1}, {1, -1}, {-1, 0}, {1, 0},
  }};

  for (const auto [dx, dy] : kEscapeOffsets) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (!world.in_bounds(nx, ny)) continue;
    const CellType t = world.at(nx, ny).type;
    if (t == CellType::Empty || sim::material_behavior(t).is_gas()) return true;
  }
  return false;
}

bool liquid_is_constrained(World& world, int x, int y, bool ltr) {
  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  const CellType below = world.at(x, y + 1).type;
  const CellType d1 = world.at(x + dx1, y + 1).type;
  const CellType d2 = world.at(x + dx2, y + 1).type;

  const bool blocked_below = below != CellType::Empty;
  const bool blocked_diag_1 = d1 != CellType::Empty;
  const bool blocked_diag_2 = d2 != CellType::Empty;
  return blocked_below && blocked_diag_1 && blocked_diag_2;
}

int choose_liquid_spread_dir(World& world, int x, int y, bool ltr) {
  const int preferred = (world.rng.coin() ? 1 : -1);
  (void)x;
  (void)y;
  (void)ltr;
  return preferred;
}

int liquid_pressure_spread_bonus(World& world, int x, int y, bool ltr) {
  const Cell& c = world.at(x, y);
  if (!liquid_is_constrained(world, x, y, ltr)) return 0;
  if (c.pressure < simcfg::kLiquidPressureActivationThreshold) return 0;

  const int over = c.pressure - simcfg::kLiquidPressureActivationThreshold;
  const int bonus = 1 + (over / simcfg::kLiquidPressureSpreadBonusStep);
  return std::clamp(bonus, 0, simcfg::kLiquidMaxSpreadBonus);
}

struct LavaContactResult {
  int water_contacts = 0;
  int steam_bursts = 0;
};

int count_adjacent_type(World& world, int x, int y, CellType t) {
  int count = 0;
  for_each_neighbor(world, x, y, [&](int, int, Cell& n) {
    if (n.type == t) ++count;
  });
  return count;
}

int count_cardinal_adjacent_type(World& world, int x, int y, CellType t) {
  int count = 0;
  for (const auto [dx, dy] : kCardinalOffsets) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (!world.in_bounds(nx, ny)) continue;
    if (world.at(nx, ny).type == t) ++count;
  }
  return count;
}

int thermal_sink_score_around(World& world, int x, int y, int source_temp) {
  int score = 0;
  for (const auto [dx, dy] : kCardinalOffsets) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (!world.in_bounds(nx, ny)) continue;
    const Cell& n = world.at(nx, ny);
    if (sim::material_behavior(n.type).molten) continue;

    if (n.type == CellType::Empty) {
      score += 2;  // exposed surface cools faster in sandbox terms
      continue;
    }

    const int temp_drop = std::max(0, source_temp - static_cast<int>(n.temp));
    if (temp_drop <= 0) continue;

    const auto& props = sim::material_props(n.type);
    const int conductivity = static_cast<int>(props.thermal_conductivity);
    const int capacity = std::max(1, static_cast<int>(props.heat_capacity));
    int sink = 1 + (temp_drop / 120) + conductivity / 2;
    sink = std::max(1, sink - capacity / 4);

    // Water is a strong thermal sink and should still favor crusting.
    if (sim::material_behavior(n.type).condenses_gas) sink += 2;
    if (sim::material_behavior(n.type).rigid_support && sim::material_behavior(n.type).cooling_surface) sink += 1;

    score += std::clamp(sink, 0, 4);
  }
  return score;
}

int count_adjacent_wet_cooling_solids(World& world, int x, int y) {
  int count = 0;
  for_each_neighbor(world, x, y, [&](int nx, int ny, Cell& n) {
    const auto& nb = sim::material_behavior(n.type);
    if (!(nb.rigid_support && nb.cooling_surface)) return;
    bool wet = false;
    for_each_neighbor(world, nx, ny, [&](int, int, Cell& m) {
      if (sim::material_behavior(m.type).condenses_gas) wet = true;
    });
    if (wet) ++count;
  });
  return count;
}

void cool_lava_through_crust(World& world, int x, int y) {
  Cell& c = world.at(x, y);
  if (c.type != CellType::Lava) return;

  const int wall_neighbors =
      count_adjacent_type(world, x, y, CellType::Wall) + count_adjacent_type(world, x, y, CellType::Stone);
  const int wet_wall_neighbors = count_adjacent_wet_cooling_solids(world, x, y);
  if (wall_neighbors == 0) return;

  const int cooling = wall_neighbors * simcfg::kLavaCrustCoolingPerWallNeighbor +
                      wet_wall_neighbors * (simcfg::kLavaCrustCoolingPerWetWallNeighbor - simcfg::kLavaCrustCoolingPerWallNeighbor);
  c.temp = static_cast<std::int16_t>(clampi(
      static_cast<int>(c.temp) - cooling, simcfg::kAmbientTemp, simcfg::kLavaMaxTemp));
}

LavaContactResult trigger_lava_water_contact(World& world, int x, int y) {
  Cell& lava = world.at(x, y);
  LavaContactResult result{};

  for_each_neighbor(world, x, y, [&](int nx, int ny, Cell& n) {
    if (n.type != CellType::Water) return;
    ++result.water_contacts;

    n.temp = static_cast<std::int16_t>(clampi(
        n.temp + simcfg::kWaterHeatAbsorbFromLava + simcfg::kWaterHeatAbsorbFromFire,
        simcfg::kAmbientTemp,
        simcfg::kMaxNeighborHeatTemp));
    n.pressure = static_cast<std::int16_t>(clampi(
        static_cast<int>(n.pressure) + simcfg::kLavaWaterPressureSpike, 0, 240));

    const bool hot_enough = n.temp >= simcfg::kLavaSteamBurstTempThreshold;
    const bool burst = (world.rng.next_u32() % simcfg::kLavaWaterSteamBurstOddsDivisor) == 0u;
    if (!hot_enough || !burst) return;
    ++result.steam_bursts;

    n.type = CellType::Smoke;  // Steam proxy.
    n.temp = simcfg::kSteamTemp;
    n.pressure = simcfg::kLavaSteamSpawnPressure;
    n.updated = world.stamp;

    if (world.in_bounds(nx, ny - 1)) {
      Cell& above = world.at(nx, ny - 1);
      if (above.type == CellType::Empty) {
        above.type = CellType::Smoke;
        above.temp = simcfg::kSteamTemp;
        above.pressure = simcfg::kLavaSteamSpawnPressure;
        above.updated = world.stamp;
      }
    }
  });

  if (result.water_contacts == 0) return result;
  lava.temp = static_cast<std::int16_t>(clampi(
      lava.temp - result.water_contacts * simcfg::kLavaWaterCoolPerNeighbor,
      simcfg::kAmbientTemp,
      simcfg::kLavaMaxTemp));
  return result;
}

bool try_solidify_lava(World& world, int x, int y, int water_contacts) {
  Cell& c = world.at(x, y);
  if (c.type != CellType::Lava) return false;

  const int adjacent_wall =
      count_adjacent_type(world, x, y, CellType::Wall) + count_adjacent_type(world, x, y, CellType::Stone);
  const int lava_neighbors = count_adjacent_type(world, x, y, CellType::Lava);
  const int cardinal_lava_neighbors = count_cardinal_adjacent_type(world, x, y, CellType::Lava);
  const int interface_score = thermal_sink_score_around(world, x, y, static_cast<int>(c.temp));
  const bool water_cooled = water_contacts > 0;
  const bool interface_cell = interface_score > 0 || water_cooled;
  const bool deep_interior = cardinal_lava_neighbors >= 4 && lava_neighbors >= 7 && !interface_cell;
  const bool cool_enough = c.temp <= simcfg::kLavaCrustTempThreshold;
  const bool nucleated_by_crust = adjacent_wall > 0 && c.temp <= simcfg::kLavaCrustPropagateTempThreshold;
  const bool passively_solidifies =
      c.temp <= (deep_interior ? simcfg::kLavaPassiveSolidifyTempThreshold - 35
                               : simcfg::kLavaPassiveSolidifyTempThreshold);

  if (!cool_enough && !nucleated_by_crust && !passively_solidifies) return false;

  std::uint32_t divisor = 0;
  if (water_cooled) {
    const bool very_cool = c.temp <= simcfg::kLavaFastCrustTempThreshold;
    const std::uint32_t contact_bonus = static_cast<std::uint32_t>(std::min(water_contacts, 3));
    const std::uint32_t cool_bonus = very_cool ? 2u : 0u;
    divisor = std::max<std::uint32_t>(1u, simcfg::kLavaCrustOddsDivisorBase - contact_bonus - cool_bonus);
  } else if (nucleated_by_crust) {
    divisor = std::max<std::uint32_t>(
        1u, simcfg::kLavaCrustPropagateOddsDivisor - static_cast<std::uint32_t>(std::min(adjacent_wall, 2)));
  } else {
    divisor = (adjacent_wall > 0) ? simcfg::kLavaPassiveSolidifyNearWallOddsDivisor
                                  : simcfg::kLavaPassiveSolidifyOddsDivisor;
  }

  // Strongly prefer solidification at cooling interfaces and resist boxed interior fill.
  if (interface_cell) {
    const std::uint32_t interface_bonus = static_cast<std::uint32_t>(std::min(interface_score, 4));
    divisor = std::max<std::uint32_t>(1u, divisor > interface_bonus ? divisor - interface_bonus : 1u);
  } else if (deep_interior) {
    divisor = std::max<std::uint32_t>(1u, divisor * 4u);
  } else {
    divisor = std::max<std::uint32_t>(1u, divisor * 2u);
  }

  if ((world.rng.next_u32() % divisor) != 0u) return false;

  c.type = CellType::Stone;
  c.temp = static_cast<std::int16_t>(std::max(simcfg::kAmbientTemp, static_cast<int>(c.temp) / 2));
  c.pressure = 0;

  // Grow crust inward from existing interfaces, but avoid "box fill" leaps into the core.
  int propagated = 0;
  for_each_neighbor(world, x, y, [&](int nx, int ny, Cell& n) {
    if (propagated >= 1) return;
    if (n.type != CellType::Lava) return;
    if (n.temp > simcfg::kLavaCrustPropagateTempThreshold) return;
    const int n_interface = thermal_sink_score_around(world, nx, ny, static_cast<int>(n.temp));
    const int n_cardinal_lava = count_cardinal_adjacent_type(world, nx, ny, CellType::Lava);
    if (n_interface == 0 && n_cardinal_lava >= 4) return;
    if ((world.rng.next_u32() % simcfg::kLavaCrustPropagateOddsDivisor) != 0u) return;
    n.type = CellType::Stone;
    n.temp = static_cast<std::int16_t>(std::max(simcfg::kAmbientTemp, static_cast<int>(n.temp) / 2));
    n.pressure = 0;
    n.updated = world.stamp;
    ++propagated;
  });
  return true;
}

}  // namespace

/**
 * @brief Dispatches per-material update logic for one active cell.
 *
 * Empty/wall cells are skipped. Cells already stamped in this frame are also
 * skipped to enforce single-update-per-tick semantics.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param left_to_right Horizontal traversal direction hint for bias control.
 */
void World::step_cell(int x, int y, bool left_to_right) {
  Cell& c = at(x, y);
  if (c.type == CellType::Empty || sim::material_behavior(c.type).static_obstacle) return;
  if (c.updated == stamp) return;

  c.updated = stamp;

  switch (c.type) {
    case CellType::Stone: step_stone(x, y, left_to_right); break;
    case CellType::Sand: step_sand(x, y, left_to_right); break;
    case CellType::Water: step_water(x, y, left_to_right); break;
    case CellType::Oil: step_oil(x, y, left_to_right); break;
    case CellType::Smoke: step_smoke(x, y, left_to_right); break;
    case CellType::Fire: step_fire(x, y); break;
    case CellType::Lava: step_lava(x, y, left_to_right); break;
    default: break;
  }
}

/**
 * @brief Sand rule: fall straight down, else slide diagonally down.
 *
 * Priority is down first, then one diagonal, then the opposite diagonal.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred diagonal order determined by frame sweep direction.
 */
void World::step_sand(int x, int y, bool ltr) {
  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  (void)try_move(x, y, x + dx2, y + 1);
}

void World::step_stone(int x, int y, bool ltr) {
  if (try_move(x, y, x, y + 1)) return;

  (void)ltr;
  const CellType below = at(x, y + 1).type;
  const bool liquid_below = (below == CellType::Water || below == CellType::Oil || below == CellType::Smoke);
  if ((rng.next_u32() % 3u) == 0u && try_swap_stone_with_fluid(*this, x, y, x, y + 1)) return;

  if (liquid_below) {
    // Stone displaces liquid by entering vertically; avoid side/diagonal "particleization" in liquid.
    (void)try_swap_stone_with_fluid(*this, x, y, x, y + 1);
    return;
  }

  // Dry-supported stone behaves as a cohesive rock mass approximation:
  // it does not keep avalanching into a sand triangle.
  return;
}

/**
 * @brief Water rule: absorb heat, evaporate if hot, then flow like a liquid.
 *
 * Water can travel horizontally up to 3 cells when blocked vertically.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred diagonal order determined by frame sweep direction.
 */
void World::step_water(int x, int y, bool ltr) {
  absorb_heat_from_hot_neighbors(*this, x, y);
  if (try_evaporate_water(*this, x, y)) return;

  Cell& self = at(x, y);
  const int ix = static_cast<int>(self.impulse_x);
  const int iy = static_cast<int>(self.impulse_y);
  const bool surface_like =
      in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty &&
      (!in_bounds(x, y - 2) || at(x, y - 2).type != CellType::Empty || std::abs(iy) >= 2);
  const bool droplet_like = (iy <= -3) && (std::abs(ix) >= 1);
  const bool airborne = in_bounds(x, y + 1) && at(x, y + 1).type == CellType::Empty;

  // Granular splash droplets: temporarily behave like tiny particles, not a connected liquid sheet.
  if (droplet_like) {
    const int updx = (ix > 0) ? 1 : -1;
    // Hard cap splash height: only launch while still near the surface (not already airborne).
    if (!airborne) {
      if (iy <= -4) {
        // Prefer diagonal-up to create discrete spray, then fallback to straight-up.
        if (try_move(x, y, x + updx, y - 1)) return;
        if (try_move(x, y, x, y - 1)) return;
      } else {
        if (try_move(x, y, x + updx, y - 1)) return;
        if (try_move(x, y, x, y - 1)) return;
      }
    }

    // If blocked near surface, quickly lose droplet identity and rejoin normal liquid behavior.
    self = at(x, y);
    if (self.type == CellType::Water) {
      // Fast decay: droplets arc briefly, then gravity wins and they merge back.
      self.impulse_y = static_cast<std::int8_t>(std::min<int>(self.impulse_y + (airborne ? 3 : 2), 0));
      // Kill sideways travel quickly so droplets arc and fall back instead of surfing to walls.
      self.impulse_x = static_cast<std::int8_t>(self.impulse_x / 3);
    }
    return;
  }

  // Impact response: upward impulse near a surface can create a quick splash rise.
  if (surface_like && droplet_like && in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty) {
    if (try_move(x, y, x, y - 1)) {
      Cell& moved = at(x, y - 1);
      if (moved.type == CellType::Water) {
        moved.impulse_y = -1;  // retain a little upward identity, but prevent repeated launch
        moved.impulse_x = static_cast<std::int8_t>((moved.impulse_x * 2) / 3);
      }
      return;
    }
  }
  if (surface_like && droplet_like) {
    const int updx = (ix > 0) ? 1 : (ix < 0 ? -1 : (ltr ? -1 : 1));
    if (try_move(x, y, x + updx, y - 1)) {
      Cell& moved = at(x + updx, y - 1);
      if (moved.type == CellType::Water) {
        moved.impulse_y = -1;
        moved.impulse_x = static_cast<std::int8_t>((moved.impulse_x * 2) / 3);
      }
      return;
    }
  }

  if (try_move(x, y, x, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x, y + 1)) return;

  const bool suppress_impulse_side_bias = airborne && iy < 0;
  const int dx1 = suppress_impulse_side_bias ? (ltr ? -1 : 1)
                                             : (ix > 0) ? 1 : (ix < 0 ? -1 : (ltr ? -1 : 1));
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx2, y + 1)) return;

  const int dir = suppress_impulse_side_bias ? choose_liquid_spread_dir(*this, x, y, ltr)
                                             : (ix > 0) ? 1 : (ix < 0 ? -1 : choose_liquid_spread_dir(*this, x, y, ltr));

  const int max_spread = simcfg::kWaterSpread + liquid_pressure_spread_bonus(*this, x, y, ltr);
  for (int i = 1; i <= max_spread; ++i) {
    if (try_move(x, y, x + dir * i, y)) return;
  }

  // Decay impulses if no movement consumed them this tick.
  self = at(x, y);
  if (self.type == CellType::Water) {
    if (self.impulse_x > 0) --self.impulse_x;
    else if (self.impulse_x < 0) ++self.impulse_x;
    if (self.impulse_y > 0) self.impulse_y = static_cast<std::int8_t>(self.impulse_y - 1);
    else if (self.impulse_y < 0) self.impulse_y = static_cast<std::int8_t>(self.impulse_y + 2);
  }
}

/**
 * @brief Oil rule: similar to water but with wider lateral spread.
 *
 * Oil can travel horizontally up to 4 cells when blocked from falling.
 * It is also flammable via fire/lava interaction handled in hot material rules.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred diagonal order determined by frame sweep direction.
 */
void World::step_oil(int x, int y, bool ltr) {
  if (try_move(x, y, x, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx2, y + 1)) return;

  const int dir = choose_liquid_spread_dir(*this, x, y, ltr);

  const int max_spread = simcfg::kOilSpread + liquid_pressure_spread_bonus(*this, x, y, ltr);
  for (int i = 1; i <= max_spread; ++i) {
    if (try_move(x, y, x + dir * i, y)) return;
  }
}

/**
 * @brief Smoke rule: rise, bubble through water/oil, then drift sideways.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred horizontal order determined by frame sweep direction.
 */
void World::step_smoke(int x, int y, bool ltr) {
  const int pressure = at(x, y).pressure;

  if (try_move(x, y, x, y - 1)) return;
  if (try_swap_smoke_with_fluid(*this, x, y, x, y - 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y - 1)) return;
  if (try_swap_smoke_with_fluid(*this, x, y, x + dx1, y - 1)) return;
  if (try_move(x, y, x + dx2, y - 1)) return;
  if (try_swap_smoke_with_fluid(*this, x, y, x + dx2, y - 1)) return;

  if (pressure >= simcfg::kHighSmokePressure) {
    if (try_move(x, y, x + dx1, y)) return;
    if (try_move(x, y, x + dx2, y)) return;
  }

  if (rng.coin()) {
    if (try_move(x, y, x + dx1, y)) return;
    (void)try_move(x, y, x + dx2, y);
  } else {
    if (try_move(x, y, x + dx2, y)) return;
    (void)try_move(x, y, x + dx1, y);
  }

  Cell& c = at(x, y);
  if (c.pressure >= simcfg::kExtremeSmokePressure &&
      c.temp <= simcfg::kSmokeCondenseTempThreshold &&
      !smoke_has_escape_route(*this, x, y) &&
      (rng.next_u32() % simcfg::kSmokeCondenseOddsDivisor) == 0u) {
    c.temp = simcfg::kAmbientTemp;
    c.pressure = 0;
    c.load = 0;
  }
}

/**
 * @brief Adds heat to all valid Moore-neighborhood cells around a source.
 *
 * Empty cells and walls are excluded. Temperature is clamped to a safe range.
 *
 * @param x Source X.
 * @param y Source Y.
 * @param amount Heat delta to add per affected neighbor.
 */
void World::heat_neighbors(int x, int y, int amount) {
  const Cell& src = at(x, y);
  const int source_temp = static_cast<int>(src.temp);
  const int head = std::max(0, source_temp - simcfg::kAmbientTemp);
  // Scale emitted heat by source temperature so cooling hot materials lose heating power.
  const int scaled_amount = (head <= 0)
                                ? 0
                                : std::max(1, (amount * std::min(head, simcfg::kMaxNeighborHeatTemp)) /
                                                 std::max(1, simcfg::kMaxNeighborHeatTemp));
  if (scaled_amount <= 0) return;

  for_each_neighbor(*this, x, y, [&](int nx, int ny, Cell& n) {
    if (n.type == CellType::Wall || n.type == CellType::Empty) return;

    if (n.type == src.type) return;

    // Rule-emitted heat should only raise cooler neighbors and should not exceed the source temp.
    const int max_target = std::max(simcfg::kAmbientTemp, source_temp - 1);
    if (n.temp >= max_target) return;

    const int delta = std::min<int>(scaled_amount, max_target - static_cast<int>(n.temp));
    if (delta <= 0) return;
    add_thermal_impulse(nx, ny, delta);
  });
}

/**
 * @brief Fire rule: self-heats, warms neighbors, can be quenched by water, then decays.
 *
 * Behavior summary:
 * - Increases own temperature (capped).
 * - Adds mild heat to adjacent cells.
 * - Has a random chance to ignite neighboring oil into fire.
 * - Loses heat quickly when adjacent to water and can extinguish when cooled.
 * - Randomly decays into smoke or disappears entirely.
 *
 * @param x Cell X.
 * @param y Cell Y.
 */
void World::step_fire(int x, int y) {
  Cell& c = at(x, y);
  heat_neighbors(x, y, simcfg::kFireNeighborHeatPerTick);
  ignite_oil_neighbors(*this, x, y);
  quench_fire_from_water(*this, x, y);

  if (c.temp < simcfg::kFireMinSustainTemp) {
    c.type = CellType::Smoke;
    c.temp = simcfg::kSmokeFromFireTemp;
    return;
  }

  const std::uint32_t r = rng.next_u32();
  if ((r % simcfg::kFireToSmokeOddsDivisor) == 0u) {
    c.type = CellType::Smoke;
    c.temp = simcfg::kSmokeFromFireTemp;
    return;
  }
  if ((r % simcfg::kFireExtinguishOddsDivisor) == 0u) {
    c.type = CellType::Smoke;
    c.temp = simcfg::kAmbientTemp;
    c.pressure = 0;
    return;
  }
}

/**
 * @brief Lava rule: hot flowing liquid that can ignite and emit smoke.
 *
 * Behavior summary:
 * - Self-heats and strongly warms nearby cells.
 * - Has a random chance to ignite neighboring oil into fire.
 * - Can cool and crust into stone solid under sustained water contact.
 * - Tries to fall/slide similarly to dense liquid.
 * - Occasionally spawns smoke above when space is available.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred lateral order determined by frame sweep direction.
 */
void World::step_lava(int x, int y, bool ltr) {
  heat_neighbors(x, y, simcfg::kLavaNeighborHeatPerTick);
  ignite_oil_neighbors(*this, x, y);
  const LavaContactResult contact = trigger_lava_water_contact(*this, x, y);
  cool_lava_through_crust(*this, x, y);
  if (try_solidify_lava(*this, x, y, contact.water_contacts)) return;

  if (try_move(x, y, x, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;
  if (try_swap_liquid_with_smoke(*this, x, y, x + dx2, y + 1)) return;

  Cell& lava = at(x, y);
  const int lava_temp = static_cast<int>(lava.temp);
  const CellType below = at(x, y + 1).type;
  const bool supported = (below != CellType::Empty && below != CellType::Smoke);
  const bool exposed_surface = in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty;

  if (supported &&
      !exposed_surface &&
      lava_temp < simcfg::kLavaSupportedFlowTempThreshold &&
      lava.pressure < simcfg::kLavaPressureFlowThreshold) {
    if (lava.flow_strength > 0) --lava.flow_strength;
    // Pooled/cooling lava behaves more like a yield fluid than a diffusing liquid.
    if (lava.flow_strength == 0) lava.flow_dir = 0;
  } else {
    std::uint32_t lateral_divisor = simcfg::kLavaLateralOddsCool;
    if (lava_temp >= simcfg::kLavaLateralFlowVeryHighTemp) lateral_divisor = simcfg::kLavaLateralOddsVeryHot;
    else if (lava_temp >= simcfg::kLavaLateralFlowHighTemp) lateral_divisor = simcfg::kLavaLateralOddsHot;
    else if (lava_temp >= simcfg::kLavaLateralFlowMidTemp) lateral_divisor = simcfg::kLavaLateralOddsWarm;

    // Cooler lava should not laterally diffuse every tick; hot lava remains mobile.
    if (lava_temp >= simcfg::kLavaLateralFlowLowTemp &&
        (lateral_divisor <= 1u || (rng.next_u32() % lateral_divisor) == 0u)) {
      const bool very_hot = lava_temp >= simcfg::kLavaLateralFlowVeryHighTemp;
      const bool momentum_active = lava.flow_strength >= 2 && lava_temp >= simcfg::kLavaMomentumAssistTemp;
      auto lateral_drop_score = [&](int dir) {
        int score = 0;
        const int nx = x + dir;
        if (!in_bounds(nx, y)) return -999;
        const CellType side = at(nx, y).type;
        const CellType side_down = at(nx, y + 1).type;
        if (side == CellType::Empty) score += 2;
        else if (side == CellType::Smoke) score += 1;
        else score -= 2;
        if (side_down == CellType::Empty) score += 4;
        else if (side_down == CellType::Smoke) score += 2;

        if (very_hot && in_bounds(x + 2 * dir, y + 1)) {
          const CellType far_down = at(x + 2 * dir, y + 1).type;
          if (far_down == CellType::Empty) score += 2;
        }
        return score;
      };

      int preferred_dir = (lava.flow_dir > 0) ? 1 : (lava.flow_dir < 0 ? -1 : 0);
      const int score1 = lateral_drop_score(dx1);
      const int score2 = lateral_drop_score(dx2);
      if (score1 != score2) preferred_dir = (score1 > score2) ? dx1 : dx2;
      if (preferred_dir == 0) preferred_dir = rng.coin() ? dx1 : dx2;
      const int first = preferred_dir;
      const int second = -first;

      if (try_move(x, y, x + first, y)) return;
      if (momentum_active) {
        if (try_move(x, y, x + first, y + 1)) return;
        if (try_swap_liquid_with_smoke(*this, x, y, x + first, y + 1)) return;
      }
      if (very_hot) {
        if (try_move(x, y, x + 2 * first, y)) return;
      }
      if (!try_move(x, y, x + second, y)) {
        // If supported and blocked sideways, cool lava loses flow memory faster.
        if (lava_temp < simcfg::kLavaLateralFlowMidTemp || lava.flow_strength <= 1) {
          lava.flow_dir = 0;
          lava.flow_strength = 0;
        } else {
          --lava.flow_strength;
        }
      }
    }
  }

  if ((rng.next_u32() % simcfg::kLavaSmokeSpawnOddsDivisor) == 0u) {
    if (in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty) {
      at(x, y - 1).type = CellType::Smoke;
      at(x, y - 1).temp = simcfg::kSmokeFromLavaTemp;
      at(x, y - 1).updated = stamp;
    }
  }
}
