#include "render_sdl.hpp"

#include <SDL.h>

static std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b);
}

static std::uint32_t color_for(const Cell& c) {
  auto heat = [](int t) -> std::uint8_t {
    int v = (t - 20) / 4;
    v = (v < 0) ? 0 : (v > 60 ? 60 : v);
    return static_cast<std::uint8_t>(v);
  };
  const std::uint8_t h = heat(c.temp);

  switch (c.type) {
    case CellType::Empty: return argb(255, 0, 0, 0);
    case CellType::Wall:  return argb(255, 90, 90, 90);
    case CellType::Sand:  return argb(255, 200, 180, 80);
    case CellType::Water: return argb(255, 60, 110, 220);
    case CellType::Oil:   return argb(255, 30, 30, 35);
    case CellType::Smoke: return argb(255, 120, 120, 120);
    case CellType::Fire:  return argb(255, static_cast<std::uint8_t>(220 + h / 2),
                                      static_cast<std::uint8_t>(80 + h), 20);
    case CellType::Lava:  return argb(255, static_cast<std::uint8_t>(180 + h),
                                      static_cast<std::uint8_t>(40 + h / 2), 10);
    default:              return argb(255, 255, 0, 255);
  }
}

std::expected<RendererSDL, std::string> RendererSDL::create(int grid_w, int grid_h, int s) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return std::unexpected(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  RendererSDL r;
  r.scale = (s <= 0) ? 1 : s;
  r.win_w = grid_w * r.scale;
  r.win_h = grid_h * r.scale;

  r.window = SDL_CreateWindow("Particle Sandbox",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              r.win_w, r.win_h, SDL_WINDOW_SHOWN);
  if (!r.window) return std::unexpected(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

  r.renderer = SDL_CreateRenderer(r.window, -1, SDL_RENDERER_ACCELERATED);
  if (!r.renderer) return std::unexpected(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());

  r.texture = SDL_CreateTexture(r.renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, grid_w, grid_h);
  if (!r.texture) return std::unexpected(std::string("SDL_CreateTexture failed: ") + SDL_GetError());

  r.pixels.resize(static_cast<std::size_t>(grid_w * grid_h), 0);
  return r;
}

void RendererSDL::destroy() {
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  texture = nullptr;
  renderer = nullptr;
  window = nullptr;
  SDL_Quit();
}

void RendererSDL::draw(const World& world) {
  const int gw = world.w;
  const int gh = world.h;

  for (int y = 0; y < gh; ++y) {
    for (int x = 0; x < gw; ++x) {
      pixels[static_cast<std::size_t>(y * gw + x)] = color_for(world.at(x, y));
    }
  }

  SDL_UpdateTexture(texture, nullptr, pixels.data(), gw * int(sizeof(std::uint32_t)));

  SDL_RenderClear(renderer);
  SDL_Rect dst{0, 0, gw * scale, gh * scale};
  SDL_RenderCopy(renderer, texture, nullptr, &dst);
  SDL_RenderPresent(renderer);
}