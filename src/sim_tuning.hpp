#pragma once

#include <cstdint>

/**
 * @file sim_tuning.hpp
 * @brief Structured simulation tuning constants used by world rules.
 */

namespace simcfg {

/** Shared temperature clamps/baselines used across rules. */
struct CommonTuning {
  int ambient_temp;
  int min_cell_temp;
  int max_neighbor_heat_temp;
};

/** Fire behavior, quenching, and decay tuning. */
struct FireTuning {
  std::uint32_t oil_ignite_odds_divisor;
  std::int16_t ignited_fire_temp;
  int self_heat_per_tick;
  int neighbor_heat_per_tick;
  int water_quench_per_neighbor;
  int min_sustain_temp;
  int max_temp;
  std::uint32_t to_smoke_odds_divisor;
  std::uint32_t extinguish_odds_divisor;
  std::int16_t smoke_temp;
};

/** Lava lateral mobility and flow-shaping tuning. */
struct LavaFlowTuning {
  int lateral_flow_low_temp;
  int lateral_flow_mid_temp;
  int lateral_flow_high_temp;
  int lateral_flow_very_high_temp;
  int momentum_assist_temp;
  std::uint32_t lateral_odds_cool;
  std::uint32_t lateral_odds_warm;
  std::uint32_t lateral_odds_hot;
  std::uint32_t lateral_odds_very_hot;
  int supported_flow_temp_threshold;
  int pressure_flow_threshold;
};

/** Lava solidification/crust growth tuning. */
struct LavaCrustTuning {
  int cooling_per_wall_neighbor;
  int cooling_per_wet_wall_neighbor;
  int temp_threshold;
  int fast_temp_threshold;
  std::uint32_t odds_divisor_base;
  int propagate_temp_threshold;
  std::uint32_t propagate_odds_divisor;
  int passive_solidify_temp_threshold;
  std::uint32_t passive_solidify_odds_divisor;
  std::uint32_t passive_solidify_near_wall_odds_divisor;
};

/** Direct lava-water contact effects (quench + steam). */
struct LavaWaterTuning {
  int cool_per_neighbor;
  int pressure_spike;
  std::uint32_t steam_burst_odds_divisor;
  int steam_burst_temp_threshold;
  int steam_spawn_pressure;
};

/** Lava thermal emission, smoke emission, and sub-config groups. */
struct LavaTuning {
  int self_heat_per_tick;
  int neighbor_heat_per_tick;
  int max_temp;
  LavaWaterTuning water;
  LavaCrustTuning crust;
  LavaFlowTuning flow;
  std::uint32_t smoke_spawn_odds_divisor;
  std::int16_t smoke_temp;
};

/** Water flow, heat absorption, and evaporation-to-steam tuning. */
struct WaterTuning {
  int spread;
  int heat_absorb_from_fire;
  int heat_absorb_from_lava;
  int steam_temp_threshold;
  std::uint32_t steam_odds_divisor;
  std::uint32_t steam_direct_lava_bonus_divisor;
  std::uint32_t steam_direct_fire_bonus_divisor;
  std::uint32_t steam_hot_wall_odds_divisor;
  int hot_wall_steam_temp_threshold;
};

/** Oil flow tuning (flammability is grouped under fire/oil ignition rules). */
struct OilTuning {
  int spread;
};

/** Smoke/steam bubbling, pressure-assisted movement, and condensation tuning. */
struct SmokeTuning {
  std::int16_t steam_temp;
  std::uint32_t bubble_swap_odds_divisor;
  std::uint32_t steam_bubble_swap_odds_divisor;
  int steam_smoke_temp_threshold;
  int high_pressure;
  int extreme_pressure;
  int condense_temp_threshold;
  std::uint32_t condense_odds_divisor;
};

/** Pressure-driven bonus spread tuning for constrained liquids. */
struct LiquidPressureTuning {
  int activation_threshold;
  int spread_bonus_step;
  int max_spread_bonus;
};

/** Top-level rule tuning bundle used by `world_rules.cpp`. */
struct SimTuning {
  CommonTuning common;
  FireTuning fire;
  LavaTuning lava;
  WaterTuning water;
  OilTuning oil;
  SmokeTuning smoke;
  LiquidPressureTuning liquid_pressure;
};

inline constexpr SimTuning kTuning{
    .common = {
        .ambient_temp = 20,
        .min_cell_temp = -50,
        .max_neighbor_heat_temp = 2000,
    },
    .fire = {
        .oil_ignite_odds_divisor = 7,
        .ignited_fire_temp = 300,
        .self_heat_per_tick = 5,
        .neighbor_heat_per_tick = 3,
        .water_quench_per_neighbor = 22,
        .min_sustain_temp = 110,
        .max_temp = 1200,
        .to_smoke_odds_divisor = 25,
        .extinguish_odds_divisor = 120,
        .smoke_temp = 80,
    },
    .lava = {
        .self_heat_per_tick = 2,
        .neighbor_heat_per_tick = 6,
        .max_temp = 2000,
        .water = {
            .cool_per_neighbor = 28,
            .pressure_spike = 70,
            .steam_burst_odds_divisor = 5,
            .steam_burst_temp_threshold = 150,
            .steam_spawn_pressure = 180,
        },
        .crust = {
            .cooling_per_wall_neighbor = 3,
            .cooling_per_wet_wall_neighbor = 6,
            .temp_threshold = 260,
            .fast_temp_threshold = 140,
            .odds_divisor_base = 7,
            .propagate_temp_threshold = 260,
            .propagate_odds_divisor = 6,
            .passive_solidify_temp_threshold = 90,
            .passive_solidify_odds_divisor = 120,
            .passive_solidify_near_wall_odds_divisor = 36,
        },
        .flow = {
            .lateral_flow_low_temp = 220,
            .lateral_flow_mid_temp = 420,
            .lateral_flow_high_temp = 720,
            .lateral_flow_very_high_temp = 950,
            .momentum_assist_temp = 380,
            .lateral_odds_cool = 9,
            .lateral_odds_warm = 5,
            .lateral_odds_hot = 3,
            .lateral_odds_very_hot = 1,
            .supported_flow_temp_threshold = 520,
            .pressure_flow_threshold = 120,
        },
        .smoke_spawn_odds_divisor = 80,
        .smoke_temp = 120,
    },
    .water = {
        .spread = 3,
        .heat_absorb_from_fire = 16,
        .heat_absorb_from_lava = 10,
        .steam_temp_threshold = 140,
        .steam_odds_divisor = 5,
        .steam_direct_lava_bonus_divisor = 2,
        .steam_direct_fire_bonus_divisor = 3,
        .steam_hot_wall_odds_divisor = 10,
        .hot_wall_steam_temp_threshold = 180,
    },
    .oil = {
        .spread = 4,
    },
    .smoke = {
        .steam_temp = 130,
        .bubble_swap_odds_divisor = 2,
        .steam_bubble_swap_odds_divisor = 1,
        .steam_smoke_temp_threshold = 120,
        .high_pressure = 80,
        .extreme_pressure = 150,
        .condense_temp_threshold = 55,
        .condense_odds_divisor = 8,
    },
    .liquid_pressure = {
        .activation_threshold = 90,
        .spread_bonus_step = 45,
        .max_spread_bonus = 3,
    },
};

// Flat compatibility aliases for existing call sites in world_rules.cpp.
inline constexpr int kAmbientTemp = kTuning.common.ambient_temp;
inline constexpr int kMinCellTemp = kTuning.common.min_cell_temp;
inline constexpr int kMaxNeighborHeatTemp = kTuning.common.max_neighbor_heat_temp;

inline constexpr std::uint32_t kOilIgniteOddsDivisor = kTuning.fire.oil_ignite_odds_divisor;
inline constexpr std::int16_t kIgnitedFireTemp = kTuning.fire.ignited_fire_temp;
inline constexpr int kFireSelfHeatPerTick = kTuning.fire.self_heat_per_tick;
inline constexpr int kFireNeighborHeatPerTick = kTuning.fire.neighbor_heat_per_tick;
inline constexpr int kFireWaterQuenchPerNeighbor = kTuning.fire.water_quench_per_neighbor;
inline constexpr int kFireMinSustainTemp = kTuning.fire.min_sustain_temp;
inline constexpr int kFireMaxTemp = kTuning.fire.max_temp;
inline constexpr std::uint32_t kFireToSmokeOddsDivisor = kTuning.fire.to_smoke_odds_divisor;
inline constexpr std::uint32_t kFireExtinguishOddsDivisor = kTuning.fire.extinguish_odds_divisor;
inline constexpr std::int16_t kSmokeFromFireTemp = kTuning.fire.smoke_temp;

inline constexpr int kLavaSelfHeatPerTick = kTuning.lava.self_heat_per_tick;
inline constexpr int kLavaNeighborHeatPerTick = kTuning.lava.neighbor_heat_per_tick;
inline constexpr int kLavaMaxTemp = kTuning.lava.max_temp;
inline constexpr int kLavaWaterCoolPerNeighbor = kTuning.lava.water.cool_per_neighbor;
inline constexpr int kLavaWaterPressureSpike = kTuning.lava.water.pressure_spike;
inline constexpr std::uint32_t kLavaWaterSteamBurstOddsDivisor = kTuning.lava.water.steam_burst_odds_divisor;
inline constexpr int kLavaSteamBurstTempThreshold = kTuning.lava.water.steam_burst_temp_threshold;
inline constexpr int kLavaSteamSpawnPressure = kTuning.lava.water.steam_spawn_pressure;
inline constexpr int kLavaCrustCoolingPerWallNeighbor = kTuning.lava.crust.cooling_per_wall_neighbor;
inline constexpr int kLavaCrustCoolingPerWetWallNeighbor = kTuning.lava.crust.cooling_per_wet_wall_neighbor;
inline constexpr int kLavaCrustTempThreshold = kTuning.lava.crust.temp_threshold;
inline constexpr int kLavaFastCrustTempThreshold = kTuning.lava.crust.fast_temp_threshold;
inline constexpr std::uint32_t kLavaCrustOddsDivisorBase = kTuning.lava.crust.odds_divisor_base;
inline constexpr int kLavaCrustPropagateTempThreshold = kTuning.lava.crust.propagate_temp_threshold;
inline constexpr std::uint32_t kLavaCrustPropagateOddsDivisor = kTuning.lava.crust.propagate_odds_divisor;
inline constexpr int kLavaPassiveSolidifyTempThreshold = kTuning.lava.crust.passive_solidify_temp_threshold;
inline constexpr std::uint32_t kLavaPassiveSolidifyOddsDivisor = kTuning.lava.crust.passive_solidify_odds_divisor;
inline constexpr std::uint32_t kLavaPassiveSolidifyNearWallOddsDivisor =
    kTuning.lava.crust.passive_solidify_near_wall_odds_divisor;
inline constexpr int kLavaLateralFlowLowTemp = kTuning.lava.flow.lateral_flow_low_temp;
inline constexpr int kLavaLateralFlowMidTemp = kTuning.lava.flow.lateral_flow_mid_temp;
inline constexpr int kLavaLateralFlowHighTemp = kTuning.lava.flow.lateral_flow_high_temp;
inline constexpr int kLavaLateralFlowVeryHighTemp = kTuning.lava.flow.lateral_flow_very_high_temp;
inline constexpr int kLavaMomentumAssistTemp = kTuning.lava.flow.momentum_assist_temp;
inline constexpr std::uint32_t kLavaLateralOddsCool = kTuning.lava.flow.lateral_odds_cool;
inline constexpr std::uint32_t kLavaLateralOddsWarm = kTuning.lava.flow.lateral_odds_warm;
inline constexpr std::uint32_t kLavaLateralOddsHot = kTuning.lava.flow.lateral_odds_hot;
inline constexpr std::uint32_t kLavaLateralOddsVeryHot = kTuning.lava.flow.lateral_odds_very_hot;
inline constexpr int kLavaSupportedFlowTempThreshold = kTuning.lava.flow.supported_flow_temp_threshold;
inline constexpr int kLavaPressureFlowThreshold = kTuning.lava.flow.pressure_flow_threshold;
inline constexpr std::uint32_t kLavaSmokeSpawnOddsDivisor = kTuning.lava.smoke_spawn_odds_divisor;
inline constexpr std::int16_t kSmokeFromLavaTemp = kTuning.lava.smoke_temp;

inline constexpr int kWaterSpread = kTuning.water.spread;
inline constexpr int kWaterHeatAbsorbFromFire = kTuning.water.heat_absorb_from_fire;
inline constexpr int kWaterHeatAbsorbFromLava = kTuning.water.heat_absorb_from_lava;
inline constexpr int kWaterSteamTempThreshold = kTuning.water.steam_temp_threshold;
inline constexpr std::uint32_t kWaterSteamOddsDivisor = kTuning.water.steam_odds_divisor;
inline constexpr std::uint32_t kWaterSteamDirectLavaBonusDivisor = kTuning.water.steam_direct_lava_bonus_divisor;
inline constexpr std::uint32_t kWaterSteamDirectFireBonusDivisor = kTuning.water.steam_direct_fire_bonus_divisor;
inline constexpr std::uint32_t kWaterSteamHotWallOddsDivisor = kTuning.water.steam_hot_wall_odds_divisor;
inline constexpr int kHotWallSteamTempThreshold = kTuning.water.hot_wall_steam_temp_threshold;

inline constexpr int kOilSpread = kTuning.oil.spread;

inline constexpr std::int16_t kSteamTemp = kTuning.smoke.steam_temp;
inline constexpr std::uint32_t kSmokeBubbleSwapOddsDivisor = kTuning.smoke.bubble_swap_odds_divisor;
inline constexpr std::uint32_t kSteamBubbleSwapOddsDivisor = kTuning.smoke.steam_bubble_swap_odds_divisor;
inline constexpr int kSteamSmokeTempThreshold = kTuning.smoke.steam_smoke_temp_threshold;
inline constexpr int kHighSmokePressure = kTuning.smoke.high_pressure;
inline constexpr int kExtremeSmokePressure = kTuning.smoke.extreme_pressure;
inline constexpr int kSmokeCondenseTempThreshold = kTuning.smoke.condense_temp_threshold;
inline constexpr std::uint32_t kSmokeCondenseOddsDivisor = kTuning.smoke.condense_odds_divisor;

inline constexpr int kLiquidPressureActivationThreshold = kTuning.liquid_pressure.activation_threshold;
inline constexpr int kLiquidPressureSpreadBonusStep = kTuning.liquid_pressure.spread_bonus_step;
inline constexpr int kLiquidMaxSpreadBonus = kTuning.liquid_pressure.max_spread_bonus;

}  // namespace simcfg
