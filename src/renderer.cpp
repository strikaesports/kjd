#include "renderer.hpp"
#include "logger.hpp"
#include "font8x8.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>

namespace kjd {

// Fallback embedded 3D vertex shader
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

// Fallback embedded 3D fragment shader (supports wireframe unlit and shaded voxels)
static const char* EMBEDDED_FRAG_SHADER = R"(#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;

void main() {
    // For wireframe lines and unlit points (normal length near zero), render crisp color directly
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

// 2D HUD shaders
static const char* HUD_VERT_SHADER = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

uniform vec2 uScreenSize;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    // Convert 2D pixel coordinates (0,0 top-left) to NDC [-1, 1]
    vec2 ndc = vec2((aPos.x / uScreenSize.x) * 2.0 - 1.0, 1.0 - (aPos.y / uScreenSize.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

static const char* HUD_FRAG_SHADER = R"(#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uFontTex;

out vec4 FragColor;

void main() {
    if (vTexCoord.x < 0.0) {
        // Solid untextured quad
        FragColor = vColor;
    } else {
        // Font character glyph
        float alpha = texture(uFontTex, vTexCoord).r;
        FragColor = vec4(vColor.rgb, vColor.a * alpha);
    }
}
)";

struct Vertex3D {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

struct HudVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
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

    if (lowerRenderer.find("llvmpipe") != std::string::npos ||
        lowerRenderer.find("softpipe") != std::string::npos ||
        lowerRenderer.find("swrast") != std::string::npos ||
        lowerRenderer.find("software") != std::string::npos) {
        m_info.isSoftware = true;
        m_info.accelerationReason = "Software rasterizer identified (" + m_info.renderer + ")";
    } else {
        m_info.isSoftware = false;
        m_info.accelerationReason = "Hardware driver identified (" + m_info.vendor + " " + m_info.renderer + ")";
    }

    KJD_LOG_INFO("Graphics Context Probed:");
    KJD_LOG_INFO("  Vendor:   ", m_info.vendor);
    KJD_LOG_INFO("  Renderer: ", m_info.renderer);
    KJD_LOG_INFO("  Version:  ", m_info.version);
    KJD_LOG_INFO("  GLSL:     ", m_info.glslVersion);
    KJD_LOG_INFO("  Mode:     ", (m_info.isSoftware ? "SOFTWARE RENDERING" : "HARDWARE ACCELERATED"), " (", m_info.accelerationReason, ")");
}

std::string Renderer::loadShaderSource(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static GLuint compileShader(GLenum type, const char* source, const char* name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> infoLog(logLen + 1);
        glGetShaderInfoLog(shader, logLen, nullptr, infoLog.data());
        KJD_LOG_ERROR("Failed to compile ", name, ": ", infoLog.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Renderer::initShaders() {
    std::string vertSrc = loadShaderSource("shaders/cube.vert");
    if (vertSrc.empty()) vertSrc = EMBEDDED_VERT_SHADER;

    std::string fragSrc = loadShaderSource("shaders/cube.frag");
    if (fragSrc.empty()) fragSrc = EMBEDDED_FRAG_SHADER;

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc.c_str(), "3D Vertex Shader");
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), "3D Fragment Shader");
    if (!vertShader || !fragShader) return false;

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertShader);
    glAttachShader(m_shaderProgram, fragShader);
    glLinkProgram(m_shaderProgram);

    GLint linked = 0;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &linked);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    if (!linked) {
        KJD_LOG_ERROR("Failed to link 3D shader program.");
        return false;
    }

    m_locMVP = glGetUniformLocation(m_shaderProgram, "uMVP");
    m_locModel = glGetUniformLocation(m_shaderProgram, "uModel");
    m_locLightDir = glGetUniformLocation(m_shaderProgram, "uLightDir");
    m_locViewPos = glGetUniformLocation(m_shaderProgram, "uViewPos");

    return true;
}

bool Renderer::initGeometry() {
    // 12 edges (24 vertices) defining the white wireframe bounding box of the cube
    const float r = 1.0f, g = 1.0f, b = 1.0f;
    const float nx = 0.0f, ny = 0.0f, nz = 0.0f;

    const std::vector<Vertex3D> wireVertices = {
        // Bottom loop (Z = -0.5)
        {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b},
        { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b},
        { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b},
        {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b},

        // Top loop (Z = +0.5)
        {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b},
        { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b},
        { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b},
        {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b},

        // 4 Connecting pillars
        {-0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b},
        { 0.5f, -0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f, -0.5f,  0.5f, nx, ny, nz, r, g, b},
        { 0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, { 0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b},
        {-0.5f,  0.5f, -0.5f, nx, ny, nz, r, g, b}, {-0.5f,  0.5f,  0.5f, nx, ny, nz, r, g, b}
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, wireVertices.size() * sizeof(Vertex3D), wireVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, nx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, r));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Setup dynamic VBO for voxel wave points
    glGenVertexArrays(1, &m_voxelVao);
    glGenBuffers(1, &m_voxelVbo);

    glBindVertexArray(m_voxelVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_voxelVbo);

    // Location 0: Pos (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVolume::VoxelVertex), (void*)offsetof(VoxelVolume::VoxelVertex, x));
    glEnableVertexAttribArray(0);
    // Location 1: Normal (constant 0.0 for unlit points)
    glVertexAttrib3f(1, 0.0f, 0.0f, 0.0f);
    glDisableVertexAttribArray(1);
    // Location 2: Color (vec3)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVolume::VoxelVertex), (void*)offsetof(VoxelVolume::VoxelVertex, r));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

bool Renderer::initHud() {
    GLuint vertShader = compileShader(GL_VERTEX_SHADER, HUD_VERT_SHADER, "HUD Vertex Shader");
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, HUD_FRAG_SHADER, "HUD Fragment Shader");
    if (!vertShader || !fragShader) return false;

    m_hudProgram = glCreateProgram();
    glAttachShader(m_hudProgram, vertShader);
    glAttachShader(m_hudProgram, fragShader);
    glLinkProgram(m_hudProgram);

    GLint linked = 0;
    glGetProgramiv(m_hudProgram, GL_LINK_STATUS, &linked);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    if (!linked) {
        KJD_LOG_ERROR("Failed to link HUD shader program.");
        return false;
    }

    m_locHudScreenSize = glGetUniformLocation(m_hudProgram, "uScreenSize");
    m_locHudUseTex = glGetUniformLocation(m_hudProgram, "uFontTex");

    // Upload 128x64 font texture from FONT8X8_BASIC (ASCII 32 to 126)
    // 16 columns of 8x8 glyphs = 128 pixels wide; 6 rows = 48 pixels high (in 64 height)
    std::vector<uint8_t> fontPixels(128 * 64, 0);

    for (int i = 0; i < 95; ++i) {
        int col = i % 16;
        int row = i / 16;
        int baseX = col * 8;
        int baseY = row * 8;

        for (int y = 0; y < 8; ++y) {
            uint8_t byteVal = FONT8X8_BASIC[i][y];
            for (int x = 0; x < 8; ++x) {
                bool on = (byteVal & (1 << (7 - x))) != 0;
                fontPixels[(baseY + y) * 128 + (baseX + x)] = on ? 255 : 0;
            }
        }
    }

    glGenTextures(1, &m_fontTexture);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 128, 64, 0, GL_RED, GL_UNSIGNED_BYTE, fontPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Dynamic HUD geometry buffer
    glGenVertexArrays(1, &m_hudVao);
    glGenBuffers(1, &m_hudVbo);

    glBindVertexArray(m_hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_hudVbo);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex), (void*)offsetof(HudVertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex), (void*)offsetof(HudVertex, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(HudVertex), (void*)offsetof(HudVertex, r));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

static void addSolidQuad(std::vector<HudVertex>& verts, float x, float y, float w, float h,
                         float r, float g, float b, float a) {
    verts.push_back({x,     y,     -1.0f, -1.0f, r, g, b, a});
    verts.push_back({x,     y + h, -1.0f, -1.0f, r, g, b, a});
    verts.push_back({x + w, y + h, -1.0f, -1.0f, r, g, b, a});

    verts.push_back({x,     y,     -1.0f, -1.0f, r, g, b, a});
    verts.push_back({x + w, y + h, -1.0f, -1.0f, r, g, b, a});
    verts.push_back({x + w, y,     -1.0f, -1.0f, r, g, b, a});
}

static void addOutlineRect(std::vector<HudVertex>& verts, float x, float y, float w, float h,
                           float r, float g, float b, float a, float thickness = 1.0f) {
    addSolidQuad(verts, x, y, w, thickness, r, g, b, a);                     // Top
    addSolidQuad(verts, x, y + h - thickness, w, thickness, r, g, b, a);     // Bottom
    addSolidQuad(verts, x, y, thickness, h, r, g, b, a);                     // Left
    addSolidQuad(verts, x + w - thickness, y, thickness, h, r, g, b, a);     // Right
}

static void addChar(std::vector<HudVertex>& verts, float x, float y, char c, float scale,
                    float r, float g, float b, float a) {
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    int col = idx % 16;
    int row = idx / 16;

    float u0 = (col * 8.0f) / 128.0f;
    float u1 = ((col + 1) * 8.0f) / 128.0f;
    float v0 = (row * 8.0f) / 64.0f;
    float v1 = ((row + 1) * 8.0f) / 64.0f;

    float w = 8.0f * scale;
    float h = 8.0f * scale;

    verts.push_back({x,     y,     u0, v0, r, g, b, a});
    verts.push_back({x,     y + h, u0, v1, r, g, b, a});
    verts.push_back({x + w, y + h, u1, v1, r, g, b, a});

    verts.push_back({x,     y,     u0, v0, r, g, b, a});
    verts.push_back({x + w, y + h, u1, v1, r, g, b, a});
    verts.push_back({x + w, y,     u1, v0, r, g, b, a});
}

static void addText(std::vector<HudVertex>& verts, float x, float y, const std::string& text,
                    float scale, float r, float g, float b, float a) {
    float curX = x;
    for (char ch : text) {
        addChar(verts, curX, y, ch, scale, r, g, b, a);
        curX += 8.0f * scale;
    }
}

void Renderer::renderHud(int width, int height, const InputState& inputState, const ReceiverStats& inputStats) {
    if (width <= 0 || height <= 0) return;

    std::vector<HudVertex> hudVerts;
    hudVerts.reserve(256);

    const float panelW = 310.0f;
    const float panelH = 92.0f;
    const float margin = 16.0f;
    const float px = static_cast<float>(width) - panelW - margin;
    const float py = margin;

    // 1. Panel Background Card and glowing border
    addSolidQuad(hudVerts, px, py, panelW, panelH, 0.05f, 0.06f, 0.09f, 0.88f);
    addOutlineRect(hudVerts, px, py, panelW, panelH, 0.25f, 0.45f, 0.65f, 0.85f, 1.5f);

    // 2. Line 1: Connection Status & Device ID
    float line1Y = py + 10.0f;
    if (inputState.connected) {
        // Bright green connection badge
        addSolidQuad(hudVerts, px + 12.0f, line1Y + 1.0f, 8.0f, 8.0f, 0.0f, 1.0f, 0.5f, 1.0f);
        addText(hudVerts, px + 26.0f, line1Y, "AIRMOUSE: CONNECTED", 1.0f, 0.0f, 1.0f, 0.5f, 1.0f);

        std::string devStr = inputState.deviceId.empty() ? "mobile-01" : inputState.deviceId;
        if (devStr.size() > 10) devStr = devStr.substr(0, 10);
        addText(hudVerts, px + 196.0f, line1Y, "[" + devStr + "]", 1.0f, 0.7f, 0.85f, 1.0f, 0.9f);
    } else {
        // Dim amber/waiting badge
        addSolidQuad(hudVerts, px + 12.0f, line1Y + 1.0f, 8.0f, 8.0f, 1.0f, 0.6f, 0.1f, 0.9f);
        addText(hudVerts, px + 26.0f, line1Y, "AIRMOUSE: WAITING...", 1.0f, 1.0f, 0.7f, 0.2f, 0.95f);
        std::string portStr = ":" + std::to_string(inputStats.boundPort > 0 ? inputStats.boundPort : 9000);
        addText(hudVerts, px + 215.0f, line1Y, portStr, 1.0f, 0.6f, 0.6f, 0.65f, 0.85f);
    }

    // 3. Line 2: Synchronized Button Indicators (LEFT, ORBIT, ZOOM, CALIBRATE)
    float line2Y = py + 29.0f;
    auto drawButtonBadge = [&](float bx, const std::string& label, bool pressed, float pr, float pg, float pb) {
        float bw = static_cast<float>(label.size() * 8 + 12);
        float bh = 15.0f;
        if (pressed) {
            addSolidQuad(hudVerts, bx, line2Y, bw, bh, pr, pg, pb, 0.95f);
            addText(hudVerts, bx + 6.0f, line2Y + 3.0f, label, 1.0f, 0.05f, 0.05f, 0.05f, 1.0f);
        } else {
            addOutlineRect(hudVerts, bx, line2Y, bw, bh, 0.35f, 0.40f, 0.50f, 0.7f, 1.0f);
            addText(hudVerts, bx + 6.0f, line2Y + 3.0f, label, 1.0f, 0.55f, 0.60f, 0.70f, 0.8f);
        }
        return bx + bw + 6.0f;
    };

    float curBx = px + 12.0f;
    curBx = drawButtonBadge(curBx, "L", inputState.leftPressed, 0.1f, 0.9f, 0.4f);
    curBx = drawButtonBadge(curBx, "ORBIT", inputState.orbitPressed, 0.1f, 0.8f, 1.0f);
    curBx = drawButtonBadge(curBx, "ZOOM", inputState.zoomPressed, 1.0f, 0.75f, 0.2f);
    drawButtonBadge(curBx, "CALIB", inputState.calibratePressed, 0.85f, 0.4f, 1.0f);

    // 4. Line 3: Live Movement Deltas (dX, dY, dZ)
    float line3Y = py + 51.0f;
    std::ostringstream deltaOss;
    deltaOss << std::fixed << std::setprecision(2)
             << "dX:" << (inputState.deltaX >= 0.0f ? "+" : "") << inputState.deltaX
             << "  dY:" << (inputState.deltaY >= 0.0f ? "+" : "") << inputState.deltaY
             << "  dZ:" << (inputState.deltaZ >= 0.0f ? "+" : "") << inputState.deltaZ;
    addText(hudVerts, px + 12.0f, line3Y, deltaOss.str(), 1.0f, 0.85f, 0.90f, 0.95f, 0.95f);

    // 5. Line 4: Packet Statistics & Throughput
    float line4Y = py + 71.0f;
    std::string statStr = "Rx: " + std::to_string(inputStats.packetsAccepted) + " pkts" +
                          " | Miss: " + std::to_string(inputStats.missingSequenceCount);
    addText(hudVerts, px + 12.0f, line4Y, statStr, 1.0f, 0.55f, 0.65f, 0.75f, 0.85f);

    if (hudVerts.empty()) return;

    // Execute 2D HUD render pass
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_hudProgram);
    glUniform2f(m_locHudScreenSize, static_cast<float>(width), static_cast<float>(height));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glUniform1i(m_locHudUseTex, 0);

