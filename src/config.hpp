#pragma once

#include <string>

namespace kjd {

struct AppConfig {
    // Window settings
    bool fullscreen{true};
    int window_width{1920};
    int window_height{1080};

    // Render settings
    int target_fps{60};
    bool vsync{true};
    float render_scale{1.0f};

    // Scene settings
    bool rotate_cube{true};
    float rotation_speed{35.0f}; // degrees per second
    float fov{60.0f};            // degrees
    float camera_distance{3.5f};

    // Diagnostics / stats
    int stats_interval_sec{5};

    // Metadata
    std::string loaded_path;
    bool using_defaults{true};
};

class ConfigManager {
public:
    static AppConfig load(const std::string& explicitPath = "");

private:
    static bool parseTomlFile(const std::string& filePath, AppConfig& config);
};

} // namespace kjd
