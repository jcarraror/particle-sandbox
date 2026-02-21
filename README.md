/**
 * @mainpage Particle Sandbox
 *
 * @section overview Overview
 * A compact falling-sand style sandbox built with C++23 and SDL2.
 *
 * @section features Features
 * - Cellular world simulation with multiple materials.
 * - Material toolbar with mouse selection.
 * - Brush painting and erase tools.
 * - Basic thermal behavior for fire and lava.
 *
 * @section build Build
 * @code{.sh}
 * cmake -S . -B build
 * cmake --build build
 * @endcode
 *
 * @section run Run
 * @code{.sh}
 * ./build/particle_sandbox
 * @endcode
 *
 * @section controls Controls
 * - `1` Sand
 * - `2` Water
 * - `3` Oil
 * - `4` Fire
 * - `5` Smoke
 * - `6` Lava
 * - `7` Wall
 * - `0` Erase
 * - Mouse wheel / `-` / `=`: brush radius
 * - Left mouse: paint
 * - Right mouse: erase
 * - `Space`: pause
 * - `C`: clear
 * - `Esc`: quit
 */
