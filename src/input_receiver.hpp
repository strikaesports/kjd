#pragma once

#include "config.hpp"
#include "math3d.hpp"
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace kjd {

enum class InputEventType {
    LeftClick,
    CalibrationRequested,
    DeviceConnected,
    DeviceDisconnected
};

struct CalibrationData {
    Vec3 forward{0.0f, 0.0f, 1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
};

struct InputEvent {
    InputEventType type;
    std::string deviceId;
    CalibrationData calibration;
    std::chrono::steady_clock::time_point timestamp;
};

struct InputState {
    bool connected{false};
    std::string deviceId;
    float deltaX{0.0f};
    float deltaY{0.0f};
    float deltaZ{0.0f};
    bool leftPressed{false};
    bool orbitPressed{false};
    bool zoomPressed{false};
    bool calibratePressed{false};
    std::chrono::steady_clock::time_point lastPacketTime;
    uint32_t packetSequence{0};
};

struct ReceiverStats {
    uint64_t packetsReceived{0};
    uint64_t packetsAccepted{0};
    uint64_t packetsRejected{0};
    uint64_t missingSequenceCount{0};
    uint64_t timeoutCount{0};
    bool connected{false};
    std::string activeDevice;
    std::string boundAddress;
    int boundPort{0};
};

class InputReceiver {
public:
    InputReceiver();
    ~InputReceiver();

    bool start(const InputConfig& config);
    void stop();
    bool isRunning() const { return m_running.load(); }

    // Fast, thread-safe access for the rendering thread
    InputState getLatestState();
    std::vector<InputEvent> drainEvents();

    ReceiverStats getStats();

    // Direct datagram testing / simulation helper
    bool processDatagramBuffer(const uint8_t* buffer, size_t length, const std::string& remoteAddr);

private:
    void workerLoop();
    void checkTimeout(const std::chrono::steady_clock::time_point& now);
    void pushEventLocked(InputEvent ev);

    InputConfig m_config;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    int m_socketFd{-1};

    // State protected by m_stateMutex
    std::mutex m_stateMutex;
    InputState m_latestState;
    std::deque<InputEvent> m_eventQueue;
    static constexpr size_t MAX_EVENT_QUEUE_SIZE = 64;

    // Sequence tracking and edge detection (worker thread)
    bool m_hasPairedDevice{false};
    std::string m_pairedDeviceId;
    uint32_t m_lastSeq{0};
    bool m_hasSeq{false};

    bool m_prevLeft{false};
    bool m_prevCalibrate{false};

    // Diagnostics stats
    ReceiverStats m_stats;
};

} // namespace kjd
