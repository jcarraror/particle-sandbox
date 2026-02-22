/**
 * @file world_rules.cpp
 * @brief World simulation rule dispatch and per-material behavior implementation.
 */

#include "world.hpp"

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

namespace simcfg {
constexpr int kAmbientTemp = 20;
constexpr int kMinCellTemp = -50;
constexpr int kMaxNeighborHeatTemp = 2000;

constexpr std::uint32_t kOilIgniteOddsDivisor = 7;
constexpr std::int16_t kIgnitedFireTemp = 300;

constexpr int kWaterSpread = 3;
constexpr int kOilSpread = 4;

constexpr int kFireSelfHeatPerTick = 5;
constexpr int kFireNeighborHeatPerTick = 3;
constexpr int kFireWaterQuenchPerNeighbor = 22;
constexpr int kFireMinSustainTemp = 110;
constexpr int kFireMaxTemp = 1200;
constexpr std::uint32_t kFireToSmokeOddsDivisor = 25;
constexpr std::uint32_t kFireExtinguishOddsDivisor = 120;
constexpr std::int16_t kSmokeFromFireTemp = 80;

constexpr int kLavaSelfHeatPerTick = 2;
constexpr int kLavaNeighborHeatPerTick = 6;
constexpr int kLavaMaxTemp = 2000;
constexpr int kLavaWaterCoolPerNeighbor = 28;
constexpr int kLavaWaterPressureSpike = 70;
constexpr std::uint32_t kLavaWaterSteamBurstOddsDivisor = 2;
constexpr int kLavaSteamBurstTempThreshold = 110;
constexpr int kLavaSteamSpawnPressure = 180;
constexpr int kWaterHeatAbsorbFromFire = 16;
constexpr int kWaterHeatAbsorbFromLava = 10;
constexpr int kWaterSteamTempThreshold = 140;
constexpr std::uint32_t kWaterSteamOddsDivisor = 5;
constexpr std::int16_t kSteamTemp = 130;
constexpr std::uint32_t kSmokeBubbleSwapOddsDivisor = 2;
constexpr std::uint32_t kSteamBubbleSwapOddsDivisor = 1;
constexpr int kSteamSmokeTempThreshold = 120;
constexpr int kHighSmokePressure = 80;
constexpr int kExtremeSmokePressure = 150;
constexpr int kSmokeCondenseTempThreshold = 55;
constexpr std::uint32_t kSmokeCondenseOddsDivisor = 8;
constexpr int kLiquidPressureActivationThreshold = 90;
constexpr int kLiquidPressureSpreadBonusStep = 45;
constexpr int kLiquidMaxSpreadBonus = 3;
constexpr std::uint32_t kLavaSmokeSpawnOddsDivisor = 80;
constexpr std::int16_t kSmokeFromLavaTemp = 120;
}  // namespace simcfg

struct NeighborOffset {
  int dx;
  int dy;
};

