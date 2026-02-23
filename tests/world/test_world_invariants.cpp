#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
int main(int argc, char* argv[]) { return Catch::Session().run(argc, argv); }
#elif __has_include(<catch2/catch.hpp>)
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#include <algorithm>

#include "world.hpp"

namespace {

void require_world_invariants(const World& world) {
  REQUIRE(world.cells.size() == static_cast<std::size_t>(world.w * world.h));
  REQUIRE(world.thermal_impulses.size() == world.cells.size());

  for (int y = 0; y < world.h; ++y) {
    for (int x = 0; x < world.w; ++x) {
      const Cell& c = world.at(x, y);
      REQUIRE(is_valid_cell_type(c.type));
      REQUIRE(static_cast<int>(c.pressure) >= 0);
      REQUIRE(static_cast<int>(c.pressure) <= 240);
      REQUIRE(static_cast<int>(c.load) >= 0);

      const bool border = (x == 0 || x == world.w - 1 || y == 0 || y == world.h - 1);
      if (border) REQUIRE(c.type == CellType::Wall);
    }
  }
}

}  // namespace

TEST_CASE("World create initializes valid bordered grid", "[world]") {
  auto ex = World::create(64, 48, 12345u);
  REQUIRE(ex.has_value());
  const World& world = *ex;
  CHECK(world.tick_count == 0);
  require_world_invariants(world);
}

TEST_CASE("Random scene generation preserves valid cell states", "[world][random]") {
  auto ex = World::create(96, 72, 0xC0FFEEu);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);

  for (int i = 0; i < 20; ++i) {
    world.generate_random_scene();
    CHECK(world.tick_count == 0);
    require_world_invariants(world);
  }
}

TEST_CASE("Simulation ticks do not produce invalid cell types", "[world][sim]") {
  auto ex = World::create(96, 72, 0xBADC0DEu);
  REQUIRE(ex.has_value());
  World world = std::move(*ex);
  world.generate_random_scene();

  for (int i = 0; i < 400; ++i) {
    world.tick();
    CHECK(world.tick_count == static_cast<std::uint64_t>(i + 1));
    require_world_invariants(world);
  }
}
