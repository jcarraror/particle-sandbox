#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "world.hpp"

/**
 * @file render_sdl.hpp
 * @brief SDL2 renderer wrapper for displaying the simulation and toolbar UI.
 */

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

/**
 * @brief Toolbar descriptor for one selectable material button.
 */
struct MaterialButton {
  CellType type{};      /**< Material type selected by this button. */
  const char* name{};   /**< Label shown in the toolbar. */
  const char* hotkey{}; /**< Keyboard shortcut hint shown in the toolbar. */
};

/**
 * @brief SDL renderer state and draw helpers.
 */
struct RendererSDL {
  SDL_Window* window = nullptr;      /**< SDL window handle. */
  SDL_Renderer* renderer = nullptr;  /**< SDL hardware/software renderer. */
  SDL_Texture* texture = nullptr;    /**< Streaming texture for world pixels. */
  bool owns_sdl_video = false;       /**< Whether this instance must call `SDL_Quit()`. */

  int scale = 4;      /**< Pixel scale factor from grid cell to screen. */
  int grid_w = 0;     /**< World width in cells. */
  int grid_h = 0;     /**< World height in cells. */
  int toolbar_h = 64; /**< Toolbar height in pixels. */

  int win_w = 0; /**< Window width in pixels. */
  int win_h = 0; /**< Window height in pixels. */

  std::vector<std::uint32_t> pixels; /**< CPU-side ARGB8888 buffer (`grid_w * grid_h`). */

  RendererSDL() = default;
  ~RendererSDL();

  RendererSDL(const RendererSDL&) = delete;
  RendererSDL& operator=(const RendererSDL&) = delete;

  RendererSDL(RendererSDL&& other) noexcept;
  RendererSDL& operator=(RendererSDL&& other) noexcept;

  /**
   * @brief Creates SDL resources for rendering.
   * @param grid_w World width in cells.
   * @param grid_h World height in cells.
   * @param scale Cell-to-pixel scale factor.
   * @param toolbar_h_px Toolbar height in pixels.
   * @return Initialized renderer object or an error string.
   */
  [[nodiscard]] static std::expected<RendererSDL, std::string> create(int grid_w, int grid_h, int scale, int toolbar_h_px = 64);

  /**
   * @brief Updates the OS window title.
   * @param title New title text.
   */
  void set_title(const std::string& title);

  /**
   * @brief Renders the world plus toolbar and brush preview.
   * @param world Simulation data to draw.
   * @param selected Currently selected brush material.
   * @param brush_radius Brush radius in cells.
   * @param mouse_x Mouse X in window pixels.
   * @param mouse_y Mouse Y in window pixels.
   * @param paused Whether simulation stepping is paused.
   * @param show_pressure_debug Whether to render pressure field visualization.
   */
  void draw(const World& world,
            CellType selected,
            int brush_radius,
            int mouse_x,
            int mouse_y,
            bool paused,
            bool show_pressure_debug);

  /**
   * @brief Converts window mouse coordinates to grid coordinates.
   * @param mx Mouse X in window pixels.
   * @param my Mouse Y in window pixels.
   * @param gx Output grid X.
   * @param gy Output grid Y.
   * @return `true` if the mouse is inside the drawable world grid.
   */
  bool mouse_to_grid(int mx, int my, int& gx, int& gy) const;

  /**
   * @brief Returns the toolbar material under a mouse position.
   * @param mx Mouse X in window pixels.
   * @param my Mouse Y in window pixels.
   * @return Selected material type if over a toolbar button.
   */
  std::optional<CellType> hit_test_toolbar(int mx, int my) const;

  /**
   * @brief Checks if the mouse is over the toolbar clear/reset button.
   * @param mx Mouse X in window pixels.
   * @param my Mouse Y in window pixels.
   * @return `true` if over the clear button.
   */
  bool hit_test_clear_button(int mx, int my) const;

  /**
   * @brief Checks if the mouse is over the toolbar randomize button.
   * @param mx Mouse X in window pixels.
   * @param my Mouse Y in window pixels.
   * @return `true` if over the randomize button.
   */
  bool hit_test_random_button(int mx, int my) const;

  /**
   * @brief Checks whether mouse Y is inside the toolbar area.
   * @param my Mouse Y in window pixels.
   * @return `true` if inside toolbar vertical bounds.
   */
  bool mouse_in_toolbar(int my) const { return my >= 0 && my < toolbar_h; }

  /**
   * @brief Gets the set of toolbar materials and labels.
   * @return Ordered list of material buttons.
   */
  static std::span<const MaterialButton> materials();

private:
  /**
   * @brief Releases all SDL resources owned by this renderer.
   */
  void destroy();
};
