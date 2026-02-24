#include "render_core.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace {

enum class ColorModel : std::uint8_t {
  Fallback,
  Solid,
  WallHeatTint,
  SandHeatTint,
  OilHeatTint,
  WaterShimmer,
  SmokeSteam,
  FireGlow,
  LavaGlow,
};

struct MaterialColorStyle {
  ColorModel model{ColorModel::Fallback};
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
  int max_value{240};
  bool use_load_field{false};
};

constexpr std::array<MaterialColorStyle, kCellTypeCount> kMaterialColorStyles{{
    {ColorModel::Solid, 0, 0, 0, 20},              // Empty
    {ColorModel::WallHeatTint, 100, 100, 110, 20}, // Wall
    {ColorModel::SandHeatTint, 210, 185, 85, 20},  // Sand
    {ColorModel::WaterShimmer, 70, 125, 235, 20},  // Water
    {ColorModel::OilHeatTint, 35, 35, 45, 20},     // Oil
    {ColorModel::FireGlow, 220, 90, 25, 300},      // Fire
    {ColorModel::SmokeSteam, 140, 140, 150, 20},   // Smoke
    {ColorModel::LavaGlow, 185, 50, 12, 800},      // Lava
}};

constexpr std::array<PressureDebugStyle, kCellTypeCount> kPressureDebugStyles{{
    {PressurePalette::Empty, 1, false},       // Empty
    {PressurePalette::NeutralWall, 1, true},  // Wall
    {PressurePalette::SandEarth, 320, true},  // Sand
    {PressurePalette::LiquidCool, 320, true}, // Water
    {PressurePalette::LiquidCool, 320, true}, // Oil
    {PressurePalette::FireWarm, 180, false},  // Fire
    {PressurePalette::SmokeWarm, 220, false}, // Smoke
    {PressurePalette::LavaHot, 420, true},    // Lava
}};

constexpr MaterialColorStyle material_color_style(CellType t) noexcept {
  if (!is_valid_cell_type(t)) {
    return MaterialColorStyle{};
  }
  return kMaterialColorStyles[static_cast<std::size_t>(t)];
}

constexpr PressureDebugStyle pressure_debug_style(CellType t) noexcept {
  if (!is_valid_cell_type(t)) {
    return PressureDebugStyle{};
  }
  return kPressureDebugStyles[static_cast<std::size_t>(t)];
}

constexpr std::uint8_t clamp_u8(int v) noexcept {
  return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

constexpr std::uint8_t triangle8(std::uint32_t phase) noexcept {
  const std::uint32_t p = phase & 0xFFu;
  return static_cast<std::uint8_t>((p < 128u) ? (p * 2u) : ((255u - p) * 2u));
}

}  // namespace

namespace render_core {

std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
  return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b);
}

std::uint32_t color_for_cell(const Cell& c) noexcept {
  auto heat = [](int t) -> std::uint8_t {
    int v = (t - 20) / 4;
    v = (v < 0) ? 0 : (v > 60 ? 60 : v);
    return static_cast<std::uint8_t>(v);
  };
  const std::uint8_t h = heat(c.temp);
  const MaterialColorStyle style = material_color_style(c.type);

  switch (style.model) {
    case ColorModel::Fallback:
    default:
      return argb(255, 255, 0, 255);
    case ColorModel::Solid:
      return argb(255, style.r, style.g, style.b);
    case ColorModel::WallHeatTint: {
      const std::uint8_t glow = static_cast<std::uint8_t>(std::min<int>(h * 2, 120));
      return argb(255,
                  static_cast<std::uint8_t>(style.r + glow / 2),
                  static_cast<std::uint8_t>(style.g + glow / 4),
                  static_cast<std::uint8_t>(style.b - std::min<int>(glow / 6, style.b / 3)));
    }
    case ColorModel::SandHeatTint: {
      const std::uint8_t glow = static_cast<std::uint8_t>(std::min<int>(h * 2, 100));
      return argb(255,
                  static_cast<std::uint8_t>(style.r + glow / 3),
                  static_cast<std::uint8_t>(style.g + glow / 10),
                  static_cast<std::uint8_t>(std::max<int>(15, style.b - glow / 2)));
    }
    case ColorModel::OilHeatTint: {
      const std::uint8_t glow = static_cast<std::uint8_t>(std::min<int>(h * 2, 110));
      return argb(255,
                  static_cast<std::uint8_t>(style.r + glow),
                  static_cast<std::uint8_t>(style.g + (glow * 3) / 4),
                  static_cast<std::uint8_t>(style.b + glow / 4));
    }
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
  }
}

