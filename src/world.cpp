/**
 * @file world.cpp
 * @brief World storage, lifecycle, and tick orchestration implementation.
 */

#include "world.hpp"

#include <algorithm>
#include <cmath>

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
  return Grid2D<Cell>{std::span<Cell>{cells}, w, h};
}

/**
 * @brief Creates a const 2D view over the internal cell buffer.
 * @return Read-only row-major grid view.
 */
Grid2D<const Cell> World::grid() const {
  return Grid2D<const Cell>{std::span<const Cell>{cells}, w, h};
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
