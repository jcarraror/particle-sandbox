#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#elif __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#include <set>

#include "render_core.hpp"
#include "render_font.hpp"
#include "render_sdl.hpp"

namespace {

RendererSDL make_test_renderer() {
  RendererSDL r;
  r.scale = 4;
  r.grid_w = 20;
  r.grid_h = 10;
  r.toolbar_h = 64;
  r.win_w = r.grid_w * r.scale;
  r.win_h = r.toolbar_h + r.grid_h * r.scale;
  return r;
}

}  // namespace

TEST_CASE("RendererSDL exposes stable material metadata", "[render][pure]") {
  const auto mats = RendererSDL::materials();
  REQUIRE_FALSE(mats.empty());
  CHECK(mats.front().type == CellType::Sand);
  CHECK(mats.back().type == CellType::Empty);

  std::set<CellType> unique_types;
  for (const auto& m : mats) {
    CHECK(m.name != nullptr);
    CHECK(m.hotkey != nullptr);
    unique_types.insert(m.type);
  }
  CHECK(unique_types.size() == mats.size());
  CHECK(unique_types.count(CellType::Wall) == 1);
  CHECK(unique_types.count(CellType::Lava) == 1);
  CHECK(unique_types.count(CellType::Empty) == 1);
}

TEST_CASE("RendererSDL mouse_to_grid rejects toolbar/outside and maps grid pixels", "[render][pure]") {
  const RendererSDL r = make_test_renderer();
  int gx = -1;
  int gy = -1;

  CHECK_FALSE(r.mouse_to_grid(-1, 70, gx, gy));
  CHECK_FALSE(r.mouse_to_grid(r.win_w, 70, gx, gy));
  CHECK_FALSE(r.mouse_to_grid(10, 0, gx, gy));           // toolbar area
  CHECK_FALSE(r.mouse_to_grid(10, r.win_h, gx, gy));     // below window

  REQUIRE(r.mouse_to_grid(0, r.toolbar_h, gx, gy));
  CHECK(gx == 0);
  CHECK(gy == 0);

  REQUIRE(r.mouse_to_grid(r.win_w - 1, r.win_h - 1, gx, gy));
  CHECK(gx == r.grid_w - 1);
  CHECK(gy == r.grid_h - 1);
}

TEST_CASE("RendererSDL toolbar hit tests find materials and action buttons", "[render][pure]") {
  RendererSDL r = make_test_renderer();
  // Ensure action buttons fit in the test window with room for material buttons.
  r.win_w = 900;

  std::set<CellType> seen_materials;
  bool saw_clear = false;
  bool saw_random = false;

  for (int y = 0; y < r.toolbar_h; ++y) {
    for (int x = 0; x < r.win_w; ++x) {
      if (auto hit = r.hit_test_toolbar(x, y)) seen_materials.insert(*hit);
      saw_clear = saw_clear || r.hit_test_clear_button(x, y);
      saw_random = saw_random || r.hit_test_random_button(x, y);
    }
  }

  const auto mats = RendererSDL::materials();
  REQUIRE(seen_materials.size() == mats.size());
  for (const auto& m : mats) CHECK(seen_materials.count(m.type) == 1);
  CHECK(saw_clear);
  CHECK(saw_random);

  // Outside toolbar Y should produce no hits.
  CHECK_FALSE(r.hit_test_toolbar(10, r.toolbar_h).has_value());
  CHECK_FALSE(r.hit_test_clear_button(r.win_w - 10, r.toolbar_h));
  CHECK_FALSE(r.hit_test_random_button(r.win_w - 150, r.toolbar_h));
}

TEST_CASE("render_core color mapping covers all material display models", "[render][core]") {
  for (std::size_t i = 0; i < kCellTypeCount; ++i) {
    Cell c{};
    c.type = static_cast<CellType>(i);
    c.temp = (c.type == CellType::Lava) ? 900 : (c.type == CellType::Fire ? 350 : 80);
    const auto color = render_core::color_for_cell(c);
    CHECK(color != 0u);
  }

  Cell invalid{};
  invalid.type = static_cast<CellType>(255);
  CHECK(render_core::color_for_cell(invalid) != 0u);
}

TEST_CASE("render_core animated lava color varies over time while static materials stay stable", "[render][core][anim]") {
  Cell lava{};
  lava.type = CellType::Lava;
  lava.temp = 900;
  lava.flow_dir = 1;
  lava.flow_strength = 3;

  const auto lava_t0 = render_core::color_for_cell_animated(lava, 10, 12, 0);
  const auto lava_t1 = render_core::color_for_cell_animated(lava, 10, 12, 9);
  CHECK(lava_t0 != 0u);
  CHECK(lava_t1 != 0u);
  CHECK(lava_t0 != lava_t1);

  Cell sand{};
  sand.type = CellType::Sand;
  sand.temp = 80;
  const auto sand_static = render_core::color_for_cell(sand);
  CHECK(render_core::color_for_cell_animated(sand, 3, 4, 0) == sand_static);
  CHECK(render_core::color_for_cell_animated(sand, 3, 4, 25) == sand_static);
}

TEST_CASE("render_core debug color mapping handles pressure and load variants", "[render][core][debug]") {
  for (std::size_t i = 0; i < kCellTypeCount; ++i) {
    Cell c{};
    c.type = static_cast<CellType>(i);
    c.pressure = 180;
    c.load = 260;
    CHECK(render_core::color_for_pressure_debug(c) != 0u);
    CHECK(render_core::color_for_load_debug(c, 0, 480) != 0u);
  }

  Cell invalid{};
  invalid.type = static_cast<CellType>(255);
  invalid.pressure = 240;
  invalid.load = 999;
  CHECK(render_core::color_for_pressure_debug(invalid) != 0u);
  CHECK(render_core::color_for_load_debug(invalid, 0, 480) != 0u);
}

TEST_CASE("render_core swatch colors are stable for toolbar materials", "[render][core]") {
  for (const auto& m : RendererSDL::materials()) {
    CHECK(render_core::material_swatch_color(m.type) != 0u);
  }
}

TEST_CASE("render_font glyph lookup supports UI glyphs and misses unknown chars", "[render][font]") {
  const auto* a = render_font::glyph_for('A');
  REQUIRE(a != nullptr);
  CHECK((*a).rows[0] != 0);

  const auto* digit = render_font::glyph_for('7');
  REQUIRE(digit != nullptr);
  CHECK((*digit).rows[0] != 0);

  CHECK(render_font::glyph_for('?') == nullptr);
  CHECK(render_font::glyph_for('a') == nullptr);
}
