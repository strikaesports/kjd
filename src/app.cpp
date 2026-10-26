#include "app.hpp"
#include "logger.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>

namespace kjd {

std::atomic<bool> App::s_running{true};

void App::signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        KJD_LOG_INFO("Received shutdown signal (", signal, "). Initiating graceful exit...");
        s_running = false;
    }
}

App::App(const AppConfig& config)
    : m_config(config),
      m_isFullscreen(config.fullscreen),
      m_drawableWidth(config.window_width),
      m_drawableHeight(config.window_height),
      m_effectiveTargetFps(config.target_fps) {
}

App::~App() {
    KJD_LOG_INFO("Releasing graphics and input resources...");
    if (m_inputReceiver) {
        m_inputReceiver->stop();
        m_inputReceiver.reset();
    }
    if (m_renderer) {
        m_renderer->cleanup();
        m_renderer.reset();
    }
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    KJD_LOG_INFO("KJD shutdown complete.");
}

void App::requestStop() {
    s_running = false;
}

bool App::init() {
    // Register signal handlers for clean systemd / terminal termination
    std::signal(SIGINT, App::signalHandler);
    std::signal(SIGTERM, App::signalHandler);

    // Configure video driver preference based on active session
    if (!std::getenv("SDL_VIDEODRIVER")) {
        if (std::getenv("WAYLAND_DISPLAY") && !std::getenv("DISPLAY")) {
            setenv("SDL_VIDEODRIVER", "wayland,x11", 1);
            KJD_LOG_INFO("Configured SDL_VIDEODRIVER=wayland,x11 for Wayland session");
        } else if (std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
            setenv("SDL_VIDEODRIVER", "x11,wayland", 1);
            KJD_LOG_INFO("Configured SDL_VIDEODRIVER=x11,wayland for X11 session");
        }
    }

    KJD_LOG_INFO("Initializing SDL video subsystem...");
    bool videoOk = (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    if (!videoOk) {
        std::string initialErr = SDL_GetError();
        KJD_LOG_WARN("Initial SDL_Init failed ('", initialErr, "'). Attempting video driver fallback...");

        // Try candidate drivers based on active environment
        std::vector<const char*> driverCandidates;
        if (std::getenv("WAYLAND_DISPLAY")) driverCandidates.push_back("wayland");
        if (std::getenv("DISPLAY")) driverCandidates.push_back("x11");
        driverCandidates.push_back("wayland");
        driverCandidates.push_back("x11");
        driverCandidates.push_back("KMSDRM");

        for (const char* drv : driverCandidates) {
            KJD_LOG_INFO("Trying SDL_VideoInit('", drv, "')...");
            if (SDL_VideoInit(drv) == 0) {
                videoOk = true;
                SDL_InitSubSystem(SDL_INIT_EVENTS);
                KJD_LOG_INFO("Successfully initialized SDL video subsystem using driver: ", drv);
                break;
            } else {
                KJD_LOG_WARN("SDL_VideoInit('", drv, "') failed: ", SDL_GetError());
            }
        }
    }

    if (!videoOk) {
        KJD_LOG_ERROR("All SDL video initializations failed: ", SDL_GetError());
        return false;
    }

    const char* activeDriver = SDL_GetCurrentVideoDriver();
    KJD_LOG_INFO("Active SDL Video Driver: ", (activeDriver ? activeDriver : "unknown"));

    SDL_version sdlCompiled{}, sdlLinked{};
    SDL_VERSION(&sdlCompiled);
    SDL_GetVersion(&sdlLinked);
    KJD_LOG_INFO("SDL Version: compiled ", (int)sdlCompiled.major, ".", (int)sdlCompiled.minor, ".", (int)sdlCompiled.patch,
                 ", linked ", (int)sdlLinked.major, ".", (int)sdlLinked.minor, ".", (int)sdlLinked.patch);

    // Request OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (m_isFullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    KJD_LOG_INFO("Creating window (target: ", m_drawableWidth, "x", m_drawableHeight,
                 ", fullscreen: ", (m_isFullscreen ? "true" : "false"), ")...");

    m_window = SDL_CreateWindow(
        "KJD Desktop Environment",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_drawableWidth,
        m_drawableHeight,
        windowFlags
    );

    if (!m_window && m_isFullscreen) {
        KJD_LOG_WARN("Failed to create fullscreen window: ", SDL_GetError(), ". Retrying in windowed mode...");
        m_isFullscreen = false;
        windowFlags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;
        m_window = SDL_CreateWindow(
            "KJD Desktop Environment",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            m_drawableWidth,
            m_drawableHeight,
            windowFlags
        );
    }

    if (!m_window) {
        KJD_LOG_ERROR("Failed to create SDL window: ", SDL_GetError());
        return false;
    }

    // Query actual drawable size (handles high-DPI and fullscreen surfaces)
    SDL_GL_GetDrawableSize(m_window, &m_drawableWidth, &m_drawableHeight);
    KJD_LOG_INFO("Drawable surface dimensions: ", m_drawableWidth, "x", m_drawableHeight);

    KJD_LOG_INFO("Creating OpenGL context...");
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        KJD_LOG_ERROR("SDL_GL_CreateContext failed: ", SDL_GetError());
        return false;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        // EGL and KMSDRM contexts do not use GLX, which causes glewInit to report non-zero.
        KJD_LOG_INFO("Note: glewInit reported status ", (int)glewErr, " ('",
                     reinterpret_cast<const char*>(glewGetErrorString(glewErr)),
                     "'). Resolving OpenGL function pointers via SDL...");
    }
    // Clear any harmless GL error generated by glewInit on modern core contexts
    while (glGetError() != GL_NO_ERROR);

    // Resolve all required OpenGL core function pointers directly from driver via SDL
    #define LOAD_GL(name, type) \
        if (!gl##name) __glew##name = (type)SDL_GL_GetProcAddress("gl" #name);

    LOAD_GL(GenVertexArrays, PFNGLGENVERTEXARRAYSPROC);
    LOAD_GL(BindVertexArray, PFNGLBINDVERTEXARRAYPROC);
    LOAD_GL(DeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC);
    LOAD_GL(GenBuffers, PFNGLGENBUFFERSPROC);
    LOAD_GL(BindBuffer, PFNGLBINDBUFFERPROC);
    LOAD_GL(BufferData, PFNGLBUFFERDATAPROC);
    LOAD_GL(DeleteBuffers, PFNGLDELETEBUFFERSPROC);
    LOAD_GL(VertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC);
    LOAD_GL(EnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC);
    LOAD_GL(CreateShader, PFNGLCREATESHADERPROC);
    LOAD_GL(ShaderSource, PFNGLSHADERSOURCEPROC);
    LOAD_GL(CompileShader, PFNGLCOMPILESHADERPROC);
    LOAD_GL(GetShaderiv, PFNGLGETSHADERIVPROC);
    LOAD_GL(GetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);
    LOAD_GL(DeleteShader, PFNGLDELETESHADERPROC);
    LOAD_GL(CreateProgram, PFNGLCREATEPROGRAMPROC);
    LOAD_GL(AttachShader, PFNGLATTACHSHADERPROC);
    LOAD_GL(LinkProgram, PFNGLLINKPROGRAMPROC);
    LOAD_GL(GetProgramiv, PFNGLGETPROGRAMIVPROC);
    LOAD_GL(GetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    LOAD_GL(UseProgram, PFNGLUSEPROGRAMPROC);
    LOAD_GL(DeleteProgram, PFNGLDELETEPROGRAMPROC);
    LOAD_GL(GetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC);
    LOAD_GL(UniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC);
    LOAD_GL(Uniform3f, PFNGLUNIFORM3FPROC);
    #undef LOAD_GL

    if (!glGenVertexArrays || !glCreateShader || !glCreateProgram) {
        KJD_LOG_ERROR("Required OpenGL core function pointers could not be resolved from active driver.");
        return false;
    }
    KJD_LOG_INFO("OpenGL core function pointers verified successfully.");

    // Configure VSync
    int swapResult = SDL_GL_SetSwapInterval(m_config.vsync ? 1 : 0);
    if (swapResult != 0) {
        KJD_LOG_WARN("SDL_GL_SetSwapInterval failed (", SDL_GetError(), "). Continuing without hardware vsync.");
    } else {
        KJD_LOG_INFO("VSync enabled: ", (m_config.vsync ? "true" : "false"));
    }

    // Initialize Renderer
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->init(m_config)) {
        KJD_LOG_ERROR("Renderer initialization failed");
        return false;
    }

    // Software rendering frame rate adjustment per specification
    const auto& gfx = m_renderer->getGraphicsInfo();
    if (gfx.isSoftware && m_config.target_fps == 60) {
        m_effectiveTargetFps = 30;
        KJD_LOG_INFO("Adjusted target FPS to 30 due to detected software rendering rasterizer.");
    } else {
        m_effectiveTargetFps = m_config.target_fps;
    }

    KJD_LOG_INFO("Target Frame Rate: ", m_effectiveTargetFps, " FPS (vsync=", (m_config.vsync ? "true" : "false"), ")");

    // Initialize Scene
    m_scene = std::make_unique<Scene>(m_config);
    KJD_LOG_INFO("3D Scene initialized (Stationary/Rotate: ", (m_scene->isRotating() ? "rotating" : "stationary"), ")");

    // Initialize UDP Input Receiver
    m_inputReceiver = std::make_unique<InputReceiver>();
    if (!m_inputReceiver->start(m_config.input)) {
        KJD_LOG_WARN("InputReceiver could not bind to requested address/port. Running without UDP controller.");
    }

    return true;
}

