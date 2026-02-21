#include "render_sdl.hpp"

#include <SDL.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

static std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b);
}

static void set_draw_color(SDL_Renderer* r, std::uint32_t c) {
  const std::uint8_t a = (c >> 24) & 0xFF;
  const std::uint8_t rr = (c >> 16) & 0xFF;
  const std::uint8_t g = (c >> 8) & 0xFF;
  const std::uint8_t b = (c >> 0) & 0xFF;
  SDL_SetRenderDrawColor(r, rr, g, b, a);
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
    case CellType::Wall:  return argb(255, 100, 100, 110);
    case CellType::Sand:  return argb(255, 210, 185, 85);
    case CellType::Water: return argb(255, 70, 125, 235);
    case CellType::Oil:   return argb(255, 35, 35, 45);
    case CellType::Smoke: return argb(255, 140, 140, 150);
    case CellType::Fire:  return argb(255, static_cast<std::uint8_t>(220 + h / 2),
                                      static_cast<std::uint8_t>(90 + h), 25);
    case CellType::Lava:  return argb(255, static_cast<std::uint8_t>(185 + h),
                                      static_cast<std::uint8_t>(50 + h / 2), 12);
    default:              return argb(255, 255, 0, 255);
  }
}

// ---------- Tiny 5x7 pixel font (ASCII subset) ----------
struct Glyph { char c; std::array<std::uint8_t, 7> rows; }; // 5 bits used per row

static constexpr std::array<Glyph, 45> FONT = {{
  {' ', {0,0,0,0,0,0,0}},
  {':', {0,4,4,0,4,4,0}},
  {'-', {0,0,0,31,0,0,0}},
  {'/', {1,2,4,8,16,0,0}},
  {'[', {14,8,8,8,8,8,14}},
  {']', {14,2,2,2,2,2,14}},

  {'0', {14,17,19,21,25,17,14}},
  {'1', {4,12,4,4,4,4,14}},
  {'2', {14,17,1,2,4,8,31}},
  {'3', {31,2,4,2,1,17,14}},
  {'4', {2,6,10,18,31,2,2}},
  {'5', {31,16,30,1,1,17,14}},
  {'6', {6,8,16,30,17,17,14}},
  {'7', {31,1,2,4,8,8,8}},
  {'8', {14,17,17,14,17,17,14}},
  {'9', {14,17,17,15,1,2,12}},

  {'A', {14,17,17,31,17,17,17}},
  {'B', {30,17,17,30,17,17,30}},
  {'C', {14,17,16,16,16,17,14}},
  {'D', {30,17,17,17,17,17,30}},
  {'E', {31,16,16,30,16,16,31}},
  {'F', {31,16,16,30,16,16,16}},
  {'G', {14,17,16,23,17,17,15}},
  {'H', {17,17,17,31,17,17,17}},
  {'I', {14,4,4,4,4,4,14}},
  {'J', {1,1,1,1,17,17,14}},
  {'K', {17,18,20,24,20,18,17}},
  {'L', {16,16,16,16,16,16,31}},
  {'M', {17,27,21,21,17,17,17}},
  {'N', {17,25,21,19,17,17,17}},
  {'O', {14,17,17,17,17,17,14}},
  {'P', {30,17,17,30,16,16,16}},
  {'Q', {14,17,17,17,21,18,13}},
  {'R', {30,17,17,30,20,18,17}},
  {'S', {15,16,16,14,1,1,30}},
  {'T', {31,4,4,4,4,4,4}},
  {'U', {17,17,17,17,17,17,14}},
  {'V', {17,17,17,17,17,10,4}},
  {'W', {17,17,17,21,21,21,10}},
  {'X', {17,17,10,4,10,17,17}},
  {'Y', {17,17,10,4,4,4,4}},
  {'Z', {31,1,2,4,8,16,31}},
}};

static const Glyph* glyph_for(char c) {
  for (const auto& g : FONT) if (g.c == c) return &g;
  return nullptr;
}

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

static void draw_text(SDL_Renderer* r, int x, int y, const std::string& s, int scale, std::uint32_t color) {
  int cx = x;
  for (char ch : s) {
    draw_char(r, cx, y, ch, scale, color);
    cx += 6 * scale; // 5px + 1px spacing
  }
}

// ---------- Toolbar layout + icons ----------

