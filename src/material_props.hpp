#pragma once

#include <array>
#include <cstdint>

#include "world.hpp"

/**
 * @file material_props.hpp
 * @brief Shared constexpr material property table used by simulation passes.
 */

struct MaterialProps {
  std::int16_t spawn_temp{20};         /**< Default temperature when spawned. */
  std::uint8_t thermal_conductivity{0}; /**< Relative local heat transfer strength. */
};

namespace sim {

constexpr std::array<MaterialProps, 8> kMaterialProps{{
    /* Empty */ {20, 0},
    /* Wall  */ {20, 1},
    /* Sand  */ {20, 1},
    /* Water */ {20, 4},
    /* Oil   */ {20, 2},
    /* Fire  */ {300, 5},
    /* Smoke */ {20, 1},
    /* Lava  */ {800, 6},
}};

static_assert(kMaterialProps.size() == 8);

constexpr const MaterialProps& material_props(CellType t) {
  return kMaterialProps[static_cast<std::size_t>(t)];
}

}  // namespace sim
