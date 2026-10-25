#pragma once

#include "math3d.hpp"
#include "config.hpp"

namespace kjd {

class Scene {
public:
    explicit Scene(const AppConfig& config);

    void update(float deltaTime);

    Mat4 getModelMatrix() const;
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float aspect) const;
    Vec3 getCameraPosition() const { return m_cameraPos; }

    void toggleRotation() { m_rotateCube = !m_rotateCube; }
    bool isRotating() const { return m_rotateCube; }

private:
    bool m_rotateCube{true};
    float m_rotationSpeed{35.0f}; // deg/sec
    float m_yawAngle{0.0f};       // deg
    float m_pitchAngle{0.0f};     // deg

    float m_fov{60.0f};
    Vec3 m_cameraPos{0.0f, 1.0f, 3.5f};
    Vec3 m_target{0.0f, 0.0f, 0.0f};
    Vec3 m_up{0.0f, 1.0f, 0.0f};
};

} // namespace kjd