constexpr std::array<NeighborOffset, 8> kMooreOffsets{{
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1},
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

  bool near_hot = false;
  for_each_neighbor(world, x, y, [&](int, int, Cell& n) {
    if (n.type == CellType::Fire || n.type == CellType::Lava) near_hot = true;
  });
  if (!near_hot) return false;

  const int overheat = std::max(0, static_cast<int>(c.temp) - simcfg::kWaterSteamTempThreshold);
  const std::uint32_t bonus = static_cast<std::uint32_t>(overheat / 40);
  const std::uint32_t divisor =
      std::max<std::uint32_t>(1u, simcfg::kWaterSteamOddsDivisor - std::min<std::uint32_t>(3u, bonus));
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
  if (dst.type != CellType::Water && dst.type != CellType::Oil) return false;

  const bool steam_like = src.temp >= simcfg::kSteamSmokeTempThreshold;
  std::uint32_t divisor =
      steam_like ? simcfg::kSteamBubbleSwapOddsDivisor : simcfg::kSmokeBubbleSwapOddsDivisor;
  if (src.pressure >= simcfg::kHighSmokePressure && divisor > 1u) divisor -= 1u;
  if (divisor > 1u && (world.rng.next_u32() % divisor) != 0u) return false;

  std::swap(src, dst);
  world.at(nx, ny).updated = world.stamp;
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
    if (t == CellType::Empty || t == CellType::Smoke) return true;
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

void trigger_lava_water_contact(World& world, int x, int y) {
  Cell& lava = world.at(x, y);
  int contacts = 0;

  for_each_neighbor(world, x, y, [&](int nx, int ny, Cell& n) {
    if (n.type != CellType::Water) return;
    ++contacts;

    n.temp = static_cast<std::int16_t>(clampi(
        n.temp + simcfg::kWaterHeatAbsorbFromLava + simcfg::kWaterHeatAbsorbFromFire,
        simcfg::kAmbientTemp,
        simcfg::kMaxNeighborHeatTemp));
    n.pressure = static_cast<std::int16_t>(clampi(
        static_cast<int>(n.pressure) + simcfg::kLavaWaterPressureSpike, 0, 240));

    const bool hot_enough = n.temp >= simcfg::kLavaSteamBurstTempThreshold;
    const bool burst = (world.rng.next_u32() % simcfg::kLavaWaterSteamBurstOddsDivisor) == 0u;
    if (!hot_enough || !burst) return;

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

  if (contacts == 0) return;
  lava.temp = static_cast<std::int16_t>(clampi(
      lava.temp - contacts * simcfg::kLavaWaterCoolPerNeighbor,
      simcfg::kAmbientTemp,
      simcfg::kLavaMaxTemp));
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
  if (c.type == CellType::Empty || c.type == CellType::Wall) return;
  if (c.updated == stamp) return;

  c.updated = stamp;

  switch (c.type) {
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

  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;

  const int dir = choose_liquid_spread_dir(*this, x, y, ltr);

  const int max_spread = simcfg::kWaterSpread + liquid_pressure_spread_bonus(*this, x, y, ltr);
  for (int i = 1; i <= max_spread; ++i) {
    if (try_move(x, y, x + dir * i, y)) return;
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

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;

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
    c.type = CellType::Empty;
    c.temp = simcfg::kAmbientTemp;
    c.pressure = 0;
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
  for_each_neighbor(*this, x, y, [&](int, int, Cell& n) {
    if (n.type == CellType::Wall || n.type == CellType::Empty) return;
    n.temp = static_cast<std::int16_t>(
        clampi(n.temp + amount, simcfg::kMinCellTemp, simcfg::kMaxNeighborHeatTemp));
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
  c.temp = static_cast<std::int16_t>(
      clampi(c.temp + simcfg::kFireSelfHeatPerTick, simcfg::kAmbientTemp, simcfg::kFireMaxTemp));
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
    c.type = CellType::Empty;
    c.temp = simcfg::kAmbientTemp;
    return;
  }
}

/**
 * @brief Lava rule: hot flowing liquid that can ignite and emit smoke.
 *
 * Behavior summary:
 * - Self-heats and strongly warms nearby cells.
 * - Has a random chance to ignite neighboring oil into fire.
 * - Tries to fall/slide similarly to dense liquid.
 * - Occasionally spawns smoke above when space is available.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred lateral order determined by frame sweep direction.
 */
void World::step_lava(int x, int y, bool ltr) {
  Cell& c = at(x, y);
  c.temp = static_cast<std::int16_t>(
      clampi(c.temp + simcfg::kLavaSelfHeatPerTick, simcfg::kAmbientTemp, simcfg::kLavaMaxTemp));
  heat_neighbors(x, y, simcfg::kLavaNeighborHeatPerTick);
  ignite_oil_neighbors(*this, x, y);
  trigger_lava_water_contact(*this, x, y);

  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;

  if (rng.coin()) (void)try_move(x, y, x + dx1, y);
  else (void)try_move(x, y, x + dx2, y);

  if ((rng.next_u32() % simcfg::kLavaSmokeSpawnOddsDivisor) == 0u) {
    if (in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty) {
      at(x, y - 1).type = CellType::Smoke;
      at(x, y - 1).temp = simcfg::kSmokeFromLavaTemp;
      at(x, y - 1).updated = stamp;
    }
  }
}
