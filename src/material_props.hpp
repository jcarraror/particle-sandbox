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
  std::int16_t thermal_equilibrium_temp{20}; /**< Material's natural resting temperature. */
  std::uint8_t thermal_relax_step{0};        /**< Per-tick step toward `thermal_equilibrium_temp`. */
};

namespace sim {

constexpr std::array<MaterialProps, 8> kMaterialProps{{
    /* Empty */ {20, 0, 1, 20, 0},
    /* Wall  */ {20, 3, 10, 20, 0},
    /* Sand  */ {20, 2, 4, 20, 0},
    /* Water */ {20, 5, 7, 20, 0},
    /* Oil   */ {20, 3, 5, 20, 0},
    /* Fire  */ {300, 5, 1, 340, 4},
    /* Smoke */ {20, 2, 1, 20, 1},
    /* Lava  */ {800, 5, 14, 880, 1},
}};

static_assert(kMaterialProps.size() == 8);

constexpr const MaterialProps& material_props(CellType t) {
  const std::size_t idx = is_valid_cell_type(t) ? static_cast<std::size_t>(t) : 0u;
  return kMaterialProps[idx];
}

}  // namespace sim
