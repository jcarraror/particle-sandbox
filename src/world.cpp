/**
 * @file world.cpp
 * @brief Simulation rules and world update implementation.
 */

#include "world.hpp"

#include <algorithm>
#include <cmath>

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

/**
 * @brief Constructs a world and initializes immutable border walls.
 * @param width World width in cells.
 * @param height World height in cells.
 * @param seed PRNG seed used for stochastic updates.
 * @return A ready-to-simulate world or an error when dimensions are invalid.
 */
std::expected<World, std::string> World::create(int width, int height, std::uint32_t seed) {
  if (width <= 0 || height <= 0) return std::unexpected("World size must be positive.");

  World wld;
  wld.w = width;
  wld.h = height;
  wld.cells.assign(static_cast<std::size_t>(width * height), Cell{});
  wld.rng = XorShift32(seed);

  for (int x = 0; x < width; ++x) {
    wld.at(x, 0).type = CellType::Wall;
    wld.at(x, height - 1).type = CellType::Wall;
  }
  for (int y = 0; y < height; ++y) {
    wld.at(0, y).type = CellType::Wall;
    wld.at(width - 1, y).type = CellType::Wall;
  }

  return wld;
}

/**
 * @brief Creates a mutable 2D view over the internal cell buffer.
 * @return Mutable row-major grid view.
 */
Grid2D<Cell> World::grid() {
  return Grid2D<Cell>{cells.data(), w, h};
}

/**
 * @brief Creates a const 2D view over the internal cell buffer.
 * @return Read-only row-major grid view.
 */
Grid2D<const Cell> World::grid() const {
  return Grid2D<const Cell>{cells.data(), w, h};
}

/**
 * @brief Checks if a coordinate belongs to the simulation domain.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return `true` when the coordinate is valid.
 */
bool World::in_bounds(int x, int y) const {
  return (x >= 0 && x < w && y >= 0 && y < h);
}

/**
 * @brief Returns mutable access to a cell by coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return Mutable reference to the selected cell.
 */
Cell& World::at(int x, int y) {
  return cells[static_cast<std::size_t>(y * w + x)];
}

/**
 * @brief Returns const access to a cell by coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return Const reference to the selected cell.
 */
const Cell& World::at(int x, int y) const {
  return cells[static_cast<std::size_t>(y * w + x)];
}

/**
 * @brief Tests whether a coordinate currently contains empty space.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return `true` if cell type is `CellType::Empty`.
 */
bool World::is_empty(int x, int y) const {
  return at(x, y).type == CellType::Empty;
}

/**
 * @brief Moves one cell into a destination if that destination is empty.
 *
 * This is the used by all element step rules. When a move succeeds,
 * the moved cell is stamped as updated for the current tick to prevent
 * multiple moves in the same frame.
 *
 * @param x Source X.
 * @param y Source Y.
 * @param nx Destination X.
 * @param ny Destination Y.
 * @return `true` if movement occurred.
 */
bool World::try_move(int x, int y, int nx, int ny) {
  if (!in_bounds(nx, ny)) return false;

  Cell& a = at(x, y);
  Cell& b = at(nx, ny);

  if (b.type != CellType::Empty) return false;

  std::swap(a, b);

  b.updated = stamp;
  return true;
}

/**
 * @brief Resets the world to empty state and restores border walls.
 *
 * All interior cells become default
 */
void World::clear() {
  for (auto& c : cells) c = Cell{};

  for (int x = 0; x < w; ++x) {
    at(x, 0).type = CellType::Wall;
    at(x, h - 1).type = CellType::Wall;
  }
  for (int y = 0; y < h; ++y) {
    at(0, y).type = CellType::Wall;
    at(w - 1, y).type = CellType::Wall;
  }
}

/**
 * @brief Paints a filled circular brush of material.
 *
 * Walls are preserved and never overwritten. Fire and lava are spawned with
 * elevated temperatures; other materials are initialized near ambient.
 *
 * @param cx Brush center X.
 * @param cy Brush center Y.
 * @param radius Brush radius in cells.
 * @param t Material to apply.
 */
void World::paint_disc(int cx, int cy, int radius, CellType t) {
  const int r2 = radius * radius;

  for (int y = cy - radius; y <= cy + radius; ++y) {
    for (int x = cx - radius; x <= cx + radius; ++x) {
      if (!in_bounds(x, y)) continue;
      const int dx = x - cx;
      const int dy = y - cy;
      if (dx * dx + dy * dy > r2) continue;

      Cell& c = at(x, y);
      if (c.type == CellType::Wall) continue;

      c.type = t;
      c.updated = stamp;

      if (t == CellType::Fire) c.temp = 300;
      else if (t == CellType::Lava) c.temp = 800;
      else c.temp = 20;
    }
  }
}

