#include "voxel_volume.hpp"
#include <algorithm>

namespace kjd {

VoxelVolume::VoxelVolume(size_t width, size_t height, size_t depth)
    : m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_sliceSize(width * height)
    , m_voxelCount(width * height * depth)
    , m_data(m_voxelCount, VOXEL_TRANSPARENT_BLACK) // Zero-initialized (invisible voxels)
{
}

bool VoxelVolume::setVoxel(size_t x, size_t y, size_t z, VoxelColor color) {
    if (!inBounds(x, y, z)) {
        return false;
    }
    m_data[computeIndex(x, y, z)] = color;
    return true;
}

VoxelColor VoxelVolume::getVoxel(size_t x, size_t y, size_t z) const {
    if (!inBounds(x, y, z)) {
        return VOXEL_TRANSPARENT_BLACK;
    }
    return m_data[computeIndex(x, y, z)];
}

VoxelColor VoxelVolume::getVoxelLinear(size_t index) const {
    if (index >= m_voxelCount) {
        return VOXEL_TRANSPARENT_BLACK;
    }
    return m_data[index];
}

void VoxelVolume::setVoxelLinear(size_t index, VoxelColor color) {
    if (index < m_voxelCount) {
        m_data[index] = color;
    }
}

void VoxelVolume::clear(VoxelColor clearColor) {
    std::fill(m_data.begin(), m_data.end(), clearColor);
}

} // namespace kjd
