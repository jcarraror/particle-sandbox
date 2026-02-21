#include "world.hpp"

#include <algorithm>
#include <cmath>

static constexpr int clampi(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

std::expected<World, std::string> World::create(int width, int height, std::uint32_t seed) {
  if (width <= 0 || height <= 0) return std::unexpected("World size must be positive.");

  World wld;
  wld.w = width;
  wld.h = height;
  wld.cells.assign(static_cast<std::size_t>(width * height), Cell{});
  wld.rng = XorShift32(seed);

  // Border walls
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

Grid2D<Cell> World::grid() {
  return Grid2D<Cell>{cells.data(), w, h};
}

Grid2D<const Cell> World::grid() const {
  return Grid2D<const Cell>{cells.data(), w, h};
}

bool World::in_bounds(int x, int y) const {
  return (x >= 0 && x < w && y >= 0 && y < h);
}

Cell& World::at(int x, int y) {
  return cells[static_cast<std::size_t>(y * w + x)];
}

const Cell& World::at(int x, int y) const {
  return cells[static_cast<std::size_t>(y * w + x)];
}

bool World::is_empty(int x, int y) const {
  return at(x, y).type == CellType::Empty;
}

bool World::try_move(int x, int y, int nx, int ny) {
  if (!in_bounds(nx, ny)) return false;

  Cell& a = at(x, y);
  Cell& b = at(nx, ny);

  if (b.type != CellType::Empty) return false;

  std::swap(a, b);

  // The moved-into cell should be considered updated for this stamp
  b.updated = stamp;
  return true;
}

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

void World::tick() {
  stamp = static_cast<std::uint8_t>(stamp + 1);
  if (stamp == 0) stamp = 1;

  const bool left_to_right = (rng.next_u32() & 1u) != 0;

  // Falling behavior: scan bottom-up
  for (int y = h - 2; y >= 1; --y) {
    if (left_to_right) {
      for (int x = 1; x < w - 1; ++x) step_cell(x, y, true);
    } else {
      for (int x = w - 2; x >= 1; --x) step_cell(x, y, false);
    }
  }

  // Cooling toward ambient (simple)
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);
      if (c.type == CellType::Empty || c.type == CellType::Wall) continue;
      if (c.temp > 20) c.temp -= 1;
      else if (c.temp < 20) c.temp += 1;
    }
  }
}

void World::step_cell(int x, int y, bool left_to_right) {
  Cell& c = at(x, y);
  if (c.type == CellType::Empty || c.type == CellType::Wall) return;
  if (c.updated == stamp) return;

  c.updated = stamp;

  switch (c.type) {
    case CellType::Sand:  step_sand(x, y, left_to_right); break;
    case CellType::Water: step_water(x, y, left_to_right); break;
    case CellType::Oil:   step_oil(x, y, left_to_right); break;
    case CellType::Smoke: step_smoke(x, y, left_to_right); break;
    case CellType::Fire:  step_fire(x, y); break;
    case CellType::Lava:  step_lava(x, y, left_to_right); break;
    default: break;
  }
}

void World::step_sand(int x, int y, bool ltr) {
  if (try_move(x, y, x, y + 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y + 1)) return;
  (void)try_move(x, y, x + dx2, y + 1);
}

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

void World::step_smoke(int x, int y, bool ltr) {
  if (try_move(x, y, x, y - 1)) return;

  const int dx1 = (ltr ? -1 : 1);
  const int dx2 = -dx1;

  if (try_move(x, y, x + dx1, y - 1)) return;
  if (try_move(x, y, x + dx2, y - 1)) return;

  if (rng.coin()) (void)try_move(x, y, x + dx1, y);
  else (void)try_move(x, y, x + dx2, y);
}

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

void World::step_fire(int x, int y) {
  Cell& c = at(x, y);
  c.temp = static_cast<std::int16_t>(clampi(c.temp + 5, 20, 1200));
  heat_neighbors(x, y, 3);

  // Ignite oil neighbors
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

  // Decay
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