#include "renderer.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

namespace kjd {

// Fallback embedded vertex shader in case external file cannot be read
static const char* EMBEDDED_VERT_SHADER = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

// Fallback embedded fragment shader
static const char* EMBEDDED_FRAG_SHADER = R"(#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;

void main() {
    // For wireframe edges (normal length near zero), output crisp unlit white color
    if (length(vNormal) < 0.1) {
        FragColor = vec4(vColor, 1.0);
        return;
    }

    float ambientStrength = 0.80;
    vec3 ambient = ambientStrength * vColor;

    vec3 norm = normalize(vNormal);
    vec3 light1 = normalize(vec3(0.577, 0.577, 0.577));
    vec3 light2 = normalize(vec3(-0.577, -0.577, -0.577));

    float diff1 = max(dot(norm, light1), 0.0) * 0.12;
    float diff2 = max(dot(norm, light2), 0.0) * 0.08;
    vec3 diffuse = (diff1 + diff2) * vColor;

    vec3 result = clamp(ambient + diffuse, 0.0, 1.0);
    FragColor = vec4(result, 1.0);
}
)";

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

Renderer::Renderer() = default;

Renderer::~Renderer() {
    cleanup();
}

void Renderer::probeGraphicsContext() {
    const GLubyte* vendorStr = glGetString(GL_VENDOR);
    const GLubyte* rendererStr = glGetString(GL_RENDERER);
    const GLubyte* versionStr = glGetString(GL_VERSION);
    const GLubyte* glslStr = glGetString(GL_SHADING_LANGUAGE_VERSION);

    m_info.vendor = vendorStr ? reinterpret_cast<const char*>(vendorStr) : "Unknown Vendor";
    m_info.renderer = rendererStr ? reinterpret_cast<const char*>(rendererStr) : "Unknown Renderer";
    m_info.version = versionStr ? reinterpret_cast<const char*>(versionStr) : "Unknown Version";
    m_info.glslVersion = glslStr ? reinterpret_cast<const char*>(glslStr) : "Unknown GLSL";

    std::string lowerRenderer = m_info.renderer;
    std::transform(lowerRenderer.begin(), lowerRenderer.end(), lowerRenderer.begin(), ::tolower);

    // Identify software rasterizers vs hardware acceleration
    if (lowerRenderer.find("llvmpipe") != std::string::npos) {
        m_info.isSoftware = true;
        m_info.accelerationReason = "Matched Mesa LLVMpipe software rasterizer";
    } else if (lowerRenderer.find("softpipe") != std::string::npos) {
        m_info.isSoftware = true;
        m_info.accelerationReason = "Matched Mesa Softpipe software rasterizer";
    } else if (lowerRenderer.find("swrast") != std::string::npos) {
        m_info.isSoftware = true;
        m_info.accelerationReason = "Matched Mesa SWRast fallback driver";
    } else if (lowerRenderer.find("software") != std::string::npos) {
        m_info.isSoftware = true;
        m_info.accelerationReason = "Renderer explicitly declares software execution";
    } else {
        m_info.isSoftware = false;
        m_info.accelerationReason = "Hardware driver identified (" + m_info.vendor + " " + m_info.renderer + ")";
    }

    KJD_LOG_INFO("Graphics Context Initialized:");
    KJD_LOG_INFO("  Vendor:   ", m_info.vendor);
    KJD_LOG_INFO("  Renderer: ", m_info.renderer);
    KJD_LOG_INFO("  Version:  ", m_info.version);
    KJD_LOG_INFO("  GLSL:     ", m_info.glslVersion);
    if (m_info.isSoftware) {
        KJD_LOG_WARN("  Mode:     SOFTWARE RENDERING (", m_info.accelerationReason, ")");
        KJD_LOG_WARN("  Notice:   Running without hardware acceleration. Frame rate may be restricted.");
    } else {
        KJD_LOG_INFO("  Mode:     HARDWARE ACCELERATED (", m_info.accelerationReason, ")");
    }
}

std::string Renderer::loadShaderSource(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static GLuint compileShader(GLenum type, const char* source, const std::string& name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength + 1);
        glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
        KJD_LOG_ERROR("Shader compilation failed (", name, "): ", infoLog.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Renderer::initShaders() {
    // Attempt to load from disk, fallback to embedded
    std::string vertSource;
    std::vector<std::string> vertPaths = {
        "shaders/cube.vert",
        "~/.local/share/kjd/shaders/cube.vert",
        "/usr/local/share/kjd/shaders/cube.vert"
    };
    for (const auto& p : vertPaths) {
        vertSource = loadShaderSource(p);
        if (!vertSource.empty()) {
            KJD_LOG_INFO("Loaded vertex shader from: ", p);
            break;
        }
    }
    if (vertSource.empty()) {
        KJD_LOG_INFO("Using embedded GLSL 330 core vertex shader");
        vertSource = EMBEDDED_VERT_SHADER;
    }

    std::string fragSource;
    std::vector<std::string> fragPaths = {
        "shaders/cube.frag",
        "~/.local/share/kjd/shaders/cube.frag",
        "/usr/local/share/kjd/shaders/cube.frag"
    };
    for (const auto& p : fragPaths) {
        fragSource = loadShaderSource(p);
        if (!fragSource.empty()) {
            KJD_LOG_INFO("Loaded fragment shader from: ", p);
            break;
        }
    }
    if (fragSource.empty()) {
        KJD_LOG_INFO("Using embedded GLSL 330 core fragment shader");
        fragSource = EMBEDDED_FRAG_SHADER;
    }

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSource.c_str(), "cube.vert");
    if (!vertShader) return false;

    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSource.c_str(), "cube.frag");
    if (!fragShader) {
        glDeleteShader(vertShader);
        return false;
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertShader);
    glAttachShader(m_shaderProgram, fragShader);
    glLinkProgram(m_shaderProgram);

    GLint linked = 0;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        glGetProgramiv(m_shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength + 1);
        glGetProgramInfoLog(m_shaderProgram, logLength, nullptr, infoLog.data());
        KJD_LOG_ERROR("Shader program link failed: ", infoLog.data());
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return false;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    // Cache uniform locations
    m_locMVP = glGetUniformLocation(m_shaderProgram, "uMVP");
    m_locModel = glGetUniformLocation(m_shaderProgram, "uModel");
    m_locLightDir = glGetUniformLocation(m_shaderProgram, "uLightDir");
    m_locViewPos = glGetUniformLocation(m_shaderProgram, "uViewPos");

    return true;
}

bool Renderer::initGeometry() {
    // 12 edges (24 vertices) defining the white wireframe bounding box of the cube
    // Corners of unit cube [-0.5, 0.5]:
    // C0: (-0.5, -0.5, -0.5)  C1: ( 0.5, -0.5, -0.5)
    // C2: ( 0.5,  0.5, -0.5)  C3: (-0.5,  0.5, -0.5)
    // C4: (-0.5, -0.5,  0.5)  C5: ( 0.5, -0.5,  0.5)
    // C6: ( 0.5,  0.5,  0.5)  C7: (-0.5,  0.5,  0.5)

    const float r = 1.0f, g = 1.0f, b = 1.0f; // Pure white edges (#FFFFFF)
    const float nx = 0.0f, ny = 0.0f, nz = 0.0f; // Zero normal indicates wireframe unlit edge

    const std::vector<Vertex> vertices = {
        // Bottom loop (Z = -0.5)
        {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, // C0 -> C1
        { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, // C1 -> C2
        { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, // C2 -> C3
        {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, // C3 -> C0

        // Top loop (Z = +0.5)
        {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, // C4 -> C5
        { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, // C5 -> C6
        { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, // C6 -> C7
        {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, // C7 -> C4

        // 4 Connecting vertical pillars
        {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, // C0 -> C4
        { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, // C1 -> C5
        { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, // C2 -> C6
        {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}  // C3 -> C7
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);

    // Normal attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(1);

    // Color attribute (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

bool Renderer::init(const AppConfig& config) {
    m_renderScale = std::clamp(config.render_scale, 0.25f, 2.0f);

    probeGraphicsContext();

    if (!initShaders()) {
        KJD_LOG_ERROR("Failed to compile or link shaders");
        return false;
    }

    if (!initGeometry()) {
        KJD_LOG_ERROR("Failed to initialize cube geometry");
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Pure black void background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    m_initialized = true;
    KJD_LOG_INFO("Renderer initialized successfully (Render Scale: ", m_renderScale, ")");
    return true;
}

void Renderer::render(const Scene& scene, int drawableWidth, int drawableHeight) {
    if (!m_initialized) return;

    int renderW = static_cast<int>(drawableWidth * m_renderScale);
    int renderH = static_cast<int>(drawableHeight * m_renderScale);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    glViewport(0, 0, renderW, renderH);

    // Clear background to pure black void
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_shaderProgram);

    float aspect = static_cast<float>(renderW) / static_cast<float>(renderH);
    Mat4 model = scene.getModelMatrix();
    Mat4 view = scene.getViewMatrix();
    Mat4 proj = scene.getProjectionMatrix(aspect);
    Mat4 mvp = proj * view * model;

    glUniformMatrix4fv(m_locMVP, 1, GL_FALSE, mvp.data());
    glUniformMatrix4fv(m_locModel, 1, GL_FALSE, model.data());

    // Directional light from top-right-front
    glUniform3f(m_locLightDir, 0.6f, -1.0f, -0.8f);

    Vec3 camPos = scene.getCameraPosition();
    glUniform3f(m_locViewPos, camPos.x, camPos.y, camPos.z);

    // Render wireframe cube with white edges
    glBindVertexArray(m_vao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    glUseProgram(0);
}

void Renderer::cleanup() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
    m_initialized = false;
}

} // namespace kjd
