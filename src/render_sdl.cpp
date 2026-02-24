/**
 * @file render_sdl.cpp
 * @brief SDL rendering implementation for world pixels and toolbar UI.
 */

#include "render_sdl.hpp"
#include "render_core.hpp"
#include "render_font.hpp"
#include "render_sdl_internal.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ToolbarLayout {
  int pad{12};
  int gap{10};
  int button_margin_y{12};
  int action_button_width{124};
  int panel_gap{16};
  int panel_min_width{120};
  int panel_inset{8};
  int panel_top_band_h{8};
  int material_strip_h{6};
  int status_line1_y{12};
  int status_line2_y{30};
};

struct ToolbarTheme {
  std::uint32_t frame_bg{};
  std::uint32_t toolbar_bg{};
  std::uint32_t toolbar_border{};
  std::uint32_t button_bg{};
  std::uint32_t button_hover_bg{};
  std::uint32_t button_selected_bg{};
  std::uint32_t button_border{};
  std::uint32_t button_selected_border{};
  std::uint32_t icon_fg{};
  std::uint32_t panel_bg{};
  std::uint32_t panel_top_bg{};
  std::uint32_t panel_border{};
  std::uint32_t status_text{};
  std::uint32_t legend_text{};
  std::uint32_t random_bg{};
  std::uint32_t random_hover_bg{};
  std::uint32_t random_top_bg{};
  std::uint32_t random_hover_top_bg{};
  std::uint32_t random_border{};
  std::uint32_t random_text{};
  std::uint32_t clear_bg{};
  std::uint32_t clear_hover_bg{};
  std::uint32_t clear_top_bg{};
  std::uint32_t clear_hover_top_bg{};
  std::uint32_t clear_border{};
  std::uint32_t clear_text{};
};

constexpr ToolbarLayout kToolbarLayout{};
constexpr int kLoadDebugRangeMin = 0;
constexpr int kLoadDebugRangeMax = 480;

const ToolbarTheme kToolbarTheme{
    .frame_bg = render_core::argb(255, 10, 10, 12),
    .toolbar_bg = render_core::argb(255, 18, 18, 22),
    .toolbar_border = render_core::argb(255, 70, 70, 85),
    .button_bg = render_core::argb(255, 24, 24, 30),
    .button_hover_bg = render_core::argb(255, 32, 32, 40),
    .button_selected_bg = render_core::argb(255, 40, 40, 50),
    .button_border = render_core::argb(255, 85, 85, 100),
    .button_selected_border = render_core::argb(255, 255, 255, 255),
    .icon_fg = render_core::argb(255, 220, 220, 230),
    .panel_bg = render_core::argb(255, 22, 24, 30),
    .panel_top_bg = render_core::argb(255, 32, 35, 45),
    .panel_border = render_core::argb(255, 70, 78, 100),
    .status_text = render_core::argb(255, 220, 226, 240),
    .legend_text = render_core::argb(255, 172, 186, 205),
    .random_bg = render_core::argb(255, 30, 56, 64),
    .random_hover_bg = render_core::argb(255, 42, 76, 88),
    .random_top_bg = render_core::argb(255, 48, 86, 96),
    .random_hover_top_bg = render_core::argb(255, 64, 114, 128),
    .random_border = render_core::argb(255, 105, 170, 185),
    .random_text = render_core::argb(255, 226, 242, 244),
    .clear_bg = render_core::argb(255, 64, 32, 32),
    .clear_hover_bg = render_core::argb(255, 88, 44, 44),
    .clear_top_bg = render_core::argb(255, 96, 48, 48),
    .clear_hover_top_bg = render_core::argb(255, 125, 64, 64),
    .clear_border = render_core::argb(255, 185, 105, 105),
    .clear_text = render_core::argb(255, 244, 226, 226),
};

}  // namespace

