#include "config.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

namespace kjd {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string stripQuotes(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static bool parseBool(const std::string& val, bool defaultVal) {
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") return true;
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") return false;
    return defaultVal;
}

static bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool ConfigManager::parseTomlFile(const std::string& filePath, AppConfig& config) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string currentSection = "";

    while (std::getline(file, line)) {
        // Strip comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Check for section [section_name]
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trim(trimmed.substr(1, trimmed.size() - 2));
            std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(), ::tolower);
            continue;
        }

        // Key = Value
        size_t eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string val = stripQuotes(trim(trimmed.substr(eqPos + 1)));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        try {
            if (currentSection == "window") {
                if (key == "fullscreen") config.fullscreen = parseBool(val, config.fullscreen);
                else if (key == "width") config.window_width = std::stoi(val);
                else if (key == "height") config.window_height = std::stoi(val);
            } else if (currentSection == "render") {
                if (key == "target_fps") config.target_fps = std::stoi(val);
                else if (key == "vsync") config.vsync = parseBool(val, config.vsync);
                else if (key == "scale") config.render_scale = std::stof(val);
            } else if (currentSection == "scene") {
                if (key == "rotate_cube") config.rotate_cube = parseBool(val, config.rotate_cube);
                else if (key == "rotation_speed") config.rotation_speed = std::stof(val);
                else if (key == "fov") config.fov = std::stof(val);
                else if (key == "camera_distance") config.camera_distance = std::stof(val);
            } else if (currentSection == "stats") {
                if (key == "interval_sec") config.stats_interval_sec = std::stoi(val);
            } else {
                // Top level or unqualified keys
                if (key == "fullscreen") config.fullscreen = parseBool(val, config.fullscreen);
                else if (key == "target_fps") config.target_fps = std::stoi(val);
            }
        } catch (const std::exception& e) {
            KJD_LOG_WARN("Failed to parse config key '", key, "': ", e.what());
        }
    }

    config.loaded_path = filePath;
    config.using_defaults = false;
    return true;
}

AppConfig ConfigManager::load(const std::string& explicitPath) {
    AppConfig config;

    // 1. Try explicit path if specified
    if (!explicitPath.empty()) {
        if (parseTomlFile(explicitPath, config)) {
            KJD_LOG_INFO("Loaded configuration from specified file: ", explicitPath);
            return config;
        } else {
            KJD_LOG_WARN("Specified config file not found or unreadable: ", explicitPath, ". Falling back to search paths.");
        }
    }

    // 2. Try $XDG_CONFIG_HOME/kjd/config.toml or ~/.config/kjd/config.toml
    std::string homeConfig = "";
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && *xdgConfigHome) {
        homeConfig = std::string(xdgConfigHome) + "/kjd/config.toml";
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            homeConfig = std::string(home) + "/.config/kjd/config.toml";
        }
    }

    if (!homeConfig.empty() && fileExists(homeConfig)) {
        if (parseTomlFile(homeConfig, config)) {
            KJD_LOG_INFO("Loaded configuration from user directory: ", homeConfig);
            return config;
        }
    }

    // 3. Try ./config/default.toml in current working directory
    if (fileExists("config/default.toml")) {
        if (parseTomlFile("config/default.toml", config)) {
            KJD_LOG_INFO("Loaded configuration from local file: config/default.toml");
            return config;
        }
    }

    // 4. Try system /etc/kjd/config.toml
    if (fileExists("/etc/kjd/config.toml")) {
        if (parseTomlFile("/etc/kjd/config.toml", config)) {
            KJD_LOG_INFO("Loaded configuration from system file: /etc/kjd/config.toml");
            return config;
        }
    }

    // 5. Fallback defaults
    config.using_defaults = true;
    config.loaded_path = "<internal defaults>";
    KJD_LOG_INFO("No configuration file found. Using internal default settings.");
    return config;
}

} // namespace kjd