    glBindVertexArray(m_hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_hudVbo);
    glBufferData(GL_ARRAY_BUFFER, hudVerts.size() * sizeof(HudVertex), hudVerts.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(hudVerts.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
}

bool Renderer::init(const AppConfig& config) {
    m_renderScale = std::clamp(config.render_scale, 0.25f, 2.0f);

    probeGraphicsContext();

    if (!initShaders()) {
        KJD_LOG_ERROR("Failed to compile or link 3D shaders");
        return false;
    }

    if (!initGeometry()) {
        KJD_LOG_ERROR("Failed to initialize cube and voxel geometry");
        return false;
    }

    if (!initHud()) {
        KJD_LOG_ERROR("Failed to initialize HUD subsystem");
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

void Renderer::render(const Scene& scene, int drawableWidth, int drawableHeight,
                      const InputState& inputState, const ReceiverStats& inputStats) {
    if (!m_initialized) return;

    int renderW = static_cast<int>(drawableWidth * m_renderScale);
    int renderH = static_cast<int>(drawableHeight * m_renderScale);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    glViewport(0, 0, renderW, renderH);

    // Clear background to pure black void
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. 3D Scene Rendering
    glUseProgram(m_shaderProgram);

    float aspect = static_cast<float>(renderW) / static_cast<float>(renderH);
    Mat4 model = scene.getModelMatrix();
    Mat4 view = scene.getViewMatrix();
    Mat4 proj = scene.getProjectionMatrix(aspect);
    Mat4 mvp = proj * view * model;

    glUniformMatrix4fv(m_locMVP, 1, GL_FALSE, mvp.data());
    glUniformMatrix4fv(m_locModel, 1, GL_FALSE, model.data());

    glUniform3f(m_locLightDir, 0.6f, -1.0f, -0.8f);
    Vec3 camPos = scene.getCameraPosition();
    glUniform3f(m_locViewPos, camPos.x, camPos.y, camPos.z);

    // Draw wireframe cube with white edges
    glBindVertexArray(m_vao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    // Draw active diagonal voxel wave points inside the wireframe cube
    if (scene.getVoxelVolume()) {
        const auto& voxelVertices = scene.getVoxelVolume()->getActiveVoxelVertices();
        if (!voxelVertices.empty()) {
            glBindVertexArray(m_voxelVao);
            glBindBuffer(GL_ARRAY_BUFFER, m_voxelVbo);
            glBufferData(GL_ARRAY_BUFFER, voxelVertices.size() * sizeof(VoxelVolume::VoxelVertex),
                         voxelVertices.data(), GL_DYNAMIC_DRAW);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glPointSize(3.5f);

            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(voxelVertices.size()));

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    glUseProgram(0);

    // 2. 2D HUD Mouse Status Overlay (Top-Right)
    renderHud(renderW, renderH, inputState, inputStats);
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
    if (m_voxelVao) {
        glDeleteVertexArrays(1, &m_voxelVao);
        m_voxelVao = 0;
    }
    if (m_voxelVbo) {
        glDeleteBuffers(1, &m_voxelVbo);
        m_voxelVbo = 0;
    }
    if (m_hudVao) {
        glDeleteVertexArrays(1, &m_hudVao);
        m_hudVao = 0;
    }
    if (m_hudVbo) {
        glDeleteBuffers(1, &m_hudVbo);
        m_hudVbo = 0;
    }
    if (m_fontTexture) {
        glDeleteTextures(1, &m_fontTexture);
        m_fontTexture = 0;
    }
    if (m_hudProgram) {
        glDeleteProgram(m_hudProgram);
        m_hudProgram = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
    m_initialized = false;
}

} // namespace kjd
