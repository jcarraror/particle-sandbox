/**
 * @file render_sdl.cpp
 * @brief SDL rendering implementation for world pixels and toolbar UI.
 */

#include "render_sdl.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

static std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b);

namespace {

constexpr std::size_t kCellTypeCount = 8;

constexpr std::size_t cell_type_index(CellType t) noexcept {
  return static_cast<std::size_t>(t);
}

enum class ColorModel : std::uint8_t {
  Solid,
  WaterShimmer,
  SmokeSteam,
  FireGlow,
  LavaGlow,
};

struct MaterialColorStyle {
  ColorModel model{ColorModel::Solid};
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};
  std::int16_t swatch_temp{20};
};

enum class PressurePalette : std::uint8_t {
  Empty,
  NeutralWall,
  SmokeWarm,
  LiquidCool,
  LavaHot,
  FireWarm,
  SandEarth,
  Fallback,
};

struct PressureDebugStyle {
  PressurePalette palette{PressurePalette::Fallback};
  int max_pressure{240};
};

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

constexpr std::array<MaterialColorStyle, kCellTypeCount> kMaterialColorStyles{{
    {ColorModel::Solid, 0, 0, 0, 20},          // Empty
    {ColorModel::Solid, 100, 100, 110, 20},    // Wall
    {ColorModel::Solid, 210, 185, 85, 20},     // Sand
    {ColorModel::WaterShimmer, 70, 125, 235, 20},  // Water
    {ColorModel::Solid, 35, 35, 45, 20},       // Oil
    {ColorModel::FireGlow, 220, 90, 25, 300},  // Fire
    {ColorModel::SmokeSteam, 140, 140, 150, 20},   // Smoke
    {ColorModel::LavaGlow, 185, 50, 12, 800},  // Lava
}};

constexpr std::array<PressureDebugStyle, kCellTypeCount> kPressureDebugStyles{{
    {PressurePalette::Empty, 1},         // Empty
    {PressurePalette::NeutralWall, 1},   // Wall
    {PressurePalette::SandEarth, 160},   // Sand
    {PressurePalette::LiquidCool, 200},  // Water
    {PressurePalette::LiquidCool, 200},  // Oil
    {PressurePalette::FireWarm, 180},    // Fire
    {PressurePalette::SmokeWarm, 220},   // Smoke
    {PressurePalette::LavaHot, 240},     // Lava
}};

constexpr MaterialColorStyle material_color_style(CellType t) noexcept {
  return kMaterialColorStyles[cell_type_index(t)];
}

constexpr PressureDebugStyle pressure_debug_style(CellType t) noexcept {
  return kPressureDebugStyles[cell_type_index(t)];
}

constexpr ToolbarLayout kToolbarLayout{};

const ToolbarTheme kToolbarTheme{
    .frame_bg = argb(255, 10, 10, 12),
    .toolbar_bg = argb(255, 18, 18, 22),
    .toolbar_border = argb(255, 70, 70, 85),
    .button_bg = argb(255, 24, 24, 30),
    .button_hover_bg = argb(255, 32, 32, 40),
    .button_selected_bg = argb(255, 40, 40, 50),
    .button_border = argb(255, 85, 85, 100),
    .button_selected_border = argb(255, 255, 255, 255),
    .icon_fg = argb(255, 220, 220, 230),
    .panel_bg = argb(255, 22, 24, 30),
    .panel_top_bg = argb(255, 32, 35, 45),
    .panel_border = argb(255, 70, 78, 100),
    .status_text = argb(255, 220, 226, 240),
    .legend_text = argb(255, 172, 186, 205),
    .random_bg = argb(255, 30, 56, 64),
    .random_hover_bg = argb(255, 42, 76, 88),
    .random_top_bg = argb(255, 48, 86, 96),
    .random_hover_top_bg = argb(255, 64, 114, 128),
    .random_border = argb(255, 105, 170, 185),
    .random_text = argb(255, 226, 242, 244),
    .clear_bg = argb(255, 64, 32, 32),
    .clear_hover_bg = argb(255, 88, 44, 44),
    .clear_top_bg = argb(255, 96, 48, 48),
    .clear_hover_top_bg = argb(255, 125, 64, 64),
    .clear_border = argb(255, 185, 105, 105),
    .clear_text = argb(255, 244, 226, 226),
};

}  // namespace

