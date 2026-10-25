#include "input_receiver.hpp"
#include "json_helper.hpp"
#include "crypto_helper.hpp"
#include "logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <algorithm>
#include <cmath>

namespace kjd {

InputReceiver::InputReceiver() = default;

InputReceiver::~InputReceiver() {
    stop();
}

bool InputReceiver::start(const InputConfig& config) {
    if (m_running.load()) {
        KJD_LOG_WARN("InputReceiver is already running.");
        return true;
    }

    m_config = config;
    if (!m_config.enabled) {
        KJD_LOG_INFO("InputReceiver is disabled in configuration.");
        return true;
    }

    KJD_LOG_INFO("Initializing UDP Input Receiver on ", m_config.bind_address, ":", m_config.port, "...");

    // Create UDP socket
    m_socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socketFd < 0) {
        KJD_LOG_ERROR("Failed to create UDP socket: ", strerror(errno));
        return false;
    }

    // Set non-blocking or allow quick timeouts
    int flags = fcntl(m_socketFd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(m_socketFd, F_SETFL, flags | O_NONBLOCK);
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(static_cast<uint16_t>(m_config.port));

    if (m_config.bind_address == "0.0.0.0" || m_config.bind_address.empty()) {
        bindAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, m_config.bind_address.c_str(), &bindAddr.sin_addr) <= 0) {
            KJD_LOG_ERROR("Invalid bind address specified: ", m_config.bind_address);
            close(m_socketFd);
            m_socketFd = -1;
            return false;
        }
    }

    if (bind(m_socketFd, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
        KJD_LOG_ERROR("Failed to bind UDP socket to ", m_config.bind_address, ":", m_config.port, ": ", strerror(errno));
        KJD_LOG_ERROR("Verify that no other service is using port ", m_config.port, " and that firewall allows incoming UDP traffic.");
        close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    // Query actual bound port if 0 was passed
    socklen_t addrLen = sizeof(bindAddr);
    getsockname(m_socketFd, reinterpret_cast<sockaddr*>(&bindAddr), &addrLen);
    m_stats.boundPort = ntohs(bindAddr.sin_port);
    m_stats.boundAddress = m_config.bind_address;

    KJD_LOG_INFO("UDP Input Receiver successfully bound to ", m_stats.boundAddress, ":", m_stats.boundPort);
    KJD_LOG_INFO("  Timeout limit: ", m_config.timeout_ms, " ms");
    KJD_LOG_INFO("  Max datagram:  ", m_config.max_datagram_bytes, " bytes");
    if (!m_config.authorized_device_id.empty()) {
        KJD_LOG_INFO("  Authorized ID: '", m_config.authorized_device_id, "'");
    } else {
        KJD_LOG_INFO("  Authorized ID: <any / first-connect pairing>");
    }
    KJD_LOG_INFO("  Authentication: ", (m_config.authentication_enabled ? "Enabled (HMAC-SHA256)" : "Disabled (Development Mode)"));

    m_running = true;
    m_workerThread = std::thread(&InputReceiver::workerLoop, this);
    return true;
}

void InputReceiver::stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    KJD_LOG_INFO("Stopping UDP Input Receiver...");

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (m_socketFd >= 0) {
        close(m_socketFd);
        m_socketFd = -1;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_latestState.connected) {
        m_latestState.connected = false;
        pushEventLocked({InputEventType::DeviceDisconnected, m_pairedDeviceId, {}, std::chrono::steady_clock::now()});
    }
    m_hasPairedDevice = false;
    m_pairedDeviceId.clear();

    KJD_LOG_INFO("UDP Input Receiver stopped cleanly.");
}

void InputReceiver::pushEventLocked(InputEvent ev) {
    if (m_eventQueue.size() >= MAX_EVENT_QUEUE_SIZE) {
        m_eventQueue.pop_front(); // Bound queue, drop oldest
    }
    m_eventQueue.push_back(std::move(ev));
}

