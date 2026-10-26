#pragma once

#include "voxel_color.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace kjd {

/**
 * VoxelVolume
 * 
 * Represents a dense 3D grid of RGBA4444 voxels held in RAM.
 * Default resolution: 100 x 100 x 100 (1,000,000 voxels).
 * Memory consumption: 1,000,000 * 2 bytes = exactly 2,000,000 bytes (~1.91 MiB).
 * 
 * Stored as a contiguous flat array of uint16_t elements.
 * Initialized by default to 0x0000 (fully transparent black / invisible voxels).
 */
class VoxelVolume {
public:
    static constexpr size_t DEFAULT_WIDTH  = 100;
    static constexpr size_t DEFAULT_HEIGHT = 100;
    static constexpr size_t DEFAULT_DEPTH  = 100;

    explicit VoxelVolume(size_t width = DEFAULT_WIDTH,
                         size_t height = DEFAULT_HEIGHT,
                         size_t depth = DEFAULT_DEPTH);

    ~VoxelVolume() = default;

    // Dimensions
    size_t width() const { return m_width; }
    size_t height() const { return m_height; }
    size_t depth() const { return m_depth; }

    // Total counts and memory
    size_t voxelCount() const { return m_voxelCount; }
    size_t sizeBytes() const { return m_data.size() * sizeof(VoxelColor); }

    // Direct data access
    const VoxelColor* data() const { return m_data.data(); }
    VoxelColor* data() { return m_data.data(); }

    // Coordinate access with bounds checking
    bool setVoxel(size_t x, size_t y, size_t z, VoxelColor color);
    VoxelColor getVoxel(size_t x, size_t y, size_t z) const;

    // Linear index access
    VoxelColor getVoxelLinear(size_t index) const;
    void setVoxelLinear(size_t index, VoxelColor color);

    // Coordinate validation
    bool inBounds(size_t x, size_t y, size_t z) const {
        return (x < m_width && y < m_height && z < m_depth);
    }

    // Reset all voxels (default: 0x0000 invisible)
    void clear(VoxelColor clearColor = VOXEL_TRANSPARENT_BLACK);

    // Compute linear index from (x, y, z)
    inline size_t computeIndex(size_t x, size_t y, size_t z) const {
        return x + (y * m_width) + (z * m_sliceSize);
    }

private:
    size_t m_width{DEFAULT_WIDTH};
    size_t m_height{DEFAULT_HEIGHT};
    size_t m_depth{DEFAULT_DEPTH};
    size_t m_sliceSize{DEFAULT_WIDTH * DEFAULT_HEIGHT};
    size_t m_voxelCount{DEFAULT_WIDTH * DEFAULT_HEIGHT * DEFAULT_DEPTH};

    // Flat contiguous array of 16-bit RGBA4444 voxels in RAM
    std::vector<VoxelColor> m_data;
};

} // namespace kjd
