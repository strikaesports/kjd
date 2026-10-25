#include "logger.hpp"
#include <ctime>

namespace kjd {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

LogLevel Logger::getLevel() const {
    return m_level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_level) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto timer = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm bt{};
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&timer, &bt);
#else
    bt = *std::localtime(&timer);
#endif

    const char* levelStr = "UNKNOWN";
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO:  levelStr = "INFO "; break;
        case LogLevel::WARN:  levelStr = "WARN "; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
    }

    std::ostream& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;

    out << "[" << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count()
        << "] [" << levelStr << "] "
        << message << std::endl;
}

} // namespace kjd
