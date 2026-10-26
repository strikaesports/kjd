#pragma once

#include "math3d.hpp"
#include "config.hpp"
#include "voxel_volume.hpp"
#include <memory>

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

    VoxelVolume* getVoxelVolume() { return m_voxelVolume.get(); }
    const VoxelVolume* getVoxelVolume() const { return m_voxelVolume.get(); }

    void setIdleWave(bool active) { m_idleWaveActive = active; }
    bool isIdleWaveActive() const { return m_idleWaveActive; }

private:
    std::unique_ptr<VoxelVolume> m_voxelVolume;

    bool m_rotateCube{false};
    float m_rotationSpeed{0.0f};  // Auto-spin stopped altogether
    float m_yawAngle{35.0f};      // deg
    float m_pitchAngle{22.0f};    // deg

    float m_fov{60.0f};
    float m_cameraDistance{2.4f}; // Zoomed in closer for detailed voxel viewing
    Vec3 m_cameraPos{0.0f, 0.8f, 2.4f};
    Vec3 m_target{0.0f, 0.0f, 0.0f};
    Vec3 m_up{0.0f, 1.0f, 0.0f};

    float m_idleTimer{0.0f};
    bool m_idleWaveActive{true};
};

} // namespace kjd
