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

enum class MaterialCategory : std::uint8_t {
  Void,
  Solid,
  Liquid,
  Gas,
  Energy,
};

struct MaterialBehaviorProps {
  MaterialCategory category{MaterialCategory::Void}; /**< Broad behavioral family. */
  bool condenses_gas{false};      /**< Liquid can condense trapped gas into itself. */
  bool molten{false};             /**< Molten/hot-flowing phase. */
  bool rigid_support{false};      /**< Rigid support/load-bearing surface. */
  bool cooling_surface{false};    /**< Acts as a cooling/solidification interface. */
  bool static_obstacle{false};    /**< User-placed/static obstacle (e.g. wall). */

  constexpr bool is_gas() const noexcept { return category == MaterialCategory::Gas; }
  constexpr bool is_liquid() const noexcept { return category == MaterialCategory::Liquid; }
  constexpr bool is_solid() const noexcept { return category == MaterialCategory::Solid; }
};

struct MaterialDef {
  MaterialProps thermal{};               /**< Thermal properties. */
  MaterialBehaviorProps behavior{};      /**< Behavioral capabilities. */
};

namespace sim {

constexpr MaterialProps thermal(std::int16_t spawn_temp,
                                std::uint8_t conductivity,
                                std::uint8_t capacity,
                                std::int16_t equilibrium_temp,
                                std::uint8_t relax_step) {
  return MaterialProps{
      .spawn_temp = spawn_temp,
      .thermal_conductivity = conductivity,
      .heat_capacity = capacity,
      .thermal_equilibrium_temp = equilibrium_temp,
      .thermal_relax_step = relax_step,
  };
}

constexpr MaterialBehaviorProps behavior_category(MaterialCategory category) {
  return MaterialBehaviorProps{.category = category};
}

constexpr MaterialBehaviorProps with_condenses_gas(MaterialBehaviorProps b, bool value = true) {
  b.condenses_gas = value;
  return b;
}

constexpr MaterialBehaviorProps with_molten(MaterialBehaviorProps b, bool value = true) {
  b.molten = value;
  return b;
}

constexpr MaterialBehaviorProps with_rigid_support(MaterialBehaviorProps b, bool value = true) {
  b.rigid_support = value;
  return b;
}

constexpr MaterialBehaviorProps with_cooling_surface(MaterialBehaviorProps b, bool value = true) {
  b.cooling_surface = value;
  return b;
}

constexpr MaterialBehaviorProps with_static_obstacle(MaterialBehaviorProps b, bool value = true) {
  b.static_obstacle = value;
  return b;
}

constexpr MaterialDef make_material(MaterialProps t, MaterialBehaviorProps b) {
  return MaterialDef{.thermal = t, .behavior = b};
}

constexpr MaterialBehaviorProps void_behavior() { return behavior_category(MaterialCategory::Void); }
constexpr MaterialBehaviorProps solid_behavior() { return behavior_category(MaterialCategory::Solid); }
constexpr MaterialBehaviorProps liquid_behavior() { return behavior_category(MaterialCategory::Liquid); }
constexpr MaterialBehaviorProps gas_behavior() { return behavior_category(MaterialCategory::Gas); }
constexpr MaterialBehaviorProps energy_behavior() { return behavior_category(MaterialCategory::Energy); }

constexpr std::array<MaterialDef, kCellTypeCount> kMaterialDefs{{
    /* Empty */ MaterialDef{
        .thermal = thermal(20, 0, 1, 20, 0),
        .behavior = void_behavior(),
    },
    /* Wall  */ make_material(
        thermal(20, 3, 10, 20, 0),
        with_static_obstacle(with_cooling_surface(with_rigid_support(solid_behavior())))),
    /* Stone */ make_material(
        thermal(20, 3, 12, 20, 0),
        with_cooling_surface(with_rigid_support(solid_behavior()))),
    /* Sand  */ make_material(
        thermal(20, 2, 4, 20, 0),
        with_cooling_surface(solid_behavior())),
    /* Water */ make_material(
        thermal(20, 5, 7, 20, 0),
        with_cooling_surface(with_condenses_gas(liquid_behavior()))),
    /* Oil   */ make_material(
        thermal(20, 3, 5, 20, 0),
        liquid_behavior()),
    /* Fire  */ make_material(
        thermal(300, 5, 1, 340, 4),
        energy_behavior()),
    /* Smoke */ make_material(
        thermal(20, 2, 1, 20, 1),
        gas_behavior()),
    /* Lava  */ make_material(
        thermal(800, 5, 14, 880, 1),
        with_molten(liquid_behavior())),
}};

static_assert(kMaterialDefs.size() == kCellTypeCount);

constexpr const MaterialDef& material_def(CellType t) {
  const std::size_t idx = is_valid_cell_type(t) ? static_cast<std::size_t>(t) : 0u;
  return kMaterialDefs[idx];
}

constexpr const MaterialProps& material_props(CellType t) {
  return material_def(t).thermal;
}

constexpr const MaterialBehaviorProps& material_behavior(CellType t) {
  return material_def(t).behavior;
}

}  // namespace sim
