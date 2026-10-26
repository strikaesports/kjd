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
    m_previousActiveIndices.clear();
    m_activeVertices.clear();
}

void VoxelVolume::updateDiagonalWave(float timeSec, float speed, float pulseWidth) {
    // Clear previously active voxels back to transparent black
    for (size_t idx : m_previousActiveIndices) {
        if (idx < m_voxelCount) {
            m_data[idx] = VOXEL_TRANSPARENT_BLACK;
        }
    }
    m_previousActiveIndices.clear();
    m_activeVertices.clear();

    // Wave center oscillates between 0.0 (corner 0,0,0) and 1.0 (corner 99,99,99)
    float center = 0.5f + 0.5f * std::sin(timeSec * speed);
    const float maxDiag = static_cast<float>((m_width - 1) + (m_height - 1) + (m_depth - 1)); // 297.0
    float targetD = center * maxDiag;
    float halfWidth = pulseWidth * maxDiag;

    int sMin = std::max(0, static_cast<int>(std::floor(targetD - halfWidth)));
    int sMax = std::min(static_cast<int>(maxDiag), static_cast<int>(std::ceil(targetD + halfWidth)));

    const float invW = 1.0f / static_cast<float>(m_width);
    const float invH = 1.0f / static_cast<float>(m_height);
    const float invD = 1.0f / static_cast<float>(m_depth);

    // Color gradient based on wave position
    float rFactor = 0.2f + 0.8f * center;
    float gFactor = 0.3f + 0.7f * std::sin(center * 3.14159f);
    float bFactor = 1.0f - 0.7f * center;

    // Traverse only the diagonal slice currently within the wave envelope
    for (size_t z = 0; z < m_depth; ++z) {
        for (size_t y = 0; y < m_height; ++y) {
            int yz = static_cast<int>(y + z);
            int xMin = std::max(0, sMin - yz);
            int xMax = std::min(static_cast<int>(m_width - 1), sMax - yz);
            if (xMin > xMax) continue;

            for (int x = xMin; x <= xMax; ++x) {
                float d = static_cast<float>(x + yz) / maxDiag;
                float diff = std::abs(d - center);
                if (diff >= pulseWidth) continue;

                float intensity = 1.0f - (diff / pulseWidth);

                uint8_t r = clamp4(static_cast<int>(std::round(15.0f * rFactor * intensity)));
                uint8_t g = clamp4(static_cast<int>(std::round(15.0f * gFactor * intensity)));
                uint8_t b = clamp4(static_cast<int>(std::round(15.0f * bFactor * intensity)));
                uint8_t a = clamp4(static_cast<int>(std::round(15.0f * intensity)));

                if (a == 0) continue;

                VoxelColor packed = packRgba4444(r, g, b, a);
                size_t idx = computeIndex(static_cast<size_t>(x), y, z);
                m_data[idx] = packed;
                m_previousActiveIndices.push_back(idx);

                // World position mapped to unit cube range [-0.5, 0.5]
                float wx = -0.5f + (static_cast<float>(x) + 0.5f) * invW;
                float wy = -0.5f + (static_cast<float>(y) + 0.5f) * invH;
                float wz = -0.5f + (static_cast<float>(z) + 0.5f) * invD;

                m_activeVertices.push_back({
                    wx, wy, wz,
                    r / 15.0f, g / 15.0f, b / 15.0f, a / 15.0f
                });
            }
        }
    }
}

} // namespace kjd
