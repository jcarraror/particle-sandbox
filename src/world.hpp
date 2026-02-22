#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "rng.hpp"

/**
 * @file world.hpp
 * @brief Core simulation types and world update API.
 */

/**
 * @brief Material kind stored in each world cell.
 */
enum class CellType : std::uint8_t {
  Empty = 0, /**< Empty space. */
  Wall,      /**< Static barrier cell. */
  Sand,      /**< Granular falling solid. */
  Water,     /**< Liquid with short horizontal spread. */
  Oil,       /**< Flammable liquid with wider spread. */
  Fire,      /**< Hot transient combustion cell. */
  Smoke,     /**< Light gas that rises. */
  Lava       /**< Dense hot liquid that emits smoke. */
};

/**
 * @brief Simulation state for a single grid cell.
 */
struct Cell {
  CellType type{CellType::Empty}; /**< Current material type. */
  std::uint8_t updated{0};        /**< Tick stamp used to prevent double-stepping. */
  std::int16_t temp{20};          /**< Approximate temperature in arbitrary units. */
  std::int16_t pressure{0};       /**< Local pressure heuristic field. */
};

/**
 * @brief Lightweight 2D view over contiguous storage.
 * @tparam T Element type.
 */
template <class T>
struct Grid2D {
  std::span<T> cells{}; /**< Row-major storage view. */
  int w{};              /**< Grid width in cells. */
  int h{};              /**< Grid height in cells. */

  /**
   * @brief Mutable element access.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return Reference to element at `(x, y)`.
   */
  constexpr T& operator()(int x, int y) noexcept { return cells[static_cast<std::size_t>(y * w + x)]; }

  /**
   * @brief Const element access.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return Const reference to element at `(x, y)`.
   */
  constexpr const T& operator()(int x, int y) const noexcept {
    return cells[static_cast<std::size_t>(y * w + x)];
  }
};

/**
 * @brief Simulation world containing cells and update logic.
 */
struct World {
  int w = 0;                 /**< World width in cells. */
  int h = 0;                 /**< World height in cells. */
  std::vector<Cell> cells;   /**< Row-major cell buffer. */
  std::uint8_t stamp = 1;    /**< Current tick stamp. */
  XorShift32 rng;            /**< RNG used for movement/random events. */

  /**
   * @brief Creates a world with border walls.
   * @param width World width in cells.
   * @param height World height in cells.
   * @param seed RNG seed.
   * @return Constructed world or an error string.
   */
  [[nodiscard]] static std::expected<World, std::string> create(int width, int height, std::uint32_t seed);

  /**
   * @brief Returns a mutable 2D view of the cell storage.
   * @return Mutable grid view.
   */
  Grid2D<Cell> grid();

  /**
   * @brief Returns a const 2D view of the cell storage.
   * @return Const grid view.
   */
  Grid2D<const Cell> grid() const;

  /**
   * @brief Clears the world and restores border walls.
   */
  void clear();

  /**
   * @brief Generates a randomized starting scene.
   *
   * Produces a terrain/fluid/hotspot layout using the world's RNG so each
   * invocation starts from a different setup.
   */
  void generate_random_scene();

  /**
   * @brief Advances the simulation by one tick.
   */
  void tick();

  /**
   * @brief Paints a filled circular brush into the world.
   * @param cx Brush center X in grid coordinates.
   * @param cy Brush center Y in grid coordinates.
   * @param radius Brush radius in cells.
   * @param t Material to apply.
   */
  void paint_disc(int cx, int cy, int radius, CellType t);

  /**
   * @brief Checks whether coordinates are inside world bounds.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return `true` if inside `[0, w) x [0, h)`.
   */
  bool in_bounds(int x, int y) const;

  /**
   * @brief Mutable cell access by coordinates.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return Mutable reference to cell.
   */
  Cell& at(int x, int y);

  /**
   * @brief Const cell access by coordinates.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return Const reference to cell.
   */
  const Cell& at(int x, int y) const;

private:
  /**
   * @brief Dispatches one cell update based on its material.
   */
  void step_cell(int x, int y, bool left_to_right);

  /**
   * @brief Attempts to move a cell into an empty destination.
   * @return `true` if movement occurred.
   */
  bool try_move(int x, int y, int nx, int ny);

  /**
   * @brief Checks if a coordinate contains an empty cell.
   */
  bool is_empty(int x, int y) const;

  /** @brief Updates sand behavior for one cell. */
  void step_sand(int x, int y, bool ltr);

  /** @brief Updates water behavior for one cell. */
  void step_water(int x, int y, bool ltr);

  /** @brief Updates oil behavior for one cell. */
  void step_oil(int x, int y, bool ltr);

  /** @brief Updates smoke behavior for one cell. */
  void step_smoke(int x, int y, bool ltr);

  /** @brief Updates fire behavior for one cell. */
  void step_fire(int x, int y);

  /** @brief Updates lava behavior for one cell. */
  void step_lava(int x, int y, bool ltr);

  /**
   * @brief Adds heat to neighboring non-empty, non-wall cells.
   * @param x Source X coordinate.
   * @param y Source Y coordinate.
   * @param amount Temperature delta applied per neighbor.
   */
  void heat_neighbors(int x, int y, int amount);

  /**
   * @brief Performs a local thermal diffusion pass.
   *
   * Exchanges heat between neighboring cells using per-material thermal
   * conductivities to smooth extreme gradients before relaxation.
   */
  void pass_thermal_exchange();
};
