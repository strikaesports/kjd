#include "logger.hpp"
#include "config.hpp"
#include "app.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>

constexpr const char* KJD_VERSION = "0.1.0-phase0";

void printUsage(const char* prog) {
    std::cout << "KJD Desktop Environment — 3D Shell Prototype (Phase 0)\n"
              << "Version: " << KJD_VERSION << "\n\n"
              << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -v, --version         Show version information\n"
              << "  -c, --config <file>   Specify path to configuration file (TOML)\n"
              << "  -w, --windowed        Force windowed mode (overrides config)\n"
              << "  -f, --fullscreen      Force fullscreen mode (overrides config)\n"
              << "      --fps <rate>      Override target frame rate (e.g. 60 or 30)\n"
              << "      --no-rotate       Start with a stationary cube (no auto-rotation)\n"
              << "      --debug           Enable debug level logging\n\n"
              << "Runtime Controls:\n"
              << "  Esc / Q               Exit cleanly\n"
              << "  F11                   Toggle fullscreen\n"
              << "  Space                 Toggle cube rotation pause/resume\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string configPath = "";
    bool forceWindowed = false;
    bool forceFullscreen = false;
    int overrideFps = 0;
    bool overrideRotate = false;
    bool disableRotate = false;
    bool debugLogging = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "KJD " << KJD_VERSION << " (Phase 0: Black Void + 3D Cube)" << std::endl;
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "-w" || arg == "--windowed") {
            forceWindowed = true;
        } else if (arg == "-f" || arg == "--fullscreen") {
            forceFullscreen = true;
        } else if (arg == "--fps" && i + 1 < argc) {
            overrideFps = std::atoi(argv[++i]);
        } else if (arg == "--no-rotate") {
            disableRotate = true;
            overrideRotate = true;
        } else if (arg == "--spin") {
            disableRotate = false;
            overrideRotate = true;
        } else if (arg == "--debug") {
            debugLogging = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (debugLogging) {
        kjd::Logger::instance().setLevel(kjd::LogLevel::DEBUG);
    }

    KJD_LOG_INFO("=================================================");
    KJD_LOG_INFO("Starting KJD Desktop Environment — Version ", KJD_VERSION);
    KJD_LOG_INFO("=================================================");

    // Check and auto-detect display environment
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* x11Display = std::getenv("DISPLAY");

    // Auto-detect Wayland socket in XDG_RUNTIME_DIR if WAYLAND_DISPLAY is not explicitly set
    if (!waylandDisplay) {
        const char* xdgRuntime = std::getenv("XDG_RUNTIME_DIR");
        std::string runtimeDir = (xdgRuntime && *xdgRuntime) ? xdgRuntime : ("/run/user/" + std::to_string(getuid()));
        for (int i = 0; i <= 3; ++i) {
            std::string socketName = "wayland-" + std::to_string(i);
            std::string socketPath = runtimeDir + "/" + socketName;
            if (access(socketPath.c_str(), R_OK | W_OK) == 0) {
                setenv("WAYLAND_DISPLAY", socketName.c_str(), 1);
                waylandDisplay = std::getenv("WAYLAND_DISPLAY");
                KJD_LOG_INFO("Auto-detected active Wayland display socket at: ", socketPath);
                break;
            }
        }
    }

    // Auto-detect X11 socket in /tmp/.X11-unix if DISPLAY is not explicitly set
    if (!x11Display && !waylandDisplay) {
        for (int i = 0; i <= 3; ++i) {
            std::string xSocket = "/tmp/.X11-unix/X" + std::to_string(i);
            if (access(xSocket.c_str(), R_OK) == 0) {
                std::string dispName = ":" + std::to_string(i);
                setenv("DISPLAY", dispName.c_str(), 1);
                x11Display = std::getenv("DISPLAY");
                KJD_LOG_INFO("Auto-detected active X11 display socket at: ", xSocket);
                break;
            }
        }
    }

    if (!waylandDisplay && !x11Display) {
        KJD_LOG_ERROR("Neither WAYLAND_DISPLAY nor DISPLAY is set or active in /run/user/", getuid(), " or /tmp/.X11-unix.");
        KJD_LOG_ERROR("KJD requires an active Wayland compositor or X11 server.");
        KJD_LOG_ERROR("To initiate the DE on Ubuntu Server without an existing desktop, run:");
        KJD_LOG_ERROR("  kjd-session   (or: cage -- kjd)");
        return 1;
    }

    if (waylandDisplay) {
        KJD_LOG_INFO("Active Wayland display: ", waylandDisplay);
    }
    if (x11Display) {
        KJD_LOG_INFO("Active X11 display: ", x11Display);
    }

    // Load configuration
    kjd::AppConfig config = kjd::ConfigManager::load(configPath);

    // Apply CLI overrides
    if (forceWindowed) {
        config.fullscreen = false;
        KJD_LOG_INFO("CLI override: Windowed mode requested");
    } else if (forceFullscreen) {
        config.fullscreen = true;
        KJD_LOG_INFO("CLI override: Fullscreen mode requested");
    }

    if (overrideFps > 0) {
        config.target_fps = overrideFps;
        KJD_LOG_INFO("CLI override: Target FPS set to ", overrideFps);
    }

    if (overrideRotate) {
        config.rotate_cube = !disableRotate;
        KJD_LOG_INFO("CLI override: Cube rotation set to ", (config.rotate_cube ? "enabled" : "disabled"));
    } else {
        config.rotate_cube = false; // Stationary cube by default
    }

    try {
        kjd::App app(config);
        if (!app.init()) {
            KJD_LOG_ERROR("Application initialization failed. Exiting.");
            return 1;
        }
        return app.run();
    } catch (const std::exception& e) {
        KJD_LOG_ERROR("Fatal unhandled exception: ", e.what());
        return 1;
    } catch (...) {
        KJD_LOG_ERROR("Fatal unknown exception occurred.");
        return 1;
    }
}
