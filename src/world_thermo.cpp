/**
 * @file world_thermo.cpp
 * @brief Thermal exchange pass implementation for the simulation world.
 */

#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "material_props.hpp"

namespace {

static constexpr int clampi(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

constexpr int kMinTemp = -50;
constexpr int kMaxTemp = 2000;
constexpr int kMinDeltaForTransfer = 4;
constexpr int kTransferDivisor = 256;
constexpr int kMaxEnergyTransferPerPair = 18;

std::size_t idx_of(const World& world, int x, int y) {
  return static_cast<std::size_t>(y * world.w + x);
}

void accumulate_heat_exchange(const World& world,
                              int ax,
                              int ay,
                              int bx,
                              int by,
                              std::vector<int>& temp_deltas) {
  const Cell& a = world.at(ax, ay);
  const Cell& b = world.at(bx, by);
  const auto& pa = sim::material_props(a.type);
  const auto& pb = sim::material_props(b.type);
  const int conductivity = std::min<int>(pa.thermal_conductivity, pb.thermal_conductivity);
  if (conductivity <= 0) return;

  const int delta = static_cast<int>(a.temp) - static_cast<int>(b.temp);
  const int abs_delta = std::abs(delta);
  if (abs_delta < kMinDeltaForTransfer) return;

  int energy = (abs_delta * conductivity) / kTransferDivisor;
  energy = std::max(1, energy);
  energy = std::min({energy, kMaxEnergyTransferPerPair, abs_delta});

  const int cap_a = std::max<int>(1, pa.heat_capacity);
  const int cap_b = std::max<int>(1, pb.heat_capacity);
  const int cap_sum = cap_a + cap_b;

  // Temperature response is inversely proportional to heat capacity.
  int dtemp_a = std::max(1, (energy * cap_b) / cap_sum);
  int dtemp_b = std::max(1, (energy * cap_a) / cap_sum);
  dtemp_a = std::min(dtemp_a, abs_delta);
  dtemp_b = std::min(dtemp_b, abs_delta);

  const std::size_t ia = idx_of(world, ax, ay);
  const std::size_t ib = idx_of(world, bx, by);
  if (delta > 0) {
    temp_deltas[ia] -= dtemp_a;
    temp_deltas[ib] += dtemp_b;
  } else {
    temp_deltas[ia] += dtemp_a;
    temp_deltas[ib] -= dtemp_b;
  }
}

}  // namespace

void World::pass_thermal_exchange() {
  std::vector<int> temp_deltas(cells.size(), 0);
  if (thermal_impulses.size() != cells.size()) {
    thermal_impulses.assign(cells.size(), 0);
  }

  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      const Cell& c = at(x, y);
      if (c.type == CellType::Empty) continue;

      if (x + 1 < w - 1) {
        const Cell& right = at(x + 1, y);
        if (right.type != CellType::Empty) {
          accumulate_heat_exchange(*this, x, y, x + 1, y, temp_deltas);
        }
      }
      if (y + 1 < h - 1) {
        const Cell& down = at(x, y + 1);
        if (down.type != CellType::Empty) {
          accumulate_heat_exchange(*this, x, y, x, y + 1, temp_deltas);
        }
      }
    }
  }

  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      Cell& c = at(x, y);
      const int exchanged = temp_deltas[idx_of(*this, x, y)];
      const int impulse = thermal_impulses[idx_of(*this, x, y)];
      const auto& props = sim::material_props(c.type);
      int next_temp =
          clampi(static_cast<int>(c.temp) + exchanged + impulse, kMinTemp, kMaxTemp);

      const int relax = static_cast<int>(props.thermal_relax_step);
      const int target = static_cast<int>(props.thermal_equilibrium_temp);
      if (relax > 0) {
        const bool exchange_pushed_hotter = (exchanged > 0) && (next_temp > target);
        const bool exchange_pushed_colder = (exchanged < 0) && (next_temp < target);
        // Preserve exchanged heat/cooling for at least one tick; otherwise small conduction
        // steps get immediately erased by equilibrium drift (e.g. hot crust -> water).
        if (!exchange_pushed_hotter && !exchange_pushed_colder) {
          if (next_temp > target) next_temp = std::max(target, next_temp - relax);
          else if (next_temp < target) next_temp = std::min(target, next_temp + relax);
        }
      }

      c.temp = static_cast<std::int16_t>(clampi(next_temp, kMinTemp, kMaxTemp));
    }
  }

  std::fill(thermal_impulses.begin(), thermal_impulses.end(), 0);
}
