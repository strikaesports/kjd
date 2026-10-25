#pragma once

#include "math3d.hpp"
#include "config.hpp"

namespace kjd {

class Scene {
public:
    explicit Scene(const AppConfig& config);

    void update(float deltaTime);

    // Orbit control: deltaX modifies yaw, deltaY modifies pitch
    void addOrbit(float deltaYaw, float deltaPitch);

    // Zoom control: deltaZ modifies camera distance
    void addZoom(float deltaZoom);

    // Reset cube orientation and zoom
    void resetOrientation();

    Mat4 getModelMatrix() const;
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float aspect) const;
    Vec3 getCameraPosition() const { return m_cameraPos; }

    void toggleRotation() { m_rotateCube = !m_rotateCube; }
    bool isRotating() const { return m_rotateCube; }

    float getYaw() const { return m_yawAngle; }
    float getPitch() const { return m_pitchAngle; }
    float getCameraDistance() const { return m_cameraDistance; }

private:
    bool m_rotateCube{false};
    float m_rotationSpeed{35.0f}; // deg/sec
    float m_yawAngle{35.0f};      // deg
    float m_pitchAngle{25.0f};    // deg

    float m_fov{60.0f};
    float m_cameraDistance{3.5f};
    Vec3 m_cameraPos{0.0f, 1.2f, 3.5f};
    Vec3 m_target{0.0f, 0.0f, 0.0f};
    Vec3 m_up{0.0f, 1.0f, 0.0f};
};

} // namespace kjd
