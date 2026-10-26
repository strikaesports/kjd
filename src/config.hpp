#pragma once

#include <string>

namespace kjd {

struct InputConfig {
    bool enabled{true};
    std::string bind_address{"0.0.0.0"};
    int port{9000};
    int timeout_ms{250};
    int max_datagram_bytes{1200};
    std::string authorized_device_id{""};
    bool authentication_enabled{false};
    std::string shared_secret{""};
    float orbit_sensitivity{1.0f};
    float zoom_sensitivity{1.0f};
    float max_delta_per_packet{10.0f};
};

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
    bool rotate_cube{false};     // default stationary!
    float rotation_speed{35.0f}; // degrees per second
    float fov{60.0f};            // degrees
    float camera_distance{3.5f};

    // Diagnostics / stats
    int stats_interval_sec{5};

    // Input settings
    InputConfig input;

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