std::uint32_t color_for_cell_animated(const Cell& c, int x, int y, std::uint64_t tick) noexcept {
  if (c.type != CellType::Lava) return color_for_cell(c);

  auto heat = [](int t) -> std::uint8_t {
    int v = (t - 20) / 4;
    v = (v < 0) ? 0 : (v > 60 ? 60 : v);
    return static_cast<std::uint8_t>(v);
  };

  const MaterialColorStyle style = material_color_style(c.type);
  const std::uint8_t h = heat(c.temp);
  const int flow_dir = static_cast<int>(c.flow_dir);
  const int flow_strength = std::clamp<int>(static_cast<int>(c.flow_strength), 0, 4);

  const int dir_bias = (flow_dir == 0) ? 2 : (flow_dir > 0 ? 5 : -5);
  const std::uint32_t fold_phase = static_cast<std::uint32_t>((tick * 3u) + (x * dir_bias) + (y * 7) +
                                                              (flow_strength * 13));
  const std::uint32_t pulse_phase =
      static_cast<std::uint32_t>((tick * static_cast<std::uint64_t>(4 + flow_strength)) + (x * 17) - (y * 11) +
                                 (flow_dir * 19));
  const int fold = static_cast<int>(triangle8(fold_phase));   // 0..254
  const int pulse = static_cast<int>(triangle8(pulse_phase)); // 0..254

  const int mobility = std::clamp((static_cast<int>(c.temp) - 180) / 18, 0, 40);
  const int flop_amp = 8 + (mobility / 2) + (flow_strength * 4);
  const int pulse_amp = 4 + (mobility / 6) + (flow_strength * 2);
  const int fold_signed = ((fold - 127) * flop_amp) / 127;
  const int pulse_signed = ((pulse - 127) * pulse_amp) / 127;
  const int bright = std::max(0, fold_signed) + std::max(0, pulse_signed / 2);
  const int dark = std::max(0, -fold_signed / 2) + std::max(0, -pulse_signed / 3);

  return argb(255,
              clamp_u8(static_cast<int>(style.r) + h + bright - dark / 2),
              clamp_u8(static_cast<int>(style.g) + (h / 2) + bright / 2 - dark / 3),
              clamp_u8(static_cast<int>(style.b) + std::max(0, pulse_signed / 3)));
}

