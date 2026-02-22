/**
 * @file world_thermo.cpp
 * @brief Thermal exchange pass implementation for the simulation world.
 */

#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "material_props.hpp"

namespace {

static constexpr int clampi(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

constexpr int kMinTemp = -50;
constexpr int kMaxTemp = 2000;
constexpr int kMinDeltaForTransfer = 4;
constexpr int kTransferDivisor = 256;
constexpr int kMaxTransferPerPair = 8;

void exchange_heat_pair(Cell& a, Cell& b) {
  const auto& pa = sim::material_props(a.type);
  const auto& pb = sim::material_props(b.type);
  const int conductivity = std::min<int>(pa.thermal_conductivity, pb.thermal_conductivity);
  if (conductivity <= 0) return;

  const int delta = static_cast<int>(a.temp) - static_cast<int>(b.temp);
  const int abs_delta = std::abs(delta);
  if (abs_delta < kMinDeltaForTransfer) return;

  int transfer = (abs_delta * conductivity) / kTransferDivisor;
  transfer = std::max(1, transfer);
  transfer = std::min({transfer, kMaxTransferPerPair, abs_delta / 2});
  if (transfer <= 0) return;

  if (delta > 0) {
    a.temp = static_cast<std::int16_t>(clampi(static_cast<int>(a.temp) - transfer, kMinTemp, kMaxTemp));
    b.temp = static_cast<std::int16_t>(clampi(static_cast<int>(b.temp) + transfer, kMinTemp, kMaxTemp));
  } else {
    a.temp = static_cast<std::int16_t>(clampi(static_cast<int>(a.temp) + transfer, kMinTemp, kMaxTemp));
    b.temp = static_cast<std::int16_t>(clampi(static_cast<int>(b.temp) - transfer, kMinTemp, kMaxTemp));
  }
}

}  // namespace

void World::pass_thermal_exchange() {
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);
      if (c.type == CellType::Empty) continue;

      if (x + 1 < w - 1) {
        Cell& right = at(x + 1, y);
        if (right.type != CellType::Empty) exchange_heat_pair(c, right);
      }
      if (y + 1 < h - 1) {
        Cell& down = at(x, y + 1);
        if (down.type != CellType::Empty) exchange_heat_pair(c, down);
      }
    }
  }
}