namespace render_sdl_detail {

namespace {

int sdl_init_video_default() { return SDL_Init(SDL_INIT_VIDEO); }

SDL_Window* sdl_create_window_default(const char* title, int x, int y, int w, int h, unsigned flags) {
  return SDL_CreateWindow(title, x, y, w, h, flags);
}

SDL_Renderer* sdl_create_renderer_default(SDL_Window* window, int index, unsigned flags) {
  return SDL_CreateRenderer(window, index, flags);
}

SDL_Texture* sdl_create_texture_default(SDL_Renderer* renderer, unsigned format, int access, int w, int h) {
  return SDL_CreateTexture(renderer, format, access, w, h);
}

const SdlApi kDefaultSdlApi{
    .init_video = &sdl_init_video_default,
    .get_error = &SDL_GetError,
    .create_window = &sdl_create_window_default,
    .create_renderer = &sdl_create_renderer_default,
    .create_texture = &sdl_create_texture_default,
};

}  // namespace

const SdlApi& sdl_api() noexcept {
  return kDefaultSdlApi;
}

std::expected<RendererSDL, std::string> create_with_api(
    int gw, int gh, int s, int toolbar_h_px, const SdlApi& api) {
  if (api.init_video() != 0) {
    return std::unexpected(std::string("SDL_Init failed: ") + api.get_error());
  }

  RendererSDL r;
  r.owns_sdl_video = true;
  r.scale = (s <= 0) ? 1 : s;
  r.grid_w = gw;
  r.grid_h = gh;
  r.toolbar_h = (toolbar_h_px < 48) ? 48 : toolbar_h_px;

  r.win_w = r.grid_w * r.scale;
  r.win_h = r.toolbar_h + r.grid_h * r.scale;

  r.window = api.create_window("Particle Sandbox (C++23)",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               r.win_w, r.win_h, SDL_WINDOW_SHOWN);
  if (!r.window) return std::unexpected(std::string("SDL_CreateWindow failed: ") + api.get_error());

  r.renderer = api.create_renderer(r.window, -1, SDL_RENDERER_ACCELERATED);
  if (!r.renderer) {
    // Some headless drivers don't support accelerated renderers.
    r.renderer = api.create_renderer(r.window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!r.renderer) return std::unexpected(std::string("SDL_CreateRenderer failed: ") + api.get_error());

  r.texture = api.create_texture(r.renderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, r.grid_w, r.grid_h);
  if (!r.texture) return std::unexpected(std::string("SDL_CreateTexture failed: ") + api.get_error());

  r.pixels.resize(static_cast<std::size_t>(r.grid_w * r.grid_h), 0);
  return r;
}

}  // namespace render_sdl_detail

/**
 * @brief Packs channels into ARGB8888.
 * @param a Alpha channel.
 * @param r Red channel.
 * @param g Green channel.
 * @param b Blue channel.
 * @return Packed 32-bit color.
 */
/**
 * @brief Sets SDL renderer draw color from ARGB8888.
 * @param r SDL renderer pointer.
 * @param c Packed color.
 */
static void set_draw_color(SDL_Renderer* r, std::uint32_t c) {
  const std::uint8_t a = (c >> 24) & 0xFF;
  const std::uint8_t rr = (c >> 16) & 0xFF;
  const std::uint8_t g = (c >> 8) & 0xFF;
  const std::uint8_t b = (c >> 0) & 0xFF;
  SDL_SetRenderDrawColor(r, rr, g, b, a);
}

/**
 * @brief Chooses a display color for a simulation cell.
 * @param c Input cell state.
 * @return Packed ARGB color.
 */

constexpr std::array<MaterialButton, 9> kMaterialButtons{{
    {CellType::Sand, "SAND", "[1]"},
    {CellType::Water, "WATER", "[2]"},
    {CellType::Oil, "OIL", "[3]"},
    {CellType::Fire, "FIRE", "[4]"},
    {CellType::Smoke, "SMOKE", "[5]"},
    {CellType::Lava, "LAVA", "[6]"},
    {CellType::Wall, "WALL", "[7]"},
    {CellType::Stone, "STONE", "[8]"},
    {CellType::Empty, "ERASE", "[0]"},
}};

/**
 * @brief Draws a single character using the pixel font.
 */
static void draw_char(SDL_Renderer* r, int x, int y, char c, int scale, std::uint32_t color) {
  const render_font::Glyph5x7* g = render_font::glyph_for(c);
  if (!g) return;
  set_draw_color(r, color);
  for (int row = 0; row < 7; ++row) {
    const std::uint8_t bits = g->rows[row];
    for (int col = 0; col < 5; ++col) {
      if (bits & (1u << (4 - col))) {
        SDL_Rect px{x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(r, &px);
      }
    }
  }
}

/**
 * @brief Draws a full string using the pixel font.
 */
static void draw_text(SDL_Renderer* r, int x, int y, const std::string& s, int scale, std::uint32_t color) {
  int cx = x;
  for (char ch : s) {
    draw_char(r, cx, y, ch, scale, color);
    cx += 6 * scale;
  }
}

std::span<const MaterialButton> RendererSDL::materials() {
  return kMaterialButtons;
}

/**
 * @brief Gets representative swatch color for a material.
 * @param t Material type.
 * @return Packed ARGB color.
 */
static std::uint32_t material_swatch_color(CellType t) {
  return render_core::material_swatch_color(t);
}

/**
 * @brief Computes the rectangle for a toolbar button index.
 * @param index Button index in the material list.
 * @param toolbar_h Toolbar height in pixels.
 * @return Screen-space button rectangle.
 */
static SDL_Rect toolbar_button_rect(int index, int toolbar_h) {
  const int size = toolbar_h - (2 * kToolbarLayout.button_margin_y);
  const int x = kToolbarLayout.pad + index * (size + kToolbarLayout.gap);
  const int y = kToolbarLayout.button_margin_y;
  return SDL_Rect{x, y, size, size};
}

enum class ToolbarActionSlot : int {
  Random = 0,
  Clear = 1,
};

/**
 * @brief Computes the rectangle for a right-side toolbar action button.
 * @param win_w Window width in pixels.
 * @param toolbar_h Toolbar height in pixels.
 * @param slot Action button slot from right to left.
 * @return Screen-space action button rectangle.
 */
static SDL_Rect action_button_rect(int win_w, int toolbar_h, ToolbarActionSlot slot) {
  const int size = toolbar_h - (2 * kToolbarLayout.button_margin_y);
  const int width = kToolbarLayout.action_button_width;
  const int x =
      win_w - kToolbarLayout.pad - width - (static_cast<int>(slot) * (width + kToolbarLayout.gap));
  const int y = kToolbarLayout.button_margin_y;
  return SDL_Rect{x, y, width, size};
}

/**
 * @brief Computes the rectangle for the toolbar clear button.
 * @param win_w Window width in pixels.
 * @param toolbar_h Toolbar height in pixels.
 * @return Screen-space clear button rectangle.
 */
static SDL_Rect clear_button_rect(int win_w, int toolbar_h) {
  return action_button_rect(win_w, toolbar_h, ToolbarActionSlot::Clear);
}

/**
 * @brief Computes the rectangle for the toolbar randomize button.
 * @param win_w Window width in pixels.
 * @param toolbar_h Toolbar height in pixels.
 * @return Screen-space randomize button rectangle.
 */
static SDL_Rect random_button_rect(int win_w, int toolbar_h) {
  return action_button_rect(win_w, toolbar_h, ToolbarActionSlot::Random);
}

/**
 * @brief Draws a symbolic icon inside a toolbar button.
 * @param r SDL renderer.
 * @param b Target button rectangle.
 * @param t Material type represented by icon.
 * @param fg Foreground icon color.
 */
static void draw_icon(SDL_Renderer* r, const SDL_Rect& b, CellType t, std::uint32_t fg) {
  set_draw_color(r, fg);

  const int cx = b.x + b.w / 2;
  const int cy = b.y + b.h / 2;

  switch (t) {
    case CellType::Sand: {
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + 10, b.y + b.h - 10);
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + b.w - 10, b.y + b.h - 10);
      SDL_RenderDrawLine(r, b.x + 10, b.y + b.h - 10, b.x + b.w - 10, b.y + b.h - 10);
    } break;

    case CellType::Water: {
      for (int i = 0; i < 3; ++i) {
        int y = b.y + 12 + i * 8;
        SDL_RenderDrawLine(r, b.x + 10, y, b.x + 18, y + 4);
        SDL_RenderDrawLine(r, b.x + 18, y + 4, b.x + 26, y);
        SDL_RenderDrawLine(r, b.x + 26, y, b.x + 34, y + 4);
        SDL_RenderDrawLine(r, b.x + 34, y + 4, b.x + b.w - 10, y);
      }
    } break;

    case CellType::Oil: {
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + 14, cy);
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + b.w - 14, cy);
      SDL_RenderDrawLine(r, b.x + 14, cy, cx, b.y + b.h - 10);
      SDL_RenderDrawLine(r, b.x + b.w - 14, cy, cx, b.y + b.h - 10);
    } break;

    case CellType::Fire: {
      SDL_RenderDrawLine(r, cx, b.y + 10, cx - 10, b.y + b.h - 12);
      SDL_RenderDrawLine(r, cx, b.y + 10, cx + 10, b.y + b.h - 12);
      SDL_RenderDrawLine(r, cx - 10, b.y + b.h - 12, cx, b.y + b.h - 6);
      SDL_RenderDrawLine(r, cx + 10, b.y + b.h - 12, cx, b.y + b.h - 6);
      SDL_RenderDrawLine(r, cx - 3, b.y + 14, cx - 12, b.y + b.h - 14);
      SDL_RenderDrawLine(r, cx + 3, b.y + 14, cx + 12, b.y + b.h - 14);
    } break;

    case CellType::Smoke: {
      for (int i = 0; i < 3; ++i) {
        int x = b.x + 14 + i * 8;
        SDL_RenderDrawLine(r, x, b.y + b.h - 12, x + 4, b.y + b.h - 22);
        SDL_RenderDrawLine(r, x + 4, b.y + b.h - 22, x, b.y + b.h - 32);
      }
    } break;

    case CellType::Lava: {
      SDL_RenderDrawLine(r, b.x + 12, cy, b.x + 22, cy - 8);
      SDL_RenderDrawLine(r, b.x + 22, cy - 8, b.x + 30, cy + 6);
      SDL_RenderDrawLine(r, b.x + 30, cy + 6, b.x + b.w - 12, cy - 4);
      SDL_Rect dot{cx - 2, cy - 2, 4, 4};
      SDL_RenderFillRect(r, &dot);
    } break;

    case CellType::Wall: {
      SDL_Rect inner{b.x + 10, b.y + 12, b.w - 20, b.h - 24};
      SDL_RenderDrawRect(r, &inner);
      SDL_RenderDrawLine(r, inner.x, inner.y + inner.h / 2, inner.x + inner.w, inner.y + inner.h / 2);
      SDL_RenderDrawLine(r, inner.x + inner.w / 2, inner.y, inner.x + inner.w / 2, inner.y + inner.h / 2);
      SDL_RenderDrawLine(r, inner.x + inner.w / 3, inner.y + inner.h / 2, inner.x + inner.w / 3, inner.y + inner.h);
      SDL_RenderDrawLine(r, inner.x + 2 * inner.w / 3, inner.y + inner.h / 2, inner.x + 2 * inner.w / 3, inner.y + inner.h);
    } break;

    // i just can't stop myself from drawing these idiot gryphs
    case CellType::Stone: {
      SDL_RenderDrawLine(r, b.x + 11, cy + 8, cx - 7, b.y + 15);
      SDL_RenderDrawLine(r, cx - 7, b.y + 15, cx + 8, b.y + 12);
      SDL_RenderDrawLine(r, cx + 8, b.y + 12, b.x + b.w - 11, cy + 4);
      SDL_RenderDrawLine(r, b.x + b.w - 11, cy + 4, cx + 4, b.y + b.h - 11);
      SDL_RenderDrawLine(r, cx + 4, b.y + b.h - 11, b.x + 14, b.y + b.h - 14);
      SDL_RenderDrawLine(r, b.x + 14, b.y + b.h - 14, b.x + 11, cy + 8);
      SDL_RenderDrawLine(r, cx - 3, cy - 2, cx + 2, cy + 3);
    } break;

    case CellType::Empty: {
      SDL_RenderDrawLine(r, b.x + 12, b.y + 12, b.x + b.w - 12, b.y + b.h - 12);
      SDL_RenderDrawLine(r, b.x + b.w - 12, b.y + 12, b.x + 12, b.y + b.h - 12);
    } break;

  }
}