void App::toggleFullscreen() {
    m_isFullscreen = !m_isFullscreen;
    Uint32 flags = m_isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(m_window, flags) == 0) {
        SDL_GL_GetDrawableSize(m_window, &m_drawableWidth, &m_drawableHeight);
        KJD_LOG_INFO("Toggled fullscreen: ", (m_isFullscreen ? "ON" : "OFF"),
                     " (Resolution: ", m_drawableWidth, "x", m_drawableHeight, ")");
    } else {
        KJD_LOG_WARN("Failed to toggle fullscreen: ", SDL_GetError());
    }
}

void App::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        KJD_LOG_INFO("Quit event received. Stopping...");
        s_running = false;
    } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
            case SDLK_q:
                KJD_LOG_INFO("Exit key pressed ('", SDL_GetKeyName(event.key.keysym.sym), "'). Stopping...");
                s_running = false;
                break;
            case SDLK_F11:
                toggleFullscreen();
                break;
            case SDLK_SPACE:
                if (m_scene) {
                    m_scene->toggleRotation();
                    KJD_LOG_INFO("Cube rotation: ", (m_scene->isRotating() ? "Resumed" : "Paused"));
                }
                break;
    } else if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_RMASK) {
            // Right-click drag orbits the cube
            float sens = 0.3f;
            m_scene->addOrbit(static_cast<float>(event.motion.xrel) * sens,
                              -static_cast<float>(event.motion.yrel) * sens);
        } else {
            // Mouse movement drives the 5x5x5 cursor
            float mouseSens = 0.5f;
            m_scene->moveCursor(static_cast<float>(event.motion.xrel) * mouseSens,
                                -static_cast<float>(event.motion.yrel) * mouseSens,
                                0.0f);
        }
    } else if (event.type == SDL_MOUSEWHEEL) {
        // Mouse wheel moves cursor in depth (Z axis)
        float wheelSens = 2.0f;
        m_scene->moveCursor(0.0f, 0.0f, static_cast<float>(event.wheel.y) * wheelSens);
    } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            SDL_GL_GetDrawableSize(m_window, &m_drawableWidth, &m_drawableHeight);
            KJD_LOG_DEBUG("Window resized to: ", m_drawableWidth, "x", m_drawableHeight);
        }
    }
}