std::uint32_t color_for_pressure_debug(const Cell& c) noexcept {
  auto norm255 = [](int p, int max_p) -> std::uint8_t {
    const int v = std::clamp((p * 255) / std::max(1, max_p), 0, 255);
    return static_cast<std::uint8_t>(v);
  };
  auto smoke_pressure_color = [](std::uint8_t t) -> std::uint32_t {
    return argb(255,
                static_cast<std::uint8_t>(90 + (t * 165) / 255),
                static_cast<std::uint8_t>(70 + (t * 185) / 255),
                static_cast<std::uint8_t>(35 + (t * 90) / 255));
  };
  auto liquid_pressure_color = [](std::uint8_t t) -> std::uint32_t {
    return argb(255,
                static_cast<std::uint8_t>(10 + (t * 120) / 255),
                static_cast<std::uint8_t>(40 + (t * 190) / 255),
                static_cast<std::uint8_t>(80 + (t * 175) / 255));
  };
  auto lava_pressure_color = [](std::uint8_t t) -> std::uint32_t {
    const std::uint8_t t2 = static_cast<std::uint8_t>((int(t) * int(t)) / 255);
    return argb(255,
                static_cast<std::uint8_t>(70 + (t * 175) / 255),
                static_cast<std::uint8_t>(18 + (t2 * 185) / 255),
                static_cast<std::uint8_t>(8 + (t2 * 44) / 255));
  };
  auto fire_pressure_color = [](std::uint8_t t) -> std::uint32_t {
    return argb(255,
                static_cast<std::uint8_t>(120 + (t * 135) / 255),
                static_cast<std::uint8_t>(30 + (t * 140) / 255),
                static_cast<std::uint8_t>(10 + (t * 40) / 255));
  };
  auto sand_pressure_color = [](std::uint8_t t) -> std::uint32_t {
    return argb(255,
                static_cast<std::uint8_t>(80 + (t * 120) / 255),
                static_cast<std::uint8_t>(70 + (t * 110) / 255),
                static_cast<std::uint8_t>(45 + (t * 60) / 255));
  };

  const PressureDebugStyle style = pressure_debug_style(c.type);
  const int debug_scalar = style.use_load_field ? static_cast<int>(c.load) : static_cast<int>(c.pressure);
  const int p = std::clamp(debug_scalar, 0, style.max_value);

  switch (style.palette) {
    case PressurePalette::Empty:
      return argb(255, 0, 0, 0);
    case PressurePalette::NeutralWall:
      return argb(255, 70, 70, 78);
    case PressurePalette::SmokeWarm:
      return smoke_pressure_color(norm255(p, style.max_value));
    case PressurePalette::LiquidCool:
      return liquid_pressure_color(norm255(p, style.max_value));
    case PressurePalette::LavaHot:
      return lava_pressure_color(norm255(p, style.max_value));
    case PressurePalette::FireWarm:
      return fire_pressure_color(norm255(p, style.max_value));
    case PressurePalette::SandEarth:
      return sand_pressure_color(norm255(p, style.max_value));
    case PressurePalette::Fallback:
    default:
      return argb(255, 255, 0, 255);
  }
}

std::uint32_t color_for_load_debug(const Cell& c, int range_min, int range_max) noexcept {
  if (c.type == CellType::Empty) return argb(255, 0, 0, 0);
  if (c.type == CellType::Smoke || c.type == CellType::Fire) return argb(255, 20, 20, 24);

  auto norm255 = [](int v, int lo, int hi) -> std::uint8_t {
    const int denom = std::max(1, hi - lo);
    const int clamped = std::clamp(v, lo, hi);
    const int scaled = ((clamped - lo) * 255) / denom;
    return static_cast<std::uint8_t>(scaled);
  };
  auto contrast = [](std::uint8_t t) -> std::uint8_t {
    return static_cast<std::uint8_t>((int(t) * int(t)) / 255);
  };

  const int raw = std::max(0, static_cast<int>(c.load));
  const std::uint8_t t = norm255(raw, range_min, range_max);
  const std::uint8_t tc = contrast(t);

  switch (c.type) {
    case CellType::Lava:
      return argb(255,
                  static_cast<std::uint8_t>(110 + (t * 145) / 255),
                  static_cast<std::uint8_t>(38 + (tc * 175) / 255),
                  static_cast<std::uint8_t>(12 + (tc * 56) / 255));
    case CellType::Wall:
      return argb(255,
                  static_cast<std::uint8_t>(40 + (tc * 120) / 255),
                  static_cast<std::uint8_t>(40 + (tc * 120) / 255),
                  static_cast<std::uint8_t>(46 + (tc * 110) / 255));
    case CellType::Sand:
      return argb(255,
                  static_cast<std::uint8_t>(55 + (t * 150) / 255),
                  static_cast<std::uint8_t>(45 + (tc * 135) / 255),
                  static_cast<std::uint8_t>(24 + (tc * 70) / 255));
    case CellType::Water:
    case CellType::Oil:
      return argb(255,
                  static_cast<std::uint8_t>(8 + (tc * 95) / 255),
                  static_cast<std::uint8_t>(30 + (t * 165) / 255),
                  static_cast<std::uint8_t>(60 + (t * 180) / 255));
    default:
      return argb(255, 255, 0, 255);
  }
}

std::uint32_t material_swatch_color(CellType t) noexcept {
  Cell tmp{};
  tmp.type = t;
  tmp.temp = material_color_style(t).swatch_temp;
  return color_for_cell(tmp);
}

}  // namespace render_core