std::vector<MaterialButton> RendererSDL::materials() {
  return {
    {CellType::Sand,  "SAND",  "[1]"},
    {CellType::Water, "WATER", "[2]"},
    {CellType::Oil,   "OIL",   "[3]"},
    {CellType::Fire,  "FIRE",  "[4]"},
    {CellType::Smoke, "SMOKE", "[5]"},
    {CellType::Lava,  "LAVA",  "[6]"},
    {CellType::Wall,  "WALL",  "[7]"},
    {CellType::Empty, "ERASE", "[0]"},
  };
}

static std::uint32_t material_swatch_color(CellType t) {
  Cell tmp{};
  tmp.type = t;
  tmp.temp = (t == CellType::Fire) ? 300 : (t == CellType::Lava ? 800 : 20);
  return color_for(tmp);
}

static SDL_Rect toolbar_button_rect(int index, int toolbar_h) {
  const int pad = 12;
  const int size = toolbar_h - 24;
  const int gap = 10;
  const int x = pad + index * (size + gap);
  const int y = 12;
  return SDL_Rect{x, y, size, size};
}

static void draw_icon(SDL_Renderer* r, const SDL_Rect& b, CellType t, std::uint32_t fg) {
  set_draw_color(r, fg);

  const int cx = b.x + b.w / 2;
  const int cy = b.y + b.h / 2;

  // symbol icons using primitives
  switch (t) {
    case CellType::Sand: {
      // triangle pile
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + 10, b.y + b.h - 10);
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + b.w - 10, b.y + b.h - 10);
      SDL_RenderDrawLine(r, b.x + 10, b.y + b.h - 10, b.x + b.w - 10, b.y + b.h - 10);
    } break;

    case CellType::Water: {
      // wave lines
      for (int i = 0; i < 3; ++i) {
        int y = b.y + 12 + i * 8;
        SDL_RenderDrawLine(r, b.x + 10, y, b.x + 18, y + 4);
        SDL_RenderDrawLine(r, b.x + 18, y + 4, b.x + 26, y);
        SDL_RenderDrawLine(r, b.x + 26, y, b.x + 34, y + 4);
        SDL_RenderDrawLine(r, b.x + 34, y + 4, b.x + b.w - 10, y);
      }
    } break;

    case CellType::Oil: {
      // diamond
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + 14, cy);
      SDL_RenderDrawLine(r, cx, b.y + 10, b.x + b.w - 14, cy);
      SDL_RenderDrawLine(r, b.x + 14, cy, cx, b.y + b.h - 10);
      SDL_RenderDrawLine(r, b.x + b.w - 14, cy, cx, b.y + b.h - 10);
    } break;

    case CellType::Fire: {
      // three spikes
      SDL_RenderDrawLine(r, cx, b.y + 10, cx - 10, b.y + b.h - 12);
      SDL_RenderDrawLine(r, cx, b.y + 10, cx + 10, b.y + b.h - 12);
      SDL_RenderDrawLine(r, cx - 10, b.y + b.h - 12, cx, b.y + b.h - 6);
      SDL_RenderDrawLine(r, cx + 10, b.y + b.h - 12, cx, b.y + b.h - 6);
      SDL_RenderDrawLine(r, cx - 3, b.y + 14, cx - 12, b.y + b.h - 14);
      SDL_RenderDrawLine(r, cx + 3, b.y + 14, cx + 12, b.y + b.h - 14);
    } break;

    case CellType::Smoke: {
      // three rising arcs
      for (int i = 0; i < 3; ++i) {
        int x = b.x + 14 + i * 8;
        SDL_RenderDrawLine(r, x, b.y + b.h - 12, x + 4, b.y + b.h - 22);
        SDL_RenderDrawLine(r, x + 4, b.y + b.h - 22, x, b.y + b.h - 32);
      }
    } break;

    case CellType::Lava: {
      // cracked line + dot
      SDL_RenderDrawLine(r, b.x + 12, cy, b.x + 22, cy - 8);
      SDL_RenderDrawLine(r, b.x + 22, cy - 8, b.x + 30, cy + 6);
      SDL_RenderDrawLine(r, b.x + 30, cy + 6, b.x + b.w - 12, cy - 4);
      SDL_Rect dot{cx - 2, cy - 2, 4, 4};
      SDL_RenderFillRect(r, &dot);
    } break;

    case CellType::Wall: {
      // brick grid
      SDL_Rect inner{b.x + 10, b.y + 12, b.w - 20, b.h - 24};
      SDL_RenderDrawRect(r, &inner);
      SDL_RenderDrawLine(r, inner.x, inner.y + inner.h / 2, inner.x + inner.w, inner.y + inner.h / 2);
      SDL_RenderDrawLine(r, inner.x + inner.w / 2, inner.y, inner.x + inner.w / 2, inner.y + inner.h / 2);
      SDL_RenderDrawLine(r, inner.x + inner.w / 3, inner.y + inner.h / 2, inner.x + inner.w / 3, inner.y + inner.h);
      SDL_RenderDrawLine(r, inner.x + 2 * inner.w / 3, inner.y + inner.h / 2, inner.x + 2 * inner.w / 3, inner.y + inner.h);
    } break;

    case CellType::Empty: {
      // X
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

void RendererSDL::destroy() {
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  texture = nullptr;
  renderer = nullptr;
  window = nullptr;
  SDL_Quit();
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
  for (int i = 0; i < (int)mats.size(); ++i) {
    SDL_Rect b = toolbar_button_rect(i, toolbar_h);
    if (mx >= b.x && mx < (b.x + b.w) && my >= b.y && my < (b.y + b.h)) {
      return mats[i].type;
    }
  }
  return std::nullopt;
}

static std::string mat_name(CellType t) {
  switch (t) {
    case CellType::Sand: return "SAND";
    case CellType::Water: return "WATER";
    case CellType::Oil: return "OIL";
    case CellType::Fire: return "FIRE";
    case CellType::Smoke: return "SMOKE";
    case CellType::Lava: return "LAVA";
    case CellType::Wall: return "WALL";
    case CellType::Empty: return "ERASE";
    default: return "UNKNOWN";
  }
}

void RendererSDL::draw(const World& world,
                       CellType selected,
                       int brush_radius,
                       int mouse_x,
                       int mouse_y,
                       bool paused) {
  // update texture pixels (one pixel per cell)
  for (int y = 0; y < grid_h; ++y) {
    for (int x = 0; x < grid_w; ++x) {
      pixels[static_cast<std::size_t>(y * grid_w + x)] = color_for(world.at(x, y));
    }
  }
  SDL_UpdateTexture(texture, nullptr, pixels.data(), grid_w * int(sizeof(std::uint32_t)));

  // clear  window
  set_draw_color(renderer, argb(255, 10, 10, 12));
  SDL_RenderClear(renderer);

  // draw background
  set_draw_color(renderer, argb(255, 18, 18, 22));
  SDL_Rect bar{0, 0, win_w, toolbar_h};
  SDL_RenderFillRect(renderer, &bar);

  // bottom divider
  set_draw_color(renderer, argb(255, 70, 70, 85));
  SDL_RenderDrawLine(renderer, 0, toolbar_h - 1, win_w, toolbar_h - 1);

  // determine hovered button
  std::optional<CellType> hovered = hit_test_toolbar(mouse_x, mouse_y);

  // draw material buttons
  const auto mats = materials();
  for (int i = 0; i < (int)mats.size(); ++i) {
    SDL_Rect b = toolbar_button_rect(i, toolbar_h);

    const bool is_sel = (mats[i].type == selected);
    const bool is_hover = hovered.has_value() && hovered.value() == mats[i].type;

    // button bg
    const std::uint32_t bgc = is_sel ? argb(255, 40, 40, 50)
                          : is_hover ? argb(255, 32, 32, 40)
                                     : argb(255, 24, 24, 30);
    set_draw_color(renderer, bgc);
    SDL_RenderFillRect(renderer, &b);

    // swatch strip at bottom
    const std::uint32_t sw = material_swatch_color(mats[i].type);
    SDL_Rect strip{b.x, b.y + b.h - 6, b.w, 6};
    set_draw_color(renderer, sw);
    SDL_RenderFillRect(renderer, &strip);

    // outline
    set_draw_color(renderer, is_sel ? argb(255, 255, 255, 255) : argb(255, 85, 85, 100));
    SDL_RenderDrawRect(renderer, &b);

    // icon
    draw_icon(renderer, b, mats[i].type, argb(255, 220, 220, 230));
  }

  // hover info text
  std::string hover_text;
  if (hovered.has_value()) {
    // find matching
    for (const auto& m : mats) {
      if (m.type == hovered.value()) {
        hover_text = std::string(m.name) + " " + m.hotkey;
        break;
      }
    }
  } else {
    hover_text = mat_name(selected) + "  R:" + std::to_string(brush_radius) + (paused ? "  [PAUSED]" : "");
  }

  // draw status text
  draw_text(renderer, win_w - (int)hover_text.size() * 12 - 12, 18, hover_text, 2, argb(255, 220, 220, 230));

  // draw world texture toolbar
  SDL_Rect dst{0, toolbar_h, grid_w * scale, grid_h * scale};
  SDL_RenderCopy(renderer, texture, nullptr, &dst);

  SDL_RenderPresent(renderer);
}