void App::logPerformanceStats(double elapsedSeconds) {
    if (m_frameCount == 0 || elapsedSeconds <= 0.0) return;

    double avgFps = static_cast<double>(m_frameCount) / elapsedSeconds;
    double avgFrameTimeMs = (m_totalFrameTime / static_cast<double>(m_frameCount)) * 1000.0;
    double minMs = m_minFrameTime * 1000.0;
    double maxMs = m_maxFrameTime * 1000.0;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1)
       << "Performance stats: Avg FPS: " << avgFps
       << " | Avg Frame Time: " << std::setprecision(2) << avgFrameTimeMs << " ms"
       << " (Min: " << minMs << " ms, Max: " << maxMs << " ms)"
       << " | Total Frames: " << m_frameCount;

    KJD_LOG_INFO(ss.str());

    if (m_inputReceiver) {
        auto inStats = m_inputReceiver->getStats();
        KJD_LOG_INFO("  [Input Stats] Connected: ", (inStats.connected ? ("Yes ('" + inStats.activeDevice + "')") : "No"),
                     " | Packets: ", inStats.packetsAccepted, " accepted, ", inStats.packetsRejected, " rejected",
                     " | Missing seq: ", inStats.missingSequenceCount, " | Timeouts: ", inStats.timeoutCount);
    }

    // Reset periodic counters
    m_frameCount = 0;
    m_totalFrameTime = 0.0;
    m_minFrameTime = 1e9;
    m_maxFrameTime = 0.0;
}

