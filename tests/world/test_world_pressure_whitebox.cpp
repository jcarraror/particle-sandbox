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
#include "world_pressure_internal.hpp"

TEST_CASE("Dense pressure target handles invalid cell type via default coeffs", "[world][pressure][whitebox]") {
  auto ex = World::create(9, 9, 321u);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  Cell& c = world.at(4, 4);
  c.type = static_cast<CellType>(255);  // invalid on purpose to hit coeffs_for_dense default branch
  c.temp = 123;
  c.pressure = 42;
  c.load = 7;

  const int p = pressure_detail::compute_dense_pressure_target(world, 4, 4, c);
  CHECK(p == 0);
}

