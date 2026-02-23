#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#elif __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#define private public
#include "world.hpp"
#undef private

TEST_CASE("Thermal pass repairs mismatched thermal impulse buffer size", "[world][thermo][whitebox]") {
  auto ex = World::create(8, 8, 123u);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  world.thermal_impulses.resize(1);  // force `pass_thermal_exchange()` resize guard
  world.at(3, 3).type = CellType::Wall;
  world.at(3, 3).temp = 300;
  world.at(4, 3).type = CellType::Water;
  world.at(4, 3).temp = 20;

  world.pass_thermal_exchange();

  CHECK(world.thermal_impulses.size() == world.cells.size());
}

