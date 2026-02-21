/**
 * @file main.cpp
 * @brief Application entry point and interactive event loop.
 */

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>

#include "render_sdl.hpp"
#include "world.hpp"

/**
 * @brief Prints a fatal error and terminates the process.
 * @param msg Message to display.
 */
static void fatal(const std::string& msg) {
  std::cerr << msg << "\n";
  std::exit(1);
}

/**
 * @brief Converts a material type to a short UI label.
 * @param t Material enum value.
 * @return Name shown in the window title.
 */
static const char* material_name(CellType t) {
  switch (t) {
    case CellType::Sand: return "Sand";
    case CellType::Water: return "Water";
    case CellType::Oil: return "Oil";
    case CellType::Fire: return "Fire";
    case CellType::Smoke: return "Smoke";
    case CellType::Lava: return "Lava";
    case CellType::Wall: return "Wall";
    case CellType::Empty: return "Erase";
    default: return "Unknown";
  }
}

/**
 * @brief Updates window title with brush and pause state.
 * @param ren Active SDL renderer wrapper.
 * @param brush Selected brush material.
 * @param radius Current brush radius.
 * @param paused Simulation paused flag.
 */
static void update_title(RendererSDL& ren, CellType brush, int radius, bool paused) {
  std::string title = "Particle Sandbox | ";
  title += material_name(brush);
  title += " | radius ";
  title += std::to_string(radius);
  if (paused) title += " | PAUSED";
  ren.set_title(title);
}

/**
 * @brief Runs the particle sandbox application.
 * @return Process exit code.
 */
int main(int, char**) {
  constexpr int W = 220;
  constexpr int H = 160;
  constexpr int SCALE = 5;
  constexpr int TOOLBAR_H = 64;

  auto world_ex = World::create(W, H, 0xC0FFEEu);
  if (!world_ex) fatal(world_ex.error());
  World world = std::move(*world_ex);

  auto ren_ex = RendererSDL::create(W, H, SCALE, TOOLBAR_H);
  if (!ren_ex) fatal(ren_ex.error());
  RendererSDL ren = std::move(*ren_ex);

  bool running = true;
  bool paused = false;

  CellType brush = CellType::Sand;
  int brush_radius = 4;

  update_title(ren, brush, brush_radius, paused);

  auto last = std::chrono::steady_clock::now();

  int mx = 0, my = 0;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;

      if (e.type == SDL_MOUSEMOTION) {
        mx = e.motion.x;
        my = e.motion.y;
      }

      if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
          case SDLK_ESCAPE: running = false; break;
          case SDLK_SPACE: paused = !paused; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_c: world.clear(); break;

          case SDLK_1: brush = CellType::Sand; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_2: brush = CellType::Water; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_3: brush = CellType::Oil; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_4: brush = CellType::Fire; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_5: brush = CellType::Smoke; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_6: brush = CellType::Lava; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_7: brush = CellType::Wall; update_title(ren, brush, brush_radius, paused); break;
          case SDLK_0: brush = CellType::Empty; update_title(ren, brush, brush_radius, paused); break;

          case SDLK_MINUS:
            brush_radius = std::max(1, brush_radius - 1);
            update_title(ren, brush, brush_radius, paused);
            break;

          case SDLK_EQUALS:
            brush_radius = std::min(30, brush_radius + 1);
            update_title(ren, brush, brush_radius, paused);
            break;
        }
      }

      if (e.type == SDL_MOUSEWHEEL) {
        brush_radius = std::clamp(brush_radius + e.wheel.y, 1, 30);
        update_title(ren, brush, brush_radius, paused);
      }

      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        mx = e.button.x;
        my = e.button.y;

        if (auto hit = ren.hit_test_toolbar(mx, my)) {
          brush = *hit;
          update_title(ren, brush, brush_radius, paused);
        }
      }
    }

    int gx = 0, gy = 0;
    const bool in_grid = ren.mouse_to_grid(mx, my, gx, gy);

    const auto mstate = SDL_GetMouseState(nullptr, nullptr);
    if (in_grid) {
      if (mstate & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        world.paint_disc(gx, gy, brush_radius, brush);
      }
      if (mstate & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
        world.paint_disc(gx, gy, brush_radius, CellType::Empty);
      }
    }

    auto now = std::chrono::steady_clock::now();
    const auto dt = now - last;

    if (dt < std::chrono::milliseconds(12)) {
      SDL_Delay(1);
      continue;
    }
    last = now;

    if (!paused) world.tick();

    ren.draw(world, brush, brush_radius, mx, my, paused);
  }

  ren.destroy();
  return 0;
}
