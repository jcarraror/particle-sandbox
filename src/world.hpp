#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "rng.hpp"

enum class CellType : std::uint8_t {
  Empty = 0,
  Wall,
  Sand,
  Water,
  Oil,
  Fire,
  Smoke,
  Lava
};

struct Cell {
  CellType type{CellType::Empty};
  std::uint8_t updated{0};   // per-tick stamp
  std::int16_t temp{20};     // simple temperature
};

// Tiny 2D view (fast + portable)
template <class T>
struct Grid2D {
  T* data{};
  int w{};
  int h{};

  constexpr T& operator()(int x, int y) noexcept { return data[y * w + x]; }
  constexpr const T& operator()(int x, int y) const noexcept { return data[y * w + x]; }
};

struct World {
  int w = 0;
  int h = 0;

  std::vector<Cell> cells;
  std::uint8_t stamp = 1;

  XorShift32 rng;

  static std::expected<World, std::string> create(int width, int height, std::uint32_t seed);

  Grid2D<Cell> grid();
  Grid2D<const Cell> grid() const;

  void clear();
  void tick();
  void paint_disc(int cx, int cy, int radius, CellType t);

  bool in_bounds(int x, int y) const;
  Cell& at(int x, int y);
  const Cell& at(int x, int y) const;

private:
  void step_cell(int x, int y, bool left_to_right);

  bool try_move(int x, int y, int nx, int ny);
  bool is_empty(int x, int y) const;

  void step_sand(int x, int y, bool ltr);
  void step_water(int x, int y, bool ltr);
  void step_oil(int x, int y, bool ltr);
  void step_smoke(int x, int y, bool ltr);
  void step_fire(int x, int y);
  void step_lava(int x, int y, bool ltr);

  void heat_neighbors(int x, int y, int amount);
};