#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "world.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

struct RendererSDL {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;

  int scale = 3;
  int win_w = 0;
  int win_h = 0;

  std::vector<std::uint32_t> pixels; // ARGB8888

  static std::expected<RendererSDL, std::string> create(int grid_w, int grid_h, int scale);
  void destroy();

  void draw(const World& world);
};