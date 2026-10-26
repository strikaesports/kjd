#pragma once

#include <cstdint>

namespace kjd {

// Compact 8x8 bitmap font for printable ASCII characters 32 (' ') through 126 ('~').
// Each character is defined by 8 bytes (1 byte per row, MSB is leftmost pixel).
// Public Domain / CC0 font table.
extern const uint8_t FONT8X8_BASIC[95][8];

} // namespace kjd