std::expected<RendererSDL, std::string> RendererSDL::create(int gw, int gh, int s, int toolbar_h_px) {
  return render_sdl_detail::create_with_api(gw, gh, s, toolbar_h_px, render_sdl_detail::sdl_api());
}

RendererSDL::~RendererSDL() {
  destroy();
}

RendererSDL::RendererSDL(RendererSDL&& other) noexcept
    : window(std::exchange(other.window, nullptr)),
      renderer(std::exchange(other.renderer, nullptr)),
      texture(std::exchange(other.texture, nullptr)),
      owns_sdl_video(std::exchange(other.owns_sdl_video, false)),
      scale(other.scale),
      grid_w(other.grid_w),
      grid_h(other.grid_h),
      toolbar_h(other.toolbar_h),
      win_w(other.win_w),
      win_h(other.win_h),
      pixels(std::move(other.pixels)) {}

RendererSDL& RendererSDL::operator=(RendererSDL&& other) noexcept {
  if (this == &other) return *this;

  destroy();

  window = std::exchange(other.window, nullptr);
  renderer = std::exchange(other.renderer, nullptr);
  texture = std::exchange(other.texture, nullptr);
  owns_sdl_video = std::exchange(other.owns_sdl_video, false);
  scale = other.scale;
  grid_w = other.grid_w;
  grid_h = other.grid_h;
  toolbar_h = other.toolbar_h;
  win_w = other.win_w;
  win_h = other.win_h;
  pixels = std::move(other.pixels);
  return *this;
}