/**
 * @brief Packs channels into ARGB8888.
 * @param a Alpha channel.
 * @param r Red channel.
 * @param g Green channel.
 * @param b Blue channel.
 * @return Packed 32-bit color.
 */
static std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b);
}

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
static std::uint32_t color_for(const Cell& c) {
  auto heat = [](int t) -> std::uint8_t {
    int v = (t - 20) / 4;
    v = (v < 0) ? 0 : (v > 60 ? 60 : v);
    return static_cast<std::uint8_t>(v);
  };
  const std::uint8_t h = heat(c.temp);
  const MaterialColorStyle style = material_color_style(c.type);

  switch (style.model) {
    case ColorModel::Solid:
      return argb(255, style.r, style.g, style.b);
    case ColorModel::WaterShimmer: {
      const std::uint8_t shimmer = static_cast<std::uint8_t>(std::min<int>(h, 48));
      return argb(255,
                  static_cast<std::uint8_t>(style.r + shimmer / 3),
                  static_cast<std::uint8_t>(style.g + shimmer / 2),
                  static_cast<std::uint8_t>(style.b - shimmer / 4));
    }
    case ColorModel::SmokeSteam: {
      const std::uint8_t steam = static_cast<std::uint8_t>(std::min<int>(h, 55));
      return argb(255,
                  static_cast<std::uint8_t>(style.r + steam),
                  static_cast<std::uint8_t>(style.g + steam),
                  static_cast<std::uint8_t>(style.b + steam / 2));
    }
    case ColorModel::FireGlow:
      return argb(255, static_cast<std::uint8_t>(style.r + h / 2),
                  static_cast<std::uint8_t>(style.g + h), style.b);
    case ColorModel::LavaGlow:
      return argb(255, static_cast<std::uint8_t>(style.r + h),
                  static_cast<std::uint8_t>(style.g + h / 2), style.b);
    default:
      return argb(255, 255, 0, 255);
  }
}

static std::uint32_t color_for_pressure_debug(const Cell& c) {
  auto norm255 = [](int p, int max_p) -> std::uint8_t {
    const int v = std::clamp((p * 255) / std::max(1, max_p), 0, 255);
    return static_cast<std::uint8_t>(v);
  };

  const PressureDebugStyle style = pressure_debug_style(c.type);
  const int p = std::clamp<int>(c.pressure, 0, 240);

  switch (style.palette) {
    case PressurePalette::Empty:
      return argb(255, 0, 0, 0);
    case PressurePalette::NeutralWall:
      return argb(255, 70, 70, 78);
    case PressurePalette::SmokeWarm: {
      // Gas pressure: warm yellow/white for trapped steam pockets.
      const std::uint8_t t = norm255(p, style.max_pressure);
      const std::uint8_t r = static_cast<std::uint8_t>(90 + (t * 165) / 255);
      const std::uint8_t g = static_cast<std::uint8_t>(70 + (t * 185) / 255);
      const std::uint8_t b = static_cast<std::uint8_t>(35 + (t * 90) / 255);
      return argb(255, r, g, b);
    }
    case PressurePalette::LiquidCool: {
      // Liquid pressure: deep blue -> cyan -> pale white.
      const std::uint8_t t = norm255(p, style.max_pressure);
      const std::uint8_t r = static_cast<std::uint8_t>(10 + (t * 120) / 255);
      const std::uint8_t g = static_cast<std::uint8_t>(40 + (t * 190) / 255);
      const std::uint8_t b = static_cast<std::uint8_t>(80 + (t * 175) / 255);
      return argb(255, r, g, b);
    }
    case PressurePalette::LavaHot: {
      // Lava pressure: dark maroon -> red -> orange -> yellow
      const std::uint8_t t = norm255(p, style.max_pressure);
      const std::uint8_t t2 = static_cast<std::uint8_t>((int(t) * int(t)) / 255);
      const std::uint8_t r = static_cast<std::uint8_t>(24 + (t * 210) / 255);
      const std::uint8_t g = static_cast<std::uint8_t>(6 + (t2 * 180) / 255);
      const std::uint8_t b = static_cast<std::uint8_t>(4 + (t2 * 36) / 255);
      return argb(255, r, g, b);
    }
    case PressurePalette::FireWarm: {
      const std::uint8_t t = norm255(p, style.max_pressure);
      return argb(255,
                  static_cast<std::uint8_t>(120 + (t * 135) / 255),
                  static_cast<std::uint8_t>(30 + (t * 140) / 255),
                  static_cast<std::uint8_t>(10 + (t * 40) / 255));
    }
    case PressurePalette::SandEarth: {
      const std::uint8_t t = norm255(p, style.max_pressure);
      return argb(255,
                  static_cast<std::uint8_t>(80 + (t * 120) / 255),
                  static_cast<std::uint8_t>(70 + (t * 110) / 255),
                  static_cast<std::uint8_t>(45 + (t * 60) / 255));
    }
    default:
      return argb(255, 255, 0, 255);
  }
}

