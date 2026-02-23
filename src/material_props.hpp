#pragma once

#include <array>
#include <cstdint>

#include "world.hpp"

/**
 * @file material_props.hpp
 * @brief Shared constexpr material property table used by simulation passes.
 */

struct MaterialProps {
  std::int16_t spawn_temp{20};           /**< Default temperature when spawned. */
  std::uint8_t thermal_conductivity{0};  /**< Relative local heat transfer strength. */
  std::uint8_t heat_capacity{1};         /**< Relative thermal inertia (higher = slower temp change). */
  std::uint8_t ambient_relax_step{0};    /**< Per-tick step toward ambient temperature. */
};

namespace sim {

constexpr std::array<MaterialProps, 8> kMaterialProps{{
    /* Empty */ {20, 0, 1, 0},
    /* Wall  */ {20, 1, 8, 0},
    /* Sand  */ {20, 1, 3, 1},
    /* Water */ {20, 4, 6, 1},
    /* Oil   */ {20, 2, 4, 1},
    /* Fire  */ {300, 5, 1, 1},
    /* Smoke */ {20, 1, 1, 1},
    /* Lava  */ {800, 6, 7, 1},
}};

static_assert(kMaterialProps.size() == 8);

constexpr const MaterialProps& material_props(CellType t) {
  return kMaterialProps[static_cast<std::size_t>(t)];
}

}  // namespace sim