void RendererSDL::destroy() {
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);

  texture = nullptr;
  renderer = nullptr;
  window = nullptr;

  if (owns_sdl_video && SDL_WasInit(SDL_INIT_VIDEO) != 0u) SDL_Quit();
  owns_sdl_video = false;
}

void RendererSDL::set_title(const std::string& title) {
  if (window) SDL_SetWindowTitle(window, title.c_str());
}

bool RendererSDL::mouse_to_grid(int mx, int my, int& gx, int& gy) const {
  if (mx < 0 || mx >= win_w) return false;
  if (my < toolbar_h || my >= win_h) return false;
  const int rel_y = my - toolbar_h;
  gx = mx / scale;
  gy = rel_y / scale;
  if (gx < 0 || gx >= grid_w || gy < 0 || gy >= grid_h) return false;
  return true;
}

std::optional<CellType> RendererSDL::hit_test_toolbar(int mx, int my) const {
  if (my < 0 || my >= toolbar_h) return std::nullopt;
  const auto mats = materials();
  for (int i = 0; i < static_cast<int>(mats.size()); ++i) {
    SDL_Rect b = toolbar_button_rect(i, toolbar_h);
    if (mx >= b.x && mx < (b.x + b.w) && my >= b.y && my < (b.y + b.h)) {
      return mats[i].type;
    }
  }
  return std::nullopt;
}

