#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

#include "render_sdl.hpp"
#include "world.hpp"

static void fatal(const std::string& msg) {
  std::cerr << msg << "\n";
  std::exit(1);
}

int main(int, char**) {
  constexpr int W = 220;
  constexpr int H = 160;
  constexpr int SCALE = 5;

  auto world_ex = World::create(W, H, 0xC0FFEEu);
  if (!world_ex) fatal(world_ex.error());
  World world = std::move(*world_ex);

  auto ren_ex = RendererSDL::create(W, H, SCALE);
  if (!ren_ex) fatal(ren_ex.error());
  RendererSDL ren = std::move(*ren_ex);

  bool running = true;
  bool paused = false;

  CellType brush = CellType::Sand;
  int brush_radius = 4;

  auto last = std::chrono::steady_clock::now();

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;

      if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
          case SDLK_ESCAPE: running = false; break;
          case SDLK_SPACE:  paused = !paused; break;
          case SDLK_c:      world.clear(); break;

          case SDLK_1: brush = CellType::Sand; break;
          case SDLK_2: brush = CellType::Water; break;
          case SDLK_3: brush = CellType::Oil; break;
          case SDLK_4: brush = CellType::Fire; break;
          case SDLK_5: brush = CellType::Smoke; break;
          case SDLK_6: brush = CellType::Lava; break;
          case SDLK_7: brush = CellType::Wall; break;
          case SDLK_0: brush = CellType::Empty; break;

          case SDLK_MINUS:  brush_radius = std::max(1, brush_radius - 1); break;
          case SDLK_EQUALS: brush_radius = std::min(30, brush_radius + 1); break;
        }
      }

      if (e.type == SDL_MOUSEWHEEL) {
        brush_radius = std::clamp(brush_radius + e.wheel.y, 1, 30);
      }
    }

    int mx, my;
    const auto m = SDL_GetMouseState(&mx, &my);
    const int gx = mx / SCALE;
    const int gy = my / SCALE;

    if (m & SDL_BUTTON(SDL_BUTTON_LEFT)) {
      world.paint_disc(gx, gy, brush_radius, brush);
    }
    if (m & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
      world.paint_disc(gx, gy, brush_radius, CellType::Empty);
    }

    auto now = std::chrono::steady_clock::now();
    const auto dt = now - last;

    if (dt < std::chrono::milliseconds(12)) {
      SDL_Delay(1);
      continue;
    }
    last = now;

    if (!paused) world.tick();
    ren.draw(world);
  }

  ren.destroy();
  return 0;
}