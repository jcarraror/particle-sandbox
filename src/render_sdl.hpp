#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include <optional>
#include "world.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

struct MaterialButton {
  CellType type{};
  const char* name{};
  const char* hotkey{};
};

struct RendererSDL {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;

  int scale = 4;

  int grid_w = 0;       // in cells
  int grid_h = 0;       // in cells
  int toolbar_h = 64;   // in pixels

  int win_w = 0;
  int win_h = 0;

  std::vector<std::uint32_t> pixels; // ARGB8888 (grid_w * grid_h)

  static std::expected<RendererSDL, std::string> create(int grid_w, int grid_h, int scale, int toolbar_h_px = 64);
  void destroy();

  void set_title(const std::string& title);

  void draw(const World& world,
            CellType selected,
            int brush_radius,
            int mouse_x,
            int mouse_y,
            bool paused);

  // convert window mouse coords -> grid coords
  bool mouse_to_grid(int mx, int my, int& gx, int& gy) const;

  // if click is on toolbar button, returns that CellType; otherwise returns nullopt
  std::optional<CellType> hit_test_toolbar(int mx, int my) const;

  // prevent painting while hovering toolbar
  bool mouse_in_toolbar(int my) const { return my >= 0 && my < toolbar_h; }

  static std::vector<MaterialButton> materials();
};