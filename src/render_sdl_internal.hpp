#pragma once

#include <expected>
#include <string>

#include "render_sdl.hpp"

namespace render_sdl_detail {

struct SdlApi {
  int (*init_video)();
  const char* (*get_error)();
  SDL_Window* (*create_window)(const char* title, int x, int y, int w, int h, unsigned flags);
  SDL_Renderer* (*create_renderer)(SDL_Window* window, int index, unsigned flags);
  SDL_Texture* (*create_texture)(SDL_Renderer* renderer, unsigned format, int access, int w, int h);
};

[[nodiscard]] const SdlApi& sdl_api() noexcept;

[[nodiscard]] std::expected<RendererSDL, std::string> create_with_api(
    int grid_w, int grid_h, int scale, int toolbar_h_px, const SdlApi& api);

}  // namespace render_sdl_detail

