/**
 * @file world_rules.cpp
 * @brief World simulation rule dispatch and per-material behavior implementation.
 */

#include "world.hpp"

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
constexpr int kFireMaxTemp = 1200;
constexpr std::uint32_t kFireToSmokeOddsDivisor = 25;
constexpr std::uint32_t kFireExtinguishOddsDivisor = 120;
constexpr std::int16_t kSmokeFromFireTemp = 80;

constexpr int kLavaSelfHeatPerTick = 2;
constexpr int kLavaNeighborHeatPerTick = 6;
constexpr int kLavaMaxTemp = 2000;
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
 * @brief Water rule: fall down, then diagonals, then short lateral spread.
 *
 * Water can travel horizontally up to 3 cells when blocked vertically.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred diagonal order determined by frame sweep direction.
 */
void World::step_water(int x, int y, bool ltr) {
  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;

  const int dir = (rng.coin() ? 1 : -1);

  for (int i = 1; i <= simcfg::kWaterSpread; ++i) {
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

  const int dir = (rng.coin() ? 1 : -1);

  for (int i = 1; i <= simcfg::kOilSpread; ++i) {
    if (try_move(x, y, x + dir * i, y)) return;
  }
}

/**
 * @brief Smoke rule: rise upward, then diagonals upward, then drift sideways.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred horizontal order determined by frame sweep direction.
 */
void World::step_smoke(int x, int y, bool ltr) {
  if (try_move(x, y, x, y - 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y - 1)) return;
  if (try_move(x, y, x + dx2, y - 1)) return;

  if (rng.coin()) (void)try_move(x, y, x + dx1, y);
  else (void)try_move(x, y, x + dx2, y);
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
 * @brief Fire rule: self-heats, warms neighbors, ignites oil, then decays.
 *
 * Behavior summary:
 * - Increases own temperature (capped).
 * - Adds mild heat to adjacent cells.
 * - Has a random chance to ignite neighboring oil into fire.
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