/**
 * @brief Advances simulation by one frame.
 *
 * Update order:
 * - Increment update stamp.
 * - Sweep bottom-up so falling materials resolve naturally.
 * - Randomize horizontal sweep direction per frame to reduce directional bias.
 * - Apply simple global cooling/heating back toward ambient temperature.
 */
void World::tick() {
  stamp = static_cast<std::uint8_t>(stamp + 1);
  if (stamp == 0) stamp = 1;

  const bool left_to_right = (rng.next_u32() & 1u) != 0;

  for (int y = h - 2; y >= 1; --y) {
    if (left_to_right) {
      for (int x = 1; x < w - 1; ++x) step_cell(x, y, true);
    } else {
      for (int x = w - 2; x >= 1; --x) step_cell(x, y, false);
    }
  }

  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);
      if (c.type == CellType::Empty || c.type == CellType::Wall) continue;
      if (c.temp > 20) c.temp -= 1;
      else if (c.temp < 20) c.temp += 1;
    }
  }
}

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

  const int spread = 3;
  const int dir = (rng.coin() ? 1 : -1);

  for (int i = 1; i <= spread; ++i) {
    if (try_move(x, y, x + dir * i, y)) return;
  }
}

/**
 * @brief Oil rule: similar to water but with wider lateral spread.
 *
 * Oil can travel horizontally up to 4 cells when blocked from falling.
 * It is also flammable via fire interaction handled in `step_fire`.
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

  const int spread = 4;
  const int dir = (rng.coin() ? 1 : -1);

  for (int i = 1; i <= spread; ++i) {
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
  for (int oy = -1; oy <= 1; ++oy) {
    for (int ox = -1; ox <= 1; ++ox) {
      if (ox == 0 && oy == 0) continue;
      const int nx = x + ox;
      const int ny = y + oy;
      if (!in_bounds(nx, ny)) continue;

      Cell& n = at(nx, ny);
      if (n.type == CellType::Wall || n.type == CellType::Empty) continue;

      n.temp = static_cast<std::int16_t>(clampi(n.temp + amount, -50, 2000));
    }
  }
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
  c.temp = static_cast<std::int16_t>(clampi(c.temp + 5, 20, 1200));
  heat_neighbors(x, y, 3);

  for (int oy = -1; oy <= 1; ++oy) {
    for (int ox = -1; ox <= 1; ++ox) {
      const int nx = x + ox;
      const int ny = y + oy;
      if (!in_bounds(nx, ny)) continue;

      Cell& n = at(nx, ny);
      if (n.type == CellType::Oil && (rng.next_u32() % 7u == 0u)) {
        n.type = CellType::Fire;
        n.temp = 300;
        n.updated = stamp;
      }
    }
  }

  const std::uint32_t r = rng.next_u32();
  if ((r % 25u) == 0u) {
    c.type = CellType::Smoke;
    c.temp = 80;
    return;
  }
  if ((r % 120u) == 0u) {
    c.type = CellType::Empty;
    c.temp = 20;
    return;
  }
}

/**
 * @brief Lava rule: just like a liquid that flows and emits occasional smoke.
 *
 * Behavior summary:
 * - Self-heats and strongly warms nearby cells.
 * - Tries to fall/slide similarly to dense liquid.
 * - Occasionally spawns smoke above when space is available.
 *
 * @param x Cell X.
 * @param y Cell Y.
 * @param ltr Preferred lateral order determined by frame sweep direction.
 */
void World::step_lava(int x, int y, bool ltr) {
  Cell& c = at(x, y);
  c.temp = static_cast<std::int16_t>(clampi(c.temp + 2, 20, 2000));
  heat_neighbors(x, y, 6);

  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  if (try_move(x, y, x + dx2, y + 1)) return;

  if (rng.coin()) (void)try_move(x, y, x + dx1, y);
  else (void)try_move(x, y, x + dx2, y);

  if ((rng.next_u32() % 80u) == 0u) {
    if (in_bounds(x, y - 1) && at(x, y - 1).type == CellType::Empty) {
      at(x, y - 1).type = CellType::Smoke;
      at(x, y - 1).temp = 120;
      at(x, y - 1).updated = stamp;
    }
  }
}
