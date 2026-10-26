#include "voxel_color.hpp"
#include "voxel_volume.hpp"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <cmath>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[FAIL] Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace kjd;

void runVoxelTests() {
    std::cout << "=============================================" << std::endl;
    std::cout << " Running KJD Voxel RGBA4444 & Volume Tests   " << std::endl;
    std::cout << "=============================================" << std::endl;

    // 1. Specific test vector: 0xF48C decodes to R=15, G=4, B=8, A=12
    {
        VoxelColor packed = 0xF48C;
        Rgba4444 c = unpackRgba4444(packed);
        TEST_ASSERT(c.r == 15);
        TEST_ASSERT(c.g == 4);
        TEST_ASSERT(c.b == 8);
        TEST_ASSERT(c.a == 12);

        // Repack and ensure round trip identity
        VoxelColor repacked = packRgba4444(c.r, c.g, c.b, c.a);
        TEST_ASSERT(repacked == 0xF48C);
        std::cout << "[PASS] 1. Reference test vector 0xF48C decodes to (15, 4, 8, 12) and repacks identically." << std::endl;
    }

    // 2. Constants: 0x0000 is transparent black, 0xFFFF is opaque white
    {
        // 0x0000: fully transparent black (opacity = 0)
        Rgba4444 transBlack = unpackRgba4444(VOXEL_TRANSPARENT_BLACK);
        TEST_ASSERT(transBlack.r == 0);
        TEST_ASSERT(transBlack.g == 0);
        TEST_ASSERT(transBlack.b == 0);
        TEST_ASSERT(transBlack.a == 0); // 0 = fully transparent

        float r = 0, g = 0, b = 0, a = 0;
        unpackRgba4444Normalized(VOXEL_TRANSPARENT_BLACK, r, g, b, a);
        TEST_ASSERT(r == 0.0f && g == 0.0f && b == 0.0f && a == 0.0f);

        // 0xFFFF: fully opaque white (opacity = 15)
        Rgba4444 opaqueWhite = unpackRgba4444(VOXEL_OPAQUE_WHITE);
        TEST_ASSERT(opaqueWhite.r == 15);
        TEST_ASSERT(opaqueWhite.g == 15);
        TEST_ASSERT(opaqueWhite.b == 15);
        TEST_ASSERT(opaqueWhite.a == 15); // 15 = fully opaque

        unpackRgba4444Normalized(VOXEL_OPAQUE_WHITE, r, g, b, a);
        TEST_ASSERT(r == 1.0f && g == 1.0f && b == 1.0f && a == 1.0f);

        std::cout << "[PASS] 2. 0x0000 confirmed transparent black, 0xFFFF confirmed opaque white." << std::endl;
    }

    // 3. Exhaustive 4-bit channel preservation test across all channels
    {
        // Test all possible combinations for each channel independently and corner values
        for (int r = 0; r <= 15; ++r) {
            for (int g = 0; g <= 15; ++g) {
                VoxelColor packed = packRgba4444(static_cast<uint8_t>(r), static_cast<uint8_t>(g), 7, 11);
                Rgba4444 out = unpackRgba4444(packed);
                TEST_ASSERT(out.r == r);
                TEST_ASSERT(out.g == g);
                TEST_ASSERT(out.b == 7);
                TEST_ASSERT(out.a == 11);
            }
        }

        // Test all 65,536 16-bit integer values: unpacking and repacking must be an exact identity bijection
        for (uint32_t val = 0; val <= 0xFFFF; ++val) {
            VoxelColor original = static_cast<VoxelColor>(val);
            Rgba4444 c = unpackRgba4444(original);
            VoxelColor repacked = packRgba4444(c.r, c.g, c.b, c.a);
            TEST_ASSERT(repacked == original);
        }
        std::cout << "[PASS] 3. Exhaustive 16-bit bijection: all 65,536 bit patterns preserved identically." << std::endl;
    }

    // 4. 8-bit channel expansion
    {
        // 0 -> 0x00 (0)
        TEST_ASSERT(expand4(0) == 0);
        // 15 -> 0xFF (255)
        TEST_ASSERT(expand4(15) == 255);
        // 1 -> 0x11 (17)
        TEST_ASSERT(expand4(1) == 17);
        // 8 -> 0x88 (136)
        TEST_ASSERT(expand4(8) == 136);

        // Verify across all 0..15 values
        for (uint8_t c = 0; c <= 15; ++c) {
            uint8_t exp = expand4(c);
            uint8_t expected = static_cast<uint8_t>((c << 4) | c);
            TEST_ASSERT(exp == expected);
        }

        // Unpack to 8-bit channels for 0xF48C
        uint8_t r8, g8, b8, a8;
        unpackRgba4444To8Bit(0xF48C, r8, g8, b8, a8);
        TEST_ASSERT(r8 == 255); // 15 -> 255
        TEST_ASSERT(g8 == 0x44); // 4 -> 68
        TEST_ASSERT(b8 == 0x88); // 8 -> 136
        TEST_ASSERT(a8 == 0xCC); // 12 -> 204
        std::cout << "[PASS] 4. 8-bit channel expansion (c << 4 | c) maps 0->0, 15->255 and all intermediate values." << std::endl;
    }

    // 5. Normalized float conversions and opacity semantics
    {
        // Opacity convention: A / 15.0f
        float r, g, b, a;
        unpackRgba4444Normalized(0xF48C, r, g, b, a);
        TEST_ASSERT(std::abs(r - 1.0f) < 1e-4f);
        TEST_ASSERT(std::abs(g - (4.0f / 15.0f)) < 1e-4f);
        TEST_ASSERT(std::abs(b - (8.0f / 15.0f)) < 1e-4f);
        TEST_ASSERT(std::abs(a - (12.0f / 15.0f)) < 1e-4f);

        // Normalized packing
        VoxelColor repacked = packRgba4444Normalized(1.0f, 4.0f / 15.0f, 8.0f / 15.0f, 12.0f / 15.0f);
        TEST_ASSERT(repacked == 0xF48C);

        // Round-trip edge floats
        TEST_ASSERT(packRgba4444Normalized(0.0f, 0.0f, 0.0f, 0.0f) == VOXEL_TRANSPARENT_BLACK);
        TEST_ASSERT(packRgba4444Normalized(1.0f, 1.0f, 1.0f, 1.0f) == VOXEL_OPAQUE_WHITE);
        std::cout << "[PASS] 5. Normalized float conversion [0.0, 1.0] and opacity semantics verified." << std::endl;
    }

    // 6. Channel input validation and clamping policy
    {
        // Out-of-range positive values must be clamped to 15
        VoxelColor clampedHigh = packRgba4444(99, 20, 255, 16);
        Rgba4444 cHigh = unpackRgba4444(clampedHigh);
        TEST_ASSERT(cHigh.r == 15);
        TEST_ASSERT(cHigh.g == 15);
        TEST_ASSERT(cHigh.b == 15);
        TEST_ASSERT(cHigh.a == 15);

        // Float clamping (< 0.0 clamped to 0.0, > 1.0 clamped to 1.0)
        VoxelColor clampedFloat = packRgba4444Normalized(-5.0f, 10.0f, -0.1f, 1.5f);
        Rgba4444 cFloat = unpackRgba4444(clampedFloat);
        TEST_ASSERT(cFloat.r == 0);
        TEST_ASSERT(cFloat.g == 15);
        TEST_ASSERT(cFloat.b == 0);
        TEST_ASSERT(cFloat.a == 15);

        std::cout << "[PASS] 6. Out-of-bounds inputs clamped strictly to [0, 15] / [0.0, 1.0]." << std::endl;
    }

    // 7. Explicit Big-Endian serialization and deserialization
    {
        uint8_t buffer[2];
        // 0xF48C -> MSB = 0xF4, LSB = 0x8C
        serializeVoxelBe(0xF48C, buffer);
        TEST_ASSERT(buffer[0] == 0xF4);
        TEST_ASSERT(buffer[1] == 0x8C);

        VoxelColor restored = deserializeVoxelBe(buffer);
        TEST_ASSERT(restored == 0xF48C);

        // Test boundary values
        serializeVoxelBe(0x0000, buffer);
        TEST_ASSERT(buffer[0] == 0x00 && buffer[1] == 0x00);
        TEST_ASSERT(deserializeVoxelBe(buffer) == 0x0000);

        serializeVoxelBe(0xFFFF, buffer);
        TEST_ASSERT(buffer[0] == 0xFF && buffer[1] == 0xFF);
        TEST_ASSERT(deserializeVoxelBe(buffer) == 0xFFFF);

        std::cout << "[PASS] 7. Explicit Big-Endian serialization (0xF48C -> [0xF4, 0x8C]) verified." << std::endl;
    }

    // 8. 100x100x100 VoxelVolume RAM allocation and size
    {
        VoxelVolume vol(100, 100, 100);

        TEST_ASSERT(vol.width() == 100);
        TEST_ASSERT(vol.height() == 100);
        TEST_ASSERT(vol.depth() == 100);
        TEST_ASSERT(vol.voxelCount() == 1000000);

        // Exactly 2,000,000 bytes (1,000,000 * 2 bytes)
        TEST_ASSERT(vol.sizeBytes() == 2000000);
        TEST_ASSERT(sizeof(VoxelColor) == 2);

        // Default initialized: all voxels are 0x0000 (invisible / transparent black)
        for (size_t i = 0; i < vol.voxelCount(); ++i) {
            TEST_ASSERT(vol.getVoxelLinear(i) == VOXEL_TRANSPARENT_BLACK);
        }

        std::cout << "[PASS] 8. 100x100x100 VoxelVolume allocates exactly 2,000,000 bytes in RAM, all invisible (0x0000)." << std::endl;
    }

    // 9. Coordinate indexing, setting, and bounds checking
    {
        VoxelVolume vol(100, 100, 100);

        // Set corners
        TEST_ASSERT(vol.setVoxel(0, 0, 0, 0xF00F));       // Opaque red at (0, 0, 0)
        TEST_ASSERT(vol.setVoxel(99, 99, 99, 0x0F0F));   // Opaque green at (99, 99, 99)
        TEST_ASSERT(vol.setVoxel(50, 50, 50, 0xF48C));   // Test color at center

        TEST_ASSERT(vol.getVoxel(0, 0, 0) == 0xF00F);
        TEST_ASSERT(vol.getVoxel(99, 99, 99) == 0x0F0F);
        TEST_ASSERT(vol.getVoxel(50, 50, 50) == 0xF48C);

        // Verify linear index mapping: index = x + y * width + z * (width * height)
        size_t expectedIndex = 50 + (50 * 100) + (50 * 10000);
        TEST_ASSERT(vol.getVoxelLinear(expectedIndex) == 0xF48C);

        // Out-of-bounds coordinate rejection
        TEST_ASSERT(!vol.setVoxel(100, 0, 0, 0xFFFF));
        TEST_ASSERT(!vol.setVoxel(0, 100, 0, 0xFFFF));
        TEST_ASSERT(!vol.setVoxel(0, 0, 100, 0xFFFF));
        TEST_ASSERT(vol.getVoxel(100, 0, 0) == VOXEL_TRANSPARENT_BLACK);

        // Clear operation
        vol.clear(VOXEL_TRANSPARENT_BLACK);
        TEST_ASSERT(vol.getVoxel(0, 0, 0) == VOXEL_TRANSPARENT_BLACK);
        TEST_ASSERT(vol.getVoxel(50, 50, 50) == VOXEL_TRANSPARENT_BLACK);
        TEST_ASSERT(vol.getVoxel(99, 99, 99) == VOXEL_TRANSPARENT_BLACK);

        std::cout << "[PASS] 9. Coordinate 3D <-> 1D mapping and bounds safety verified." << std::endl;
    }

    std::cout << "=============================================" << std::endl;
    std::cout << " ALL VOXEL FORMAT & VOLUME TESTS PASSED!     " << std::endl;
    std::cout << "=============================================" << std::endl;
}

int main() {
    try {
        runVoxelTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Voxel test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