int App::run() {
    KJD_LOG_INFO("Entering KJD main rendering loop...");

    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();
    auto statsLastTime = Clock::now();

    const double targetFrameDuration = (m_effectiveTargetFps > 0) ? (1.0 / m_effectiveTargetFps) : 0.0;

    while (s_running.load()) {
        auto frameStartTime = Clock::now();

        // 1. Process Window / Input Events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handleEvent(event);
            if (!s_running.load()) break;
        }
        if (!s_running.load()) break;

        // 2. Process Mobile UDP Input Events and State
        InputState inState;
        ReceiverStats inStats;

        if (m_inputReceiver) {
            auto inputEvents = m_inputReceiver->drainEvents();
            for (const auto& ev : inputEvents) {
                switch (ev.type) {
                    case InputEventType::LeftClick:
                        KJD_LOG_INFO("[Input Event] Left click event triggered from device '", ev.deviceId, "'");
                        break;
                    case InputEventType::CalibrationRequested:
                        KJD_LOG_INFO("[Input Event] Calibration requested from device '", ev.deviceId, "'. Resetting orientation and cursor.");
                        m_scene->resetOrientation();
                        m_scene->resetCursor();
                        break;
                    case InputEventType::DeviceConnected:
                        KJD_LOG_INFO("[Input Event] Mobile device connected: '", ev.deviceId, "'");
                        break;
                    case InputEventType::DeviceDisconnected:
                        KJD_LOG_INFO("[Input Event] Mobile device disconnected: '", ev.deviceId, "'");
                        break;
                }
            }

            inState = m_inputReceiver->getLatestState();
            inStats = m_inputReceiver->getStats();

            if (inState.connected) {
                if (inState.orbitPressed) {
                    // Positive X: right -> increases yaw
                    // Positive Y: up -> increases pitch
                    float orbitSens = m_config.input.orbit_sensitivity * 15.0f;
                    m_scene->addOrbit(inState.deltaX * orbitSens, inState.deltaY * orbitSens);
                } else if (inState.zoomPressed) {
                    // Positive Z: forward -> zooms in closer
                    float zoomSens = m_config.input.zoom_sensitivity * 0.4f;
                    m_scene->addZoom(inState.deltaZ * zoomSens);
                } else {
                    // Move 5x5x5 cursor along with mouse gadget deltas
                    if (inState.deltaX != 0.0f || inState.deltaY != 0.0f || inState.deltaZ != 0.0f) {
                        float cursorSens = m_config.input.orbit_sensitivity * 1.5f;
                        m_scene->moveCursor(inState.deltaX * cursorSens,
                                            inState.deltaY * cursorSens,
                                            inState.deltaZ * cursorSens);
                    }
                }
            }
        }

        // 3. Compute delta time
        auto now = Clock::now();
        std::chrono::duration<float> deltaDuration = now - lastTime;
        float deltaTime = deltaDuration.count();
        lastTime = now;

        // Clamp excessive delta time (e.g. if paused or dragged)
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // 4. Update 3D Scene
        m_scene->update(deltaTime);

        // 5. Render Frame to Back Buffer (with active voxels & top-right HUD)
        m_renderer->render(*m_scene, m_drawableWidth, m_drawableHeight, inState, inStats);

        // 5. Present Frame
        SDL_GL_SwapWindow(m_window);

        // 6. Frame timing & Frame rate limiting to avoid busy spinning
        auto frameEndTime = Clock::now();
        std::chrono::duration<double> frameDuration = frameEndTime - frameStartTime;
        double frameTimeSec = frameDuration.count();

        if (targetFrameDuration > 0.0 && frameTimeSec < targetFrameDuration) {
            double sleepTimeSec = targetFrameDuration - frameTimeSec;
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTimeSec));
            auto postSleepTime = Clock::now();
            frameDuration = postSleepTime - frameStartTime;
            frameTimeSec = frameDuration.count();
        }

        // 7. Track Performance Metrics
        m_frameCount++;
        m_totalFrameTime += frameTimeSec;
        if (frameTimeSec < m_minFrameTime) m_minFrameTime = frameTimeSec;
        if (frameTimeSec > m_maxFrameTime) m_maxFrameTime = frameTimeSec;

        std::chrono::duration<double> statsElapsed = frameEndTime - statsLastTime;
        if (statsElapsed.count() >= static_cast<double>(m_config.stats_interval_sec)) {
            logPerformanceStats(statsElapsed.count());
            statsLastTime = Clock::now();
        }
    }

    KJD_LOG_INFO("Exited main rendering loop.");
    return 0;
}

} // namespace kjd
