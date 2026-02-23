#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#elif __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#error "Catch2 headers not found"
#endif

#include <cstdlib>

#include <SDL.h>

#include "render_sdl.hpp"
#include "render_sdl_internal.hpp"
#include "world.hpp"

namespace {

void enable_headless_sdl() {
#if defined(_WIN32)
  _putenv_s("SDL_VIDEODRIVER", "dummy");
#else
  setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
}

World make_world() {
  auto ex = World::create(24, 18, 0xC0FFEEu);
  REQUIRE(ex.has_value());
  return std::move(*ex);
}

}  // namespace

TEST_CASE("RendererSDL create and draw work in headless mode", "[render][sdl][integration]") {
  enable_headless_sdl();

  auto rx = RendererSDL::create(24, 18, 0, 40);  // scale clamp and toolbar min clamp
  REQUIRE(rx.has_value());
  RendererSDL ren = std::move(*rx);

  CHECK(ren.scale == 1);
  CHECK(ren.toolbar_h >= 48);
  CHECK(ren.window != nullptr);
  CHECK(ren.renderer != nullptr);
  CHECK(ren.texture != nullptr);

  World world = make_world();
  world.generate_random_scene();
  for (int i = 0; i < 4; ++i) world.tick();

  // Normal draw (no hover)
  ren.draw(world, CellType::Sand, 3, -1, -1, false, DebugView::None);

  // Toolbar hover paths (material / random / clear).
  ren.draw(world, CellType::Water, 2, 16, 20, true, DebugView::None);               // first material button
  ren.draw(world, CellType::Oil, 2, ren.win_w - 200, 20, false, DebugView::None);    // random button area
  ren.draw(world, CellType::Fire, 2, ren.win_w - 70, 20, false, DebugView::None);    // clear button area

  // Debug views with grid hover for cell readout
  const int grid_hover_x = 4 * ren.scale;
  const int grid_hover_y = ren.toolbar_h + 4 * ren.scale;
  ren.draw(world, CellType::Lava, 4, grid_hover_x, grid_hover_y, false, DebugView::Pressure);
  ren.draw(world, CellType::Smoke, 1, grid_hover_x, grid_hover_y, false, DebugView::Load);

  // Unknown selected type exercises mat_name fallback.
  ren.draw(world, static_cast<CellType>(255), 1, -1, -1, false, DebugView::None);
}

TEST_CASE("RendererSDL move operations preserve resource ownership", "[render][sdl][integration]") {
  enable_headless_sdl();

  auto rx = RendererSDL::create(8, 6, 2, 64);
  REQUIRE(rx.has_value());
  RendererSDL a = std::move(*rx);
  REQUIRE(a.window != nullptr);

  RendererSDL b = std::move(a);  // move ctor
  CHECK(a.window == nullptr);
  CHECK(a.renderer == nullptr);
  CHECK(a.texture == nullptr);
  REQUIRE(b.window != nullptr);

  RendererSDL c;
  c = std::move(b);  // move assign
  CHECK(b.window == nullptr);
  CHECK(b.renderer == nullptr);
  CHECK(b.texture == nullptr);
  CHECK(c.window != nullptr);

  c.set_title("render-test");  // true branch (window exists)
  RendererSDL empty;
  empty.set_title("no-window");  // false branch (no window)
}

namespace {

int injected_init_failure() {
  SDL_SetError("Injected SDL_Init failure");
  return -1;
}

SDL_Renderer* fail_accelerated_only(SDL_Window* window, int index, unsigned flags) {
  if (flags == SDL_RENDERER_ACCELERATED) {
    SDL_SetError("Injected accelerated renderer failure");
    return nullptr;
  }
  return SDL_CreateRenderer(window, index, flags);
}

}  // namespace

TEST_CASE("RendererSDL create reports SDL init failures", "[render][sdl][integration]") {
  enable_headless_sdl();
  render_sdl_detail::SdlApi api = render_sdl_detail::sdl_api();
  api.init_video = &injected_init_failure;

  auto rx = render_sdl_detail::create_with_api(8, 6, 2, 64, api);
  REQUIRE_FALSE(rx.has_value());
  CHECK(rx.error().find("SDL_Init failed:") != std::string::npos);
}

TEST_CASE("RendererSDL create falls back to software renderer when accelerated renderer fails",
          "[render][sdl][integration]") {
  enable_headless_sdl();
  render_sdl_detail::SdlApi api = render_sdl_detail::sdl_api();
  api.create_renderer = &fail_accelerated_only;

  auto rx = render_sdl_detail::create_with_api(8, 6, 2, 64, api);
  REQUIRE(rx.has_value());
  RendererSDL ren = std::move(*rx);
  CHECK(ren.renderer != nullptr);
}
