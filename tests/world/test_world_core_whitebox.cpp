#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#elif __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#define private public
#include "world.hpp"
#undef private

TEST_CASE("World grid views expose the backing storage", "[world][core][whitebox]") {
  auto ex = World::create(8, 6, 42u);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  auto mut_grid = world.grid();
  REQUIRE(mut_grid.w == world.w);
  REQUIRE(mut_grid.h == world.h);
  mut_grid(3, 3).type = CellType::Sand;

  const World& cworld = world;
  auto const_grid = cworld.grid();
  CHECK(const_grid.w == world.w);
  CHECK(const_grid.h == world.h);
  CHECK(const_grid(3, 3).type == CellType::Sand);
}

TEST_CASE("World private empty checks report cell occupancy", "[world][core][whitebox]") {
  auto ex = World::create(8, 6, 77u);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  CHECK(world.is_empty(3, 3));
  world.at(3, 3).type = CellType::Smoke;
  CHECK_FALSE(world.is_empty(3, 3));
}

TEST_CASE("World tick repairs invalid cells and resizes thermal impulse buffer", "[world][core][whitebox]") {
  auto ex = World::create(12, 10, 99u);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  world.thermal_impulses.resize(1);

  // Inject an invalid enum value so the post-tick sanitization reset branch runs.
  Cell& c = world.at(5, 5);
  c.type = static_cast<CellType>(255);
  c.temp = 321;
  c.pressure = 999;
  c.load = -50;

  world.tick();

  CHECK(world.thermal_impulses.size() == world.cells.size());
  CHECK(world.tick_count == 1);

  const Cell& repaired = world.at(5, 5);
  CHECK(repaired.type == CellType::Empty);
  CHECK(repaired.temp == 20);
  CHECK(repaired.pressure == 0);
  CHECK(repaired.load == 0);
}
