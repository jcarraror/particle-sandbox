#pragma once

#include <cstdint>

#include "world.hpp"

namespace render_core {

[[nodiscard]] std::uint32_t argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;

[[nodiscard]] std::uint32_t color_for_cell(const Cell& c) noexcept;
[[nodiscard]] std::uint32_t color_for_cell_animated(const Cell& c,
                                                    int x,
                                                    int y,
                                                    std::uint64_t tick) noexcept;

[[nodiscard]] std::uint32_t color_for_pressure_debug(const Cell& c) noexcept;

[[nodiscard]] std::uint32_t color_for_load_debug(const Cell& c, int range_min, int range_max) noexcept;

[[nodiscard]] std::uint32_t material_swatch_color(CellType t) noexcept;

}  // namespace render_core
