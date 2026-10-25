#include "scene.hpp"

namespace kjd {

Scene::Scene(const AppConfig& config)
    : m_rotateCube(config.rotate_cube),
      m_rotationSpeed(config.rotation_speed),
      m_yawAngle(35.0f),
      m_pitchAngle(25.0f),
      m_fov(config.fov) {
    m_cameraPos = Vec3(0.0f, 1.2f, config.camera_distance);
    m_target = Vec3(0.0f, 0.0f, 0.0f);
    m_up = Vec3(0.0f, 1.0f, 0.0f);
}

void Scene::update(float deltaTime) {
    if (m_rotateCube) {
        m_yawAngle += m_rotationSpeed * deltaTime;
        if (m_yawAngle >= 360.0f) m_yawAngle -= 360.0f;

        // Subtle secondary oscillation on pitch for dynamic visual confirmation
        m_pitchAngle = 20.0f + 10.0f * std::sin(radians(m_yawAngle * 0.7f));
    }
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