/**
 * @brief Glyph entry for the built-in 5x7 pixel font.
 */
struct Glyph {
  char c;
  std::array<std::uint8_t, 7> rows;
};

constexpr std::array<MaterialButton, 8> kMaterialButtons{{
    {CellType::Sand, "SAND", "[1]"},
    {CellType::Water, "WATER", "[2]"},
    {CellType::Oil, "OIL", "[3]"},
    {CellType::Fire, "FIRE", "[4]"},
    {CellType::Smoke, "SMOKE", "[5]"},
    {CellType::Lava, "LAVA", "[6]"},
    {CellType::Wall, "WALL", "[7]"},
    {CellType::Empty, "ERASE", "[0]"},
}};

/**
 * @brief Font table for UI text rendering.
 */
static constexpr std::array<Glyph, 45> FONT = {{
  {' ', {0, 0, 0, 0, 0, 0, 0}},
  {':', {0, 4, 4, 0, 4, 4, 0}},
  {'-', {0, 0, 0, 31, 0, 0, 0}},
  {'/', {1, 2, 4, 8, 16, 0, 0}},
  {'[', {14, 8, 8, 8, 8, 8, 14}},
  {']', {14, 2, 2, 2, 2, 2, 14}},

  {'0', {14, 17, 19, 21, 25, 17, 14}},
  {'1', {4, 12, 4, 4, 4, 4, 14}},
  {'2', {14, 17, 1, 2, 4, 8, 31}},
  {'3', {31, 2, 4, 2, 1, 17, 14}},
  {'4', {2, 6, 10, 18, 31, 2, 2}},
  {'5', {31, 16, 30, 1, 1, 17, 14}},
  {'6', {6, 8, 16, 30, 17, 17, 14}},
  {'7', {31, 1, 2, 4, 8, 8, 8}},
  {'8', {14, 17, 17, 14, 17, 17, 14}},
  {'9', {14, 17, 17, 15, 1, 2, 12}},

  {'A', {14, 17, 17, 31, 17, 17, 17}},
  {'B', {30, 17, 17, 30, 17, 17, 30}},
  {'C', {14, 17, 16, 16, 16, 17, 14}},
  {'D', {30, 17, 17, 17, 17, 17, 30}},
  {'E', {31, 16, 16, 30, 16, 16, 31}},
  {'F', {31, 16, 16, 30, 16, 16, 16}},
  {'G', {14, 17, 16, 23, 17, 17, 15}},
  {'H', {17, 17, 17, 31, 17, 17, 17}},
  {'I', {14, 4, 4, 4, 4, 4, 14}},
  {'J', {1, 1, 1, 1, 17, 17, 14}},
  {'K', {17, 18, 20, 24, 20, 18, 17}},
  {'L', {16, 16, 16, 16, 16, 16, 31}},
  {'M', {17, 27, 21, 21, 17, 17, 17}},
  {'N', {17, 25, 21, 19, 17, 17, 17}},
  {'O', {14, 17, 17, 17, 17, 17, 14}},
  {'P', {30, 17, 17, 30, 16, 16, 16}},
  {'Q', {14, 17, 17, 17, 21, 18, 13}},
  {'R', {30, 17, 17, 30, 20, 18, 17}},
  {'S', {15, 16, 16, 14, 1, 1, 30}},
  {'T', {31, 4, 4, 4, 4, 4, 4}},
  {'U', {17, 17, 17, 17, 17, 17, 14}},
  {'V', {17, 17, 17, 17, 17, 10, 4}},
  {'W', {17, 17, 17, 21, 21, 21, 10}},
  {'X', {17, 17, 10, 4, 10, 17, 17}},
  {'Y', {17, 17, 10, 4, 4, 4, 4}},
  {'Z', {31, 1, 2, 4, 8, 16, 31}},
}};

