#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace kjd {

/**
 * KJD Voxel Color Format: RGBA4444
 * 
 * Compact 16-bit color representation for KJD voxels.
 * Each voxel occupies exactly 2 bytes (16 bits).
 *
 * Bit layout (MSB to LSB):
 *   Bits 15–12: Red   (R), 0–15
 *   Bits 11–8:  Green (G), 0–15
 *   Bits 7–4:   Blue  (B), 0–15
 *   Bits 3–0:   Alpha (A), 0–15 (Opacity: 0 = transparent, 15 = fully opaque)
 *
 * Binary layout: RRRR GGGG BBBB AAAA
 * Example: 0xF48C represents R=15, G=4, B=8, A=12
 */

using VoxelColor = uint16_t;

// Standard constant colors
constexpr VoxelColor VOXEL_TRANSPARENT_BLACK = 0x0000;
constexpr VoxelColor VOXEL_OPAQUE_WHITE      = 0xFFFF;
constexpr VoxelColor VOXEL_OPAQUE_BLACK      = 0x000F;
constexpr VoxelColor VOXEL_OPAQUE_RED        = 0xF00F;
constexpr VoxelColor VOXEL_OPAQUE_GREEN      = 0x0F0F;
constexpr VoxelColor VOXEL_OPAQUE_BLUE       = 0x00FF;

/**
 * Unpacked representation of 4-bit RGBA channels.
 */
struct Rgba4444 {
    uint8_t r{0}; // 0-15
    uint8_t g{0}; // 0-15
    uint8_t b{0}; // 0-15
    uint8_t a{0}; // 0-15 (opacity: 0 = transparent, 15 = opaque)
};

/**
 * Clamps an integer channel value to the valid 4-bit range [0, 15].
 */
constexpr inline uint8_t clamp4(int val) {
    if (val < 0) return 0;
    if (val > 15) return 15;
    return static_cast<uint8_t>(val);
}

/**
 * Packs four 4-bit channels into a 16-bit RGBA4444 integer.
 * Input values are automatically clamped to [0, 15].
 */
constexpr inline VoxelColor packRgba4444(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint16_t>(clamp4(r)) << 12) |
           (static_cast<uint16_t>(clamp4(g)) << 8)  |
           (static_cast<uint16_t>(clamp4(b)) << 4)  |
           (static_cast<uint16_t>(clamp4(a)));
}

/**
 * Packs an Rgba4444 struct into a 16-bit RGBA4444 integer.
 */
constexpr inline VoxelColor packRgba4444(const Rgba4444& c) {
    return packRgba4444(c.r, c.g, c.b, c.a);
}

/**
 * Unpacks a 16-bit RGBA4444 integer into individual 4-bit channels.
 */
constexpr inline Rgba4444 unpackRgba4444(VoxelColor packed) {
    return Rgba4444{
        static_cast<uint8_t>((packed >> 12) & 0x0F),
        static_cast<uint8_t>((packed >> 8)  & 0x0F),
        static_cast<uint8_t>((packed >> 4)  & 0x0F),
        static_cast<uint8_t>(packed         & 0x0F)
    };
}

/**
 * Expands a 4-bit channel [0, 15] to an 8-bit channel [0, 255].
 * Formula: (c << 4) | c
 * Example: 0 -> 0x00 (0), 15 -> 0xFF (255), 8 -> 0x88 (136)
 */
constexpr inline uint8_t expand4(uint8_t c) {
    c &= 0x0F;
    return static_cast<uint8_t>((c << 4) | c);
}

/**
 * Converts a 16-bit RGBA4444 integer into 8-bit channels [0, 255].
 */
inline void unpackRgba4444To8Bit(VoxelColor packed, uint8_t& r8, uint8_t& g8, uint8_t& b8, uint8_t& a8) {
    Rgba4444 c = unpackRgba4444(packed);
    r8 = expand4(c.r);
    g8 = expand4(c.g);
    b8 = expand4(c.b);
    a8 = expand4(c.a);
}

/**
 * Converts a 16-bit RGBA4444 integer to normalized floating-point channels [0.0, 1.0].
 * Straight alpha convention: RGB is independent of alpha.
 * Opacity = A / 15.0f.
 */
inline void unpackRgba4444Normalized(VoxelColor packed, float& r, float& g, float& b, float& a) {
    Rgba4444 c = unpackRgba4444(packed);
    r = c.r / 15.0f;
    g = c.g / 15.0f;
    b = c.b / 15.0f;
    a = c.a / 15.0f;
}

/**
 * Packs normalized floating-point channels [0.0, 1.0] into a 16-bit RGBA4444 integer.
 */
inline VoxelColor packRgba4444Normalized(float r, float g, float b, float a) {
    auto to4 = [](float f) -> uint8_t {
        int v = static_cast<int>(std::round(std::clamp(f, 0.0f, 1.0f) * 15.0f));
        return clamp4(v);
    };
    return packRgba4444(to4(r), to4(g), to4(b), to4(a));
}

/**
 * Serializes a 16-bit RGBA4444 integer to 2 raw bytes in Big-Endian (canonical) format.
 * Independent of host CPU endianness.
 */
inline void serializeVoxelBe(VoxelColor color, uint8_t out[2]) {
    out[0] = static_cast<uint8_t>((color >> 8) & 0xFF);
    out[1] = static_cast<uint8_t>(color & 0xFF);
}

/**
 * Deserializes 2 raw bytes in Big-Endian format into a 16-bit RGBA4444 integer.
 * Independent of host CPU endianness.
 */
inline VoxelColor deserializeVoxelBe(const uint8_t in[2]) {
    return (static_cast<uint16_t>(in[0]) << 8) | static_cast<uint16_t>(in[1]);
}

} // namespace kjd
