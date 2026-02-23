#pragma once

#include <array>
#include <cstdint>

namespace render_font {

struct Glyph5x7 {
  std::array<std::uint8_t, 7> rows{};
};

[[nodiscard]] const Glyph5x7* glyph_for(char c) noexcept;

}  // namespace render_font

