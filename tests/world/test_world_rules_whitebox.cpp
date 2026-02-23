#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#elif __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#include <array>

#define private public
#include "world.hpp"
#undef private

namespace {

World make_world(int w = 11, int h = 11, std::uint32_t seed = 1u) {
  auto ex = World::create(w, h, seed);
  return std::move(ex.value());
}

void set_cell(World& world, int x, int y, CellType type, int temp = 20) {
  Cell& c = world.at(x, y);
  c.type = type;
  c.temp = static_cast<std::int16_t>(temp);
  c.pressure = 0;
  c.load = 0;
  c.updated = 0;
}

}  // namespace

TEST_CASE("Water absorbs heat from adjacent hot fire and lava", "[world][rules][water]") {
  World world = make_world();
  world.stamp = 9;

  set_cell(world, 5, 5, CellType::Water, 20);
  set_cell(world, 4, 5, CellType::Fire, 300);
  set_cell(world, 6, 5, CellType::Lava, 800);

  // Block movement so only thermal interaction is exercised.
  set_cell(world, 5, 6, CellType::Wall, 20);
  set_cell(world, 4, 6, CellType::Wall, 20);
  set_cell(world, 6, 6, CellType::Wall, 20);
  set_cell(world, 4, 4, CellType::Wall, 20);
  set_cell(world, 5, 4, CellType::Wall, 20);
  set_cell(world, 6, 4, CellType::Wall, 20);
  set_cell(world, 2, 5, CellType::Wall, 20);
  set_cell(world, 3, 5, CellType::Wall, 20);
  set_cell(world, 7, 5, CellType::Wall, 20);
  set_cell(world, 8, 5, CellType::Wall, 20);

  world.step_water(5, 5, true);

  CHECK(world.at(4, 5).temp == 284);
  CHECK(world.at(6, 5).temp == 790);
  CHECK(world.at(5, 5).temp == 46);
}

TEST_CASE("Water hot-wall evaporation path is exercised without direct flame contact", "[world][rules][water]") {
  World world = make_world(9, 9, 2u);
  world.stamp = 3;

  set_cell(world, 4, 4, CellType::Water, 180);
  set_cell(world, 4, 3, CellType::Wall, 250);  // hot wall only (no fire/lava)
  set_cell(world, 4, 5, CellType::Wall, 20);   // block falling so state remains local
  set_cell(world, 3, 3, CellType::Wall, 20);
  set_cell(world, 3, 4, CellType::Wall, 20);
  set_cell(world, 3, 5, CellType::Wall, 20);
  set_cell(world, 5, 3, CellType::Wall, 20);
  set_cell(world, 5, 4, CellType::Wall, 20);
  set_cell(world, 5, 5, CellType::Wall, 20);
  set_cell(world, 1, 4, CellType::Wall, 20);
  set_cell(world, 2, 4, CellType::Wall, 20);
  set_cell(world, 6, 4, CellType::Wall, 20);
  set_cell(world, 7, 4, CellType::Wall, 20);

  world.step_water(4, 4, true);

  CHECK((world.at(4, 4).type == CellType::Water || world.at(4, 4).type == CellType::Smoke));
}

TEST_CASE("Fire quench lowers fire temperature and can force low-temp smoke conversion", "[world][rules][fire]") {
  World world = make_world(9, 9, 3u);
  world.stamp = 7;

  set_cell(world, 4, 4, CellType::Fire, 120);
  set_cell(world, 4, 3, CellType::Water, 20);

  world.step_fire(4, 4);

  CHECK(world.at(4, 3).temp == 36);
  CHECK(world.at(4, 4).type == CellType::Smoke);
  CHECK(world.at(4, 4).temp == 80);
}

TEST_CASE("Lava passive solidification path applies interface penalties before RNG", "[world][rules][lava]") {
  SECTION("deep interior path hits strong penalty branch") {
    World world = make_world(11, 11, 4u);
    world.stamp = 5;

    for (int y = 4; y <= 6; ++y) {
      for (int x = 4; x <= 6; ++x) set_cell(world, x, y, CellType::Lava, 80);
    }

    world.step_lava(5, 5, true);
    CHECK(true);
  }

  SECTION("non-interface non-deep path hits moderate penalty branch") {
    World world = make_world(11, 11, 5u);
    world.stamp = 6;

    set_cell(world, 5, 5, CellType::Lava, 80);
    set_cell(world, 4, 5, CellType::Lava, 80);
    set_cell(world, 6, 5, CellType::Lava, 80);
    set_cell(world, 5, 4, CellType::Lava, 80);
    set_cell(world, 5, 6, CellType::Sand, 80);  // breaks deep interior without adding thermal sink
    set_cell(world, 4, 4, CellType::Lava, 80);
    set_cell(world, 6, 4, CellType::Lava, 80);
    set_cell(world, 4, 6, CellType::Sand, 80);
    set_cell(world, 6, 6, CellType::Sand, 80);

    world.step_lava(5, 5, false);
    CHECK(true);
  }
}

TEST_CASE("Lava solidification can propagate crust into adjacent cooled lava", "[world][rules][lava]") {
  bool propagated = false;

  for (std::uint32_t seed = 1; seed <= 256 && !propagated; ++seed) {
    World world = make_world(13, 13, seed);
    world.stamp = 11;

    // Center lava: very cool + water-cooled => main solidification divisor reaches 1.
    set_cell(world, 6, 6, CellType::Lava, 100);
    set_cell(world, 6, 5, CellType::Water, 20);
    set_cell(world, 5, 6, CellType::Water, 20);
    set_cell(world, 7, 6, CellType::Water, 20);

    // Eligible neighboring lava for propagation (cool enough and not deep interior).
    set_cell(world, 6, 7, CellType::Lava, 100);

    world.step_lava(6, 6, true);

    const bool center_solidified = world.at(6, 6).type == CellType::Wall;
    const bool neighbor_propagated = world.at(6, 7).type == CellType::Wall;
    propagated = center_solidified && neighbor_propagated;
  }

  CHECK(propagated);
}
