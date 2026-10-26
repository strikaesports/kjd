#pragma once

#include "scene.hpp"
#include "config.hpp"
#include "input_receiver.hpp"
#include <GL/glew.h>
#include <string>
#include <vector>

namespace kjd {

struct GraphicsInfo {
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string glslVersion;
    bool isSoftware{false};
    std::string accelerationReason;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const AppConfig& config);
    void render(const Scene& scene, int drawableWidth, int drawableHeight,
                const InputState& inputState = {}, const ReceiverStats& inputStats = {});
    void cleanup();

    const GraphicsInfo& getGraphicsInfo() const { return m_info; }

private:
    bool initShaders();
    bool initGeometry();
    bool initHud();
    void renderHud(int width, int height, const InputState& inputState, const ReceiverStats& inputStats,
                   int cursorX = 50, int cursorY = 50, int cursorZ = 50);
    void probeGraphicsContext();
    std::string loadShaderSource(const std::string& path);

    // 3D Scene program & geometry
    GLuint m_shaderProgram{0};
    GLuint m_vao{0};
    GLuint m_vbo{0};

    // Dynamic point VBO for visible voxels
    GLuint m_voxelVao{0};
    GLuint m_voxelVbo{0};

    // 2D HUD program, font texture, and geometry
    GLuint m_hudProgram{0};
    GLuint m_hudVao{0};
    GLuint m_hudVbo{0};
    GLuint m_fontTexture{0};

    // Uniform locations for 3D
    GLint m_locMVP{-1};
    GLint m_locModel{-1};
    GLint m_locLightDir{-1};
    GLint m_locViewPos{-1};

    // Uniform locations for HUD
    GLint m_locHudScreenSize{-1};
    GLint m_locHudUseTex{-1};

    GraphicsInfo m_info;
    float m_renderScale{1.0f};
    bool m_initialized{false};
};

} // namespace kjd
