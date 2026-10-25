#pragma once

#include "scene.hpp"
#include "config.hpp"
#include <GL/glew.h>
#include <string>

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
    void render(const Scene& scene, int drawableWidth, int drawableHeight);
    void cleanup();

    const GraphicsInfo& getGraphicsInfo() const { return m_info; }

private:
    bool initShaders();
    bool initGeometry();
    void probeGraphicsContext();
    std::string loadShaderSource(const std::string& path);

    GLuint m_shaderProgram{0};
    GLuint m_vao{0};
    GLuint m_vbo{0};

    // Uniform locations
    GLint m_locMVP{-1};
    GLint m_locModel{-1};
    GLint m_locLightDir{-1};
    GLint m_locViewPos{-1};

    GraphicsInfo m_info;
    float m_renderScale{1.0f};
    bool m_initialized{false};
};

} // namespace kjd