void InputReceiver::checkTimeout(const std::chrono::steady_clock::time_point& now) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (!m_latestState.connected) return;

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_latestState.lastPacketTime).count();
    if (elapsedMs > m_config.timeout_ms) {
        KJD_LOG_INFO("Input device '", m_pairedDeviceId, "' disconnected (timeout after ", elapsedMs, " ms without packet).");

        m_latestState.connected = false;
        m_latestState.deltaX = 0.0f;
        m_latestState.deltaY = 0.0f;
        m_latestState.deltaZ = 0.0f;
        m_latestState.leftPressed = false;
        m_latestState.orbitPressed = false;
        m_latestState.zoomPressed = false;
        m_latestState.calibratePressed = false;

        pushEventLocked({InputEventType::DeviceDisconnected, m_pairedDeviceId, {}, now});

        m_stats.timeoutCount++;
        m_stats.connected = false;
        m_stats.activeDevice.clear();

        m_hasPairedDevice = false;
        m_pairedDeviceId.clear();
        m_hasSeq = false;
        m_prevLeft = false;
        m_prevCalibrate = false;
    }
}

bool InputReceiver::processDatagramBuffer(const uint8_t* buffer, size_t length, const std::string& remoteAddr) {
    m_stats.packetsReceived++;

    if (length > static_cast<size_t>(m_config.max_datagram_bytes)) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected datagram from ", remoteAddr, ": size ", length, " exceeds max ", m_config.max_datagram_bytes);
        return false;
    }

    std::string_view jsonView(reinterpret_cast<const char*>(buffer), length);
    JsonValue root;
    std::string parseErr;
    if (!SimpleJson::parse(jsonView, root, parseErr)) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected datagram from ", remoteAddr, ": malformed JSON (", parseErr, ")");
        return false;
    }

    if (!root.isObject()) {
        m_stats.packetsRejected++;
        return false;
    }

    // 1. Version check
    const JsonValue* verVal = root.get("version");
    if (!verVal || !verVal->isNumber() || verVal->asInt64() != 1) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: unsupported version (expected 1)");
        return false;
    }

    // 2. Device ID check
    const JsonValue* devVal = root.get("device_id");
    if (!devVal || !devVal->isString() || devVal->asString().empty()) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: missing or empty device_id");
        return false;
    }
    const std::string& deviceId = devVal->asString();

    if (!m_config.authorized_device_id.empty() && deviceId != m_config.authorized_device_id) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: device_id '", deviceId, "' does not match authorized '", m_config.authorized_device_id, "'");
        return false;
    }

    // Pairing constraint (one device at a time)
    if (m_hasPairedDevice && deviceId != m_pairedDeviceId) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: device_id '", deviceId, "' rejected; currently paired with '", m_pairedDeviceId, "'");
        return false;
    }

    // 3. Sequence number
    const JsonValue* seqVal = root.get("seq");
    if (!seqVal || !seqVal->isNumber() || seqVal->asInt64() < 0) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: missing or invalid sequence number");
        return false;
    }
    uint32_t seq = static_cast<uint32_t>(seqVal->asInt64());

    // 4. Type check
    const JsonValue* typeVal = root.get("type");
    if (!typeVal || !typeVal->isString()) {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: missing or invalid message type");
        return false;
    }
    const std::string& msgType = typeVal->asString();

    // 5. Authentication check if enabled
    if (m_config.authentication_enabled) {
        const JsonValue* authVal = root.get("auth");
        if (!authVal || !authVal->isString()) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected packet: authentication enabled but 'auth' token missing");
            return false;
        }

        std::string canonical = deviceId + ":" + std::to_string(seq) + ":" + msgType;
        std::string expectedAuth = Sha256::hmac(m_config.shared_secret, canonical);
        if (!Sha256::constantTimeCompare(authVal->asString(), expectedAuth)) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected packet: HMAC authentication verification failed");
            return false;
        }
    }

    // 6. Wraparound-safe sequence check
    if (m_hasSeq) {
        int32_t diff = static_cast<int32_t>(seq - m_lastSeq);
        if (diff <= 0) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected packet: duplicate or out-of-order sequence (seq=", seq, ", last=", m_lastSeq, ")");
            return false;
        }
        if (diff > 1) {
            m_stats.missingSequenceCount += static_cast<uint64_t>(diff - 1);
        }
    }
    m_lastSeq = seq;
    m_hasSeq = true;

    auto now = std::chrono::steady_clock::now();

    // 7. Process based on packet type
    if (msgType == "input") {
        const JsonValue* deltaObj = root.get("delta");
        const JsonValue* btnObj = root.get("buttons");
        if (!deltaObj || !deltaObj->isObject() || !btnObj || !btnObj->isObject()) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected input packet: missing 'delta' or 'buttons' object");
            return false;
        }

        const JsonValue* xVal = deltaObj->get("x");
        const JsonValue* yVal = deltaObj->get("y");
        const JsonValue* zVal = deltaObj->get("z");
        if (!xVal || !xVal->isNumber() || !yVal || !yVal->isNumber() || !zVal || !zVal->isNumber()) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected input packet: delta coordinates must be finite numbers");
            return false;
        }

        const JsonValue* leftVal = btnObj->get("left");
        const JsonValue* orbitVal = btnObj->get("orbit");
        const JsonValue* zoomVal = btnObj->get("zoom");
        const JsonValue* calibVal = btnObj->get("calibrate");
        if (!leftVal || !leftVal->isBool() || !orbitVal || !orbitVal->isBool() ||
            !zoomVal || !zoomVal->isBool() || !calibVal || !calibVal->isBool()) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected input packet: button fields must be booleans");
            return false;
        }

        float dx = static_cast<float>(xVal->asDouble());
        float dy = static_cast<float>(yVal->asDouble());
        float dz = static_cast<float>(zVal->asDouble());

        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected input packet: delta contains NaN or Infinity");
            return false;
        }

        // Clamp deltas to configured safe threshold
        float maxDelta = m_config.max_delta_per_packet;
        dx = std::clamp(dx, -maxDelta, maxDelta);
        dy = std::clamp(dy, -maxDelta, maxDelta);
        dz = std::clamp(dz, -maxDelta, maxDelta);

        bool left = leftVal->asBool();
        bool orbit = orbitVal->asBool();
        bool zoom = zoomVal->asBool();
        bool calibrate = calibVal->asBool();

        std::lock_guard<std::mutex> lock(m_stateMutex);

        // Connection transition
        if (!m_latestState.connected) {
            m_latestState.connected = true;
            m_hasPairedDevice = true;
            m_pairedDeviceId = deviceId;
            m_latestState.deviceId = deviceId;

            m_stats.connected = true;
            m_stats.activeDevice = deviceId;

            KJD_LOG_INFO("Input device '", deviceId, "' connected from ", remoteAddr, ".");
            pushEventLocked({InputEventType::DeviceConnected, deviceId, {}, now});
        }

        // Button edge transitions
        if (!m_prevLeft && left) {
            pushEventLocked({InputEventType::LeftClick, deviceId, {}, now});
            KJD_LOG_DEBUG("Left click event detected from device '", deviceId, "'");
        }
        if (!m_prevCalibrate && calibrate) {
            pushEventLocked({InputEventType::CalibrationRequested, deviceId, {}, now});
            KJD_LOG_INFO("Calibration triggered via button from device '", deviceId, "'");
        }
        m_prevLeft = left;
        m_prevCalibrate = calibrate;

        // Apply movement semantics
        if (orbit && !zoom) {
            m_latestState.deltaX = dx;
            m_latestState.deltaY = dy;
            m_latestState.deltaZ = 0.0f;
        } else if (zoom && !orbit) {
            m_latestState.deltaX = 0.0f;
            m_latestState.deltaY = 0.0f;
            m_latestState.deltaZ = dz;
        } else {
            // Neither relevant button held, or both held conflictingly: do not apply movement
            m_latestState.deltaX = 0.0f;
            m_latestState.deltaY = 0.0f;
            m_latestState.deltaZ = 0.0f;
        }

        m_latestState.leftPressed = left;
        m_latestState.orbitPressed = orbit;
        m_latestState.zoomPressed = zoom;
        m_latestState.calibratePressed = calibrate;
        m_latestState.lastPacketTime = now;
        m_latestState.packetSequence = seq;

        m_stats.packetsAccepted++;
        return true;

    } else if (msgType == "calibrate") {
        const JsonValue* refObj = root.get("reference");
        if (!refObj || !refObj->isObject()) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected calibrate packet: missing 'reference' object");
            return false;
        }

        const JsonValue* fArr = refObj->get("forward");
        const JsonValue* uArr = refObj->get("up");
        const JsonValue* rArr = refObj->get("right");

        if (!fArr || !fArr->isArray() || fArr->arrVal.size() != 3 ||
            !uArr || !uArr->isArray() || uArr->arrVal.size() != 3 ||
            !rArr || !rArr->isArray() || rArr->arrVal.size() != 3) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected calibrate packet: reference vectors must be 3-element arrays");
            return false;
        }

        auto readVec = [](const JsonValue& arr, Vec3& v) -> bool {
            if (!arr.arrVal[0].isNumber() || !arr.arrVal[1].isNumber() || !arr.arrVal[2].isNumber()) return false;
            v.x = static_cast<float>(arr.arrVal[0].asDouble());
            v.y = static_cast<float>(arr.arrVal[1].asDouble());
            v.z = static_cast<float>(arr.arrVal[2].asDouble());
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };

        Vec3 fwd, up, right;
        if (!readVec(*fArr, fwd) || !readVec(*uArr, up) || !readVec(*rArr, right)) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected calibrate packet: non-finite or invalid vector values");
            return false;
        }

        // Validate vector lengths (must not be near-zero)
        if (fwd.length() < 1e-3f || up.length() < 1e-3f || right.length() < 1e-3f) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected calibrate packet: near-zero basis vector");
            return false;
        }

        Vec3 normF = fwd.normalized();
        Vec3 normU = up.normalized();
        Vec3 normR = right.normalized();

        // Check approximate orthogonality (tolerance 0.25)
        float dotFU = std::abs(Vec3::dot(normF, normU));
        float dotFR = std::abs(Vec3::dot(normF, normR));
        float dotUR = std::abs(Vec3::dot(normU, normR));

        if (dotFU > 0.25f || dotFR > 0.25f || dotUR > 0.25f) {
            m_stats.packetsRejected++;
            KJD_LOG_DEBUG("Rejected calibrate packet: basis vectors not approximately orthogonal (dotFU=", dotFU,
                          ", dotFR=", dotFR, ", dotUR=", dotUR, ")");
            return false;
        }

        CalibrationData calib{normF, normU, normR};

        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_latestState.connected) {
            m_latestState.connected = true;
            m_hasPairedDevice = true;
            m_pairedDeviceId = deviceId;
            m_latestState.deviceId = deviceId;
            m_stats.connected = true;
            m_stats.activeDevice = deviceId;
            pushEventLocked({InputEventType::DeviceConnected, deviceId, {}, now});
        }
        m_latestState.lastPacketTime = now;
        m_latestState.packetSequence = seq;

        pushEventLocked({InputEventType::CalibrationRequested, deviceId, calib, now});
        m_stats.packetsAccepted++;

        KJD_LOG_INFO("Accepted calibration packet from '", deviceId,
                     "' (F=[", normF.x, ",", normF.y, ",", normF.z,
                     "], U=[", normU.x, ",", normU.y, ",", normU.z,
                     "], R=[", normR.x, ",", normR.y, ",", normR.z, "])");
        return true;

    } else {
        m_stats.packetsRejected++;
        KJD_LOG_DEBUG("Rejected packet: unknown message type '", msgType, "'");
        return false;
    }
}