/**
 * @brief Finds a glyph in the built-in font table.
 * @param c Character to lookup.
 * @return Matching glyph or `nullptr` if unsupported.
 */
static const Glyph* glyph_for(char c) {
  for (const auto& g : FONT) {
    if (g.c == c) return &g;
  }
  return nullptr;
}

/**
 * @brief Draws a single character using the pixel font.
 */
static void draw_char(SDL_Renderer* r, int x, int y, char c, int scale, std::uint32_t color) {
  const Glyph* g = glyph_for(c);
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
  Cell tmp{};
  tmp.type = t;
  tmp.temp = material_color_style(t).swatch_temp;
  return color_for(tmp);
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

    case CellType::Empty: {
      SDL_RenderDrawLine(r, b.x + 12, b.y + 12, b.x + b.w - 12, b.y + b.h - 12);
      SDL_RenderDrawLine(r, b.x + b.w - 12, b.y + 12, b.x + 12, b.y + b.h - 12);
    } break;

    default: break;
  }
}

std::expected<RendererSDL, std::string> RendererSDL::create(int gw, int gh, int s, int toolbar_h_px) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return std::unexpected(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  RendererSDL r;
  r.owns_sdl_video = true;
  r.scale = (s <= 0) ? 1 : s;
  r.grid_w = gw;
  r.grid_h = gh;
  r.toolbar_h = (toolbar_h_px < 48) ? 48 : toolbar_h_px;

  r.win_w = r.grid_w * r.scale;
  r.win_h = r.toolbar_h + r.grid_h * r.scale;

  r.window = SDL_CreateWindow("Particle Sandbox (C++23)",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              r.win_w, r.win_h, SDL_WINDOW_SHOWN);
  if (!r.window) return std::unexpected(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

  r.renderer = SDL_CreateRenderer(r.window, -1, SDL_RENDERER_ACCELERATED);
  if (!r.renderer) return std::unexpected(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());

  r.texture = SDL_CreateTexture(r.renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, r.grid_w, r.grid_h);
  if (!r.texture) return std::unexpected(std::string("SDL_CreateTexture failed: ") + SDL_GetError());

  r.pixels.resize(static_cast<std::size_t>(r.grid_w * r.grid_h), 0);
  return r;
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
                       bool show_pressure_debug) {
  for (int y = 0; y < grid_h; ++y) {
    for (int x = 0; x < grid_w; ++x) {
      const Cell& c = world.at(x, y);
      pixels[static_cast<std::size_t>(y * grid_w + x)] =
          show_pressure_debug ? color_for_pressure_debug(c) : color_for(c);
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
                 (show_pressure_debug ? "  [PRESSURE]" : "");
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

  if (show_pressure_debug) {
    std::string legend = "PRESSURE: Y=SMOKE  C=LIQ  O=LAVA";
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
  }

  SDL_Rect dst{0, toolbar_h, grid_w * scale, grid_h * scale};
  SDL_RenderCopy(renderer, texture, nullptr, &dst);

  SDL_RenderPresent(renderer);
}
