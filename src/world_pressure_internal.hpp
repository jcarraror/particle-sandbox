#pragma once

#include <cstdint>

#include "world.hpp"

namespace pressure_detail {

[[nodiscard]] bool is_dense_pressure_material(CellType t) noexcept;

[[nodiscard]] int compute_smoke_pressure_target(const World& world, int x, int y, const Cell& c);

[[nodiscard]] int compute_dense_pressure_target(const World& world, int x, int y, const Cell& c);

[[nodiscard]] std::int16_t damp_pressure(std::int16_t current, int target) noexcept;

void relax_dense_pressure(World& world);

}  // namespace pressure_detail