void InputReceiver::workerLoop() {
    uint8_t buffer[2048];

    while (m_running.load()) {
        struct pollfd pfd{};
        pfd.fd = m_socketFd;
        pfd.events = POLLIN;

        int pollRet = poll(&pfd, 1, 25); // 25 ms timeout for responsive shutdown and timeout checks
        auto now = std::chrono::steady_clock::now();

        if (pollRet > 0 && (pfd.revents & POLLIN)) {
            sockaddr_in srcAddr{};
            socklen_t srcLen = sizeof(srcAddr);

            ssize_t bytesRead = recvfrom(m_socketFd, buffer, sizeof(buffer), 0,
                                         reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
            if (bytesRead > 0) {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &srcAddr.sin_addr, ipStr, sizeof(ipStr));
                std::string remoteAddr = std::string(ipStr) + ":" + std::to_string(ntohs(srcAddr.sin_port));

                processDatagramBuffer(buffer, static_cast<size_t>(bytesRead), remoteAddr);
            }
        }

        checkTimeout(now);
    }
}

InputState InputReceiver::getLatestState() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    InputState state = m_latestState;
    // Consume deltas so the same movement sample is not re-applied across multiple frames
    m_latestState.deltaX = 0.0f;
    m_latestState.deltaY = 0.0f;
    m_latestState.deltaZ = 0.0f;
    return state;
}

std::vector<InputEvent> InputReceiver::drainEvents() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::vector<InputEvent> events(m_eventQueue.begin(), m_eventQueue.end());
    m_eventQueue.clear();
    return events;
}

ReceiverStats InputReceiver::getStats() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    ReceiverStats s = m_stats;
    s.connected = m_latestState.connected;
    s.activeDevice = m_pairedDeviceId;
    return s;
}

} // namespace kjd
