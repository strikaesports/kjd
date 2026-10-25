#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace kjd {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    void log(LogLevel level, const std::string& message);

    template<typename... Args>
    void logFormatted(LogLevel level, Args&&... args) {
        if (level < m_level) return;
        std::ostringstream ss;
        (ss << ... << std::forward<Args>(args));
        log(level, ss.str());
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel m_level{LogLevel::INFO};
    std::mutex m_mutex;
};

#define KJD_LOG_DEBUG(...) ::kjd::Logger::instance().logFormatted(::kjd::LogLevel::DEBUG, __VA_ARGS__)
#define KJD_LOG_INFO(...)  ::kjd::Logger::instance().logFormatted(::kjd::LogLevel::INFO,  __VA_ARGS__)
#define KJD_LOG_WARN(...)  ::kjd::Logger::instance().logFormatted(::kjd::LogLevel::WARN,  __VA_ARGS__)
#define KJD_LOG_ERROR(...) ::kjd::Logger::instance().logFormatted(::kjd::LogLevel::ERROR, __VA_ARGS__)

} // namespace kjd
