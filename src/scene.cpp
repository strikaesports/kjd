#include "scene.hpp"
#include "logger.hpp"
#include <algorithm>

namespace kjd {

Scene::Scene(const AppConfig& config)
    : m_rotateCube(false), // Auto-spin stopped altogether
      m_rotationSpeed(0.0f),
      m_yawAngle(35.0f),
      m_pitchAngle(22.0f),
      m_fov(config.fov),
      m_cameraDistance(2.4f) { // Zoomed in closer for prominent voxel visibility
    m_cameraPos = Vec3(0.0f, 0.8f, m_cameraDistance);
    m_target = Vec3(0.0f, 0.0f, 0.0f);
    m_up = Vec3(0.0f, 1.0f, 0.0f);

    m_voxelVolume = std::make_unique<VoxelVolume>(100, 100, 100);
    KJD_LOG_INFO("Allocated 100x100x100 voxel grid in RAM (",
                 m_voxelVolume->sizeBytes(), " bytes / ~1.91 MiB, format: RGBA4444)");
}

void Scene::update(float deltaTime) {
    // Auto-spin permanently disabled. Cube only rotates via AirMouse / user commands.
    if (m_rotateCube && m_rotationSpeed > 0.0f) {
        m_yawAngle += m_rotationSpeed * deltaTime;
        if (m_yawAngle >= 360.0f) m_yawAngle -= 360.0f;
    }

    // Advance idle voxel diagonal wave oscillation
    m_idleTimer += deltaTime;
    if (m_idleWaveActive && m_voxelVolume) {
        m_voxelVolume->updateDiagonalWave(m_idleTimer);
    }
}

void Scene::addOrbit(float deltaYaw, float deltaPitch) {
    m_yawAngle += deltaYaw;
    while (m_yawAngle >= 360.0f) m_yawAngle -= 360.0f;
    while (m_yawAngle < 0.0f) m_yawAngle += 360.0f;

    m_pitchAngle += deltaPitch;
    m_pitchAngle = std::clamp(m_pitchAngle, -89.0f, 89.0f);
}

void Scene::addZoom(float deltaZoom) {
    // Positive deltaZ (forward) zooms closer to the cube
    m_cameraDistance -= deltaZoom;
    m_cameraDistance = std::clamp(m_cameraDistance, 1.0f, 15.0f);
    m_cameraPos = Vec3(0.0f, 0.8f, m_cameraDistance);
}

void Scene::resetOrientation() {
    m_yawAngle = 35.0f;
    m_pitchAngle = 22.0f;
    m_cameraDistance = 2.4f;
    m_cameraPos = Vec3(0.0f, 0.8f, m_cameraDistance);
}

Mat4 Scene::getModelMatrix() const {
    Mat4 rotY = Mat4::rotation(radians(m_yawAngle), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 rotX = Mat4::rotation(radians(m_pitchAngle), Vec3(1.0f, 0.0f, 0.0f));
    return rotX * rotY;
}

Mat4 Scene::getViewMatrix() const {
    return Mat4::lookAt(m_cameraPos, m_target, m_up);
}

Mat4 Scene::getProjectionMatrix(float aspect) const {
    return Mat4::perspective(radians(m_fov), aspect, 0.1f, 100.0f);
}

} // namespace kjd
