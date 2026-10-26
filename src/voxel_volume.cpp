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

void VoxelVolume::appendVoxelCubeVertices(float wx, float wy, float wz,
                                         float dx, float dy, float dz,
                                         float r, float g, float b, float a) {
    float x0 = wx - dx * 0.5f;
    float x1 = wx + dx * 0.5f;
    float y0 = wy - dy * 0.5f;
    float y1 = wy + dy * 0.5f;
    float z0 = wz - dz * 0.5f;
    float z1 = wz + dz * 0.5f;

    // 6 faces * 2 triangles * 3 vertices = 36 vertices (standard CCW winding with surface normals)
    // Front face (+Z, normal: 0, 0, 1)
    m_activeVertices.push_back({x0, y0, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z1, 0.0f, 0.0f, 1.0f, r, g, b, a});

    // Back face (-Z, normal: 0, 0, -1)
    m_activeVertices.push_back({x1, y0, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z0, 0.0f, 0.0f, -1.0f, r, g, b, a});

    // Top face (+Y, normal: 0, 1, 0)
    m_activeVertices.push_back({x0, y1, z0, 0.0f, 1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z1, 0.0f, 1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 0.0f, 1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z0, 0.0f, 1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 0.0f, 1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z0, 0.0f, 1.0f, 0.0f, r, g, b, a});

    // Bottom face (-Y, normal: 0, -1, 0)
    m_activeVertices.push_back({x0, y0, z0, 0.0f, -1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z0, 0.0f, -1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z1, 0.0f, -1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z0, 0.0f, -1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z1, 0.0f, -1.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z1, 0.0f, -1.0f, 0.0f, r, g, b, a});

    // Right face (+X, normal: 1, 0, 0)
    m_activeVertices.push_back({x1, y0, z0, 1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z0, 1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z0, 1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y1, z1, 1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x1, y0, z1, 1.0f, 0.0f, 0.0f, r, g, b, a});

    // Left face (-X, normal: -1, 0, 0)
    m_activeVertices.push_back({x0, y0, z0, -1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z1, -1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z1, -1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y0, z0, -1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z1, -1.0f, 0.0f, 0.0f, r, g, b, a});
    m_activeVertices.push_back({x0, y1, z0, -1.0f, 0.0f, 0.0f, r, g, b, a});
}

void VoxelVolume::updateCursor(int cx, int cy, int cz, int size, VoxelColor color) {
    // Clear previously active voxels back to transparent black
    for (size_t idx : m_previousActiveIndices) {
        if (idx < m_voxelCount) {
            m_data[idx] = VOXEL_TRANSPARENT_BLACK;
        }
    }
    m_previousActiveIndices.clear();
    m_activeVertices.clear();

    m_cursorX = cx;
    m_cursorY = cy;
    m_cursorZ = cz;
    m_cursorSize = size;

    int half = size / 2;
    int minX = std::max(0, cx - half);
    int maxX = std::min(static_cast<int>(m_width - 1), cx + half);
    int minY = std::max(0, cy - half);
    int maxY = std::min(static_cast<int>(m_height - 1), cy + half);
    int minZ = std::max(0, cz - half);
    int maxZ = std::min(static_cast<int>(m_depth - 1), cz + half);

    float baseR, baseG, baseB, baseA;
    unpackRgba4444Normalized(color, baseR, baseG, baseB, baseA);

    const float invW = 1.0f / static_cast<float>(m_width);
    const float invH = 1.0f / static_cast<float>(m_height);
    const float invD = 1.0f / static_cast<float>(m_depth);

    size_t count = static_cast<size_t>((maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1));
    m_activeVertices.reserve(count * 36);
    m_previousActiveIndices.reserve(count);

    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                size_t idx = computeIndex(static_cast<size_t>(x), static_cast<size_t>(y), static_cast<size_t>(z));
                m_data[idx] = color;
                m_previousActiveIndices.push_back(idx);

                // World position mapped to unit cube range [-0.5, 0.5]
                float wx = -0.5f + (static_cast<float>(x) + 0.5f) * invW;
                float wy = -0.5f + (static_cast<float>(y) + 0.5f) * invH;
                float wz = -0.5f + (static_cast<float>(z) + 0.5f) * invD;

                // Subtle checker shading so individual voxels are distinct without gaps
                bool alt = ((x + y + z) & 1) != 0;
                float shade = alt ? 0.90f : 1.0f;

                appendVoxelCubeVertices(wx, wy, wz, invW, invH, invD,
                                        baseR * shade, baseG * shade, baseB * shade, baseA);
            }
        }
    }
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

                appendVoxelCubeVertices(wx, wy, wz, invW, invH, invD,
                                        r / 15.0f, g / 15.0f, b / 15.0f, a / 15.0f);
            }
        }
    }
}

} // namespace kjd
