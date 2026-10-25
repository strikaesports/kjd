#pragma once

#include "config.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include <SDL2/SDL.h>
#include <atomic>
#include <memory>

namespace kjd {

class App {
public:
    explicit App(const AppConfig& config);
    ~App();

    bool init();
    int run();
    void requestStop();

private:
    void handleEvent(const SDL_Event& event);
    void toggleFullscreen();
    void logPerformanceStats(double elapsedSeconds);

    AppConfig m_config;
    SDL_Window* m_window{nullptr};
    SDL_GLContext m_glContext{nullptr};

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Scene> m_scene;

    bool m_isFullscreen{false};
    int m_drawableWidth{1920};
    int m_drawableHeight{1080};

    // Frame timing & statistics
    int m_effectiveTargetFps{60};
    uint64_t m_frameCount{0};
    double m_statsTimer{0.0};
    double m_totalFrameTime{0.0};
    double m_minFrameTime{1e9};
    double m_maxFrameTime{0.0};

    static std::atomic<bool> s_running;
    static void signalHandler(int signal);
};

} // namespace kjd