bool RendererSDL::hit_test_clear_button(int mx, int my) const {
  if (my < 0 || my >= toolbar_h) return false;
  const SDL_Rect b = clear_button_rect(win_w, toolbar_h);
  return mx >= b.x && mx < (b.x + b.w) && my >= b.y && my < (b.y + b.h);
}

bool RendererSDL::hit_test_random_button(int mx, int my) const {
  if (my < 0 || my >= toolbar_h) return false;
  const SDL_Rect b = random_button_rect(win_w, toolbar_h);
  return mx >= b.x && mx < (b.x + b.w) && my >= b.y && my < (b.y + b.h);
}

/**
 * @brief Converts material enum value into toolbar display text.
 * @param t Material type.
 * @return Uppercase material label.
 */
static std::string mat_name(CellType t) {
  for (const auto& m : kMaterialButtons) {
    if (m.type == t) return m.name;
  }
  return "UNKNOWN";
}

void RendererSDL::draw(const World& world,
                       CellType selected,
                       int brush_radius,
                       int mouse_x,
                       int mouse_y,
                       bool paused,
                       DebugView debug_view,
                       int fps) {
  const bool show_pressure_debug = (debug_view == DebugView::Pressure);
  const bool show_load_debug = (debug_view == DebugView::Load);
  const int load_range_min = kLoadDebugRangeMin;
  const int load_range_max = kLoadDebugRangeMax;

  for (int y = 0; y < grid_h; ++y) {
    for (int x = 0; x < grid_w; ++x) {
      const Cell& c = world.at(x, y);
      pixels[static_cast<std::size_t>(y * grid_w + x)] =
          show_pressure_debug ? render_core::color_for_pressure_debug(c)
                              : show_load_debug ? render_core::color_for_load_debug(c, load_range_min, load_range_max)
                                                : render_core::color_for_cell_animated(c, x, y, world.tick_count);
    }
  }
  SDL_UpdateTexture(texture, nullptr, pixels.data(), grid_w * int(sizeof(std::uint32_t)));

  set_draw_color(renderer, kToolbarTheme.frame_bg);
  SDL_RenderClear(renderer);

  set_draw_color(renderer, kToolbarTheme.toolbar_bg);
  SDL_Rect bar{0, 0, win_w, toolbar_h};
  SDL_RenderFillRect(renderer, &bar);

  set_draw_color(renderer, kToolbarTheme.toolbar_border);
  SDL_RenderDrawLine(renderer, 0, toolbar_h - 1, win_w, toolbar_h - 1);

  std::optional<CellType> hovered = hit_test_toolbar(mouse_x, mouse_y);
  const bool random_hovered = hit_test_random_button(mouse_x, mouse_y);
  const bool clear_hovered = hit_test_clear_button(mouse_x, mouse_y);

  const auto mats = materials();
  int tools_right = 12;
  for (int i = 0; i < static_cast<int>(mats.size()); ++i) {
    SDL_Rect b = toolbar_button_rect(i, toolbar_h);
    tools_right = std::max(tools_right, b.x + b.w);

    const bool is_sel = (mats[i].type == selected);
    const bool is_hover = hovered.has_value() && hovered.value() == mats[i].type;

    const std::uint32_t bgc = is_sel ? kToolbarTheme.button_selected_bg
                                     : is_hover ? kToolbarTheme.button_hover_bg
                                                : kToolbarTheme.button_bg;
    set_draw_color(renderer, bgc);
    SDL_RenderFillRect(renderer, &b);

    const std::uint32_t sw = material_swatch_color(mats[i].type);
    SDL_Rect strip{b.x, b.y + b.h - kToolbarLayout.material_strip_h, b.w, kToolbarLayout.material_strip_h};
    set_draw_color(renderer, sw);
    SDL_RenderFillRect(renderer, &strip);

    set_draw_color(renderer, is_sel ? kToolbarTheme.button_selected_border : kToolbarTheme.button_border);
    SDL_RenderDrawRect(renderer, &b);

    draw_icon(renderer, b, mats[i].type, kToolbarTheme.icon_fg);
  }

  // Draw right-side action buttons.
  const SDL_Rect random_btn = random_button_rect(win_w, toolbar_h);
  const std::uint32_t random_bg = random_hovered ? kToolbarTheme.random_hover_bg : kToolbarTheme.random_bg;
  set_draw_color(renderer, random_bg);
  SDL_RenderFillRect(renderer, &random_btn);
  SDL_Rect random_top{random_btn.x + 1, random_btn.y + 1, random_btn.w - 2, kToolbarLayout.panel_top_band_h};
  set_draw_color(renderer, random_hovered ? kToolbarTheme.random_hover_top_bg : kToolbarTheme.random_top_bg);
  SDL_RenderFillRect(renderer, &random_top);
  set_draw_color(renderer, kToolbarTheme.random_border);
  SDL_RenderDrawRect(renderer, &random_btn);
  draw_text(renderer, random_btn.x + 10, random_btn.y + 12, "RANDOM", 2, kToolbarTheme.random_text);

  const SDL_Rect clear_btn = clear_button_rect(win_w, toolbar_h);
  const std::uint32_t clear_bg = clear_hovered ? kToolbarTheme.clear_hover_bg : kToolbarTheme.clear_bg;
  set_draw_color(renderer, clear_bg);
  SDL_RenderFillRect(renderer, &clear_btn);
  SDL_Rect clear_top{clear_btn.x + 1, clear_btn.y + 1, clear_btn.w - 2, kToolbarLayout.panel_top_band_h};
  set_draw_color(renderer, clear_hovered ? kToolbarTheme.clear_hover_top_bg : kToolbarTheme.clear_top_bg);
  SDL_RenderFillRect(renderer, &clear_top);
  set_draw_color(renderer, kToolbarTheme.clear_border);
  SDL_RenderDrawRect(renderer, &clear_btn);
  draw_text(renderer, clear_btn.x + 18, clear_btn.y + 12, "RESET", 2, kToolbarTheme.clear_text);

  std::string hover_text;
  if (random_hovered) {
    hover_text = "GENERATE RANDOM START [R]";
  } else if (clear_hovered) {
    hover_text = "RESET WORLD [C]";
  } else if (hovered.has_value()) {
    for (const auto& m : mats) {
      if (m.type == hovered.value()) {
        hover_text = std::string("TOOL: ") + m.name + " " + m.hotkey;
        break;
      }
    }
  } else {
    hover_text = "ACTIVE: " + mat_name(selected) + "  BRUSH " + std::to_string(brush_radius) +
                 (paused ? "  [PAUSED]" : "") +
                 (show_pressure_debug ? "  [PRESSURE]" : show_load_debug ? "  [LOAD]" : "");
  }

  const int panel_x = tools_right + kToolbarLayout.panel_gap;
  const int actions_left = std::min(random_btn.x, clear_btn.x);
  const int panel_w = std::max(kToolbarLayout.panel_min_width, actions_left - kToolbarLayout.panel_gap - panel_x);
  SDL_Rect status_panel{panel_x, kToolbarLayout.button_margin_y, panel_w, toolbar_h - (2 * kToolbarLayout.button_margin_y)};
  set_draw_color(renderer, kToolbarTheme.panel_bg);
  SDL_RenderFillRect(renderer, &status_panel);
  SDL_Rect status_top{status_panel.x + 1, status_panel.y + 1, status_panel.w - 2, kToolbarLayout.panel_top_band_h};
  set_draw_color(renderer, kToolbarTheme.panel_top_bg);
  SDL_RenderFillRect(renderer, &status_top);
  set_draw_color(renderer, kToolbarTheme.panel_border);
  SDL_RenderDrawRect(renderer, &status_panel);

  const int max_chars = std::max(0, (status_panel.w - (2 * kToolbarLayout.panel_inset)) / 12);
  if (static_cast<int>(hover_text.size()) > max_chars) {
    hover_text.resize(static_cast<std::size_t>(max_chars));
  }
  draw_text(renderer,
            status_panel.x + kToolbarLayout.panel_inset,
            status_panel.y + kToolbarLayout.status_line1_y,
            hover_text,
            2,
            kToolbarTheme.status_text);

  std::string legend = "TICK: " + std::to_string(world.tick_count) + "  FPS: " + std::to_string(std::max(0, fps));
  if (show_pressure_debug || show_load_debug) {
    legend = show_load_debug
                             ? "LOAD: DENSE STRESS FIXED [" + std::to_string(load_range_min) + ".." +
                                   std::to_string(load_range_max) + "]"
                             : "PRESSURE: Y=SMOKE  C=LIQ  O=LAVA";
    int gx = 0;
    int gy = 0;
    if (mouse_to_grid(mouse_x, mouse_y, gx, gy)) {
      const Cell& hc = world.at(gx, gy);
      legend = "CELL " + mat_name(hc.type) + " T" + std::to_string(static_cast<int>(hc.temp)) +
               " P" + std::to_string(static_cast<int>(hc.pressure)) +
               " L" + std::to_string(static_cast<int>(hc.load));
    }
  }
  const int legend_max_chars = std::max(0, (status_panel.w - (2 * kToolbarLayout.panel_inset)) / 6);
  if (static_cast<int>(legend.size()) > legend_max_chars) {
    legend.resize(static_cast<std::size_t>(legend_max_chars));
  }
  draw_text(renderer,
            status_panel.x + kToolbarLayout.panel_inset,
            status_panel.y + kToolbarLayout.status_line2_y,
            legend,
            1,
            kToolbarTheme.legend_text);

  SDL_Rect dst{0, toolbar_h, grid_w * scale, grid_h * scale};
  SDL_RenderCopy(renderer, texture, nullptr, &dst);

  SDL_RenderPresent(renderer);
}
