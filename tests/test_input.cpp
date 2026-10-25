#include "input_receiver.hpp"
#include "crypto_helper.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace kjd;

void runTests() {
    std::cout << "==========================================" << std::endl;
    std::cout << " Running KJD UDP Input Receiver Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    InputConfig cfg;
    cfg.enabled = true;
    cfg.port = 0; // Don't bind in offline tests
    cfg.timeout_ms = 100;
    cfg.max_datagram_bytes = 500;
    cfg.max_delta_per_packet = 5.0f;

    InputReceiver rx;

    // 1. Valid input packet
    {
        std::string pkt = R"({
            "version": 1,
            "device_id": "test-dev-01",
            "seq": 1,
            "type": "input",
            "delta": {"x": 1.2, "y": -0.8, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(pkt.data()), pkt.size(), "127.0.0.1:5000"));
        InputState s = rx.getLatestState();
        assert(s.connected);
        assert(s.deviceId == "test-dev-01");
        assert(s.orbitPressed == true);
        assert(s.deltaX == 1.2f);
        assert(s.deltaY == -0.8f);
        assert(s.deltaZ == 0.0f);
        std::cout << "[PASS] 1. Valid input packet parsed and state updated." << std::endl;
    }

    // 2. Missing & incorrectly typed fields
    {
        std::string missingDelta = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 2, "type": "input",
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(missingDelta.data()), missingDelta.size(), "127.0.0.1:5000"));

        std::string badButtonType = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 3, "type": "input",
            "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": "not-a-bool", "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(badButtonType.data()), badButtonType.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 2. Missing and incorrectly typed fields rejected." << std::endl;
    }

    // 3. Malformed and oversized datagrams
    {
        std::string malformed = "{ this is not json }";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size(), "127.0.0.1:5000"));

        std::string oversized(600, 'x');
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(oversized.data()), oversized.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 3. Malformed and oversized datagrams rejected." << std::endl;
    }

    // 4. Unsupported protocol version
    {
        std::string badVer = R"({
            "version": 2, "device_id": "test-dev-01", "seq": 4, "type": "input",
            "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": false, "orbit": false, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(badVer.data()), badVer.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 4. Unsupported protocol versions rejected." << std::endl;
    }

    // 5. Extreme deltas clamped, and NaN/Inf rejected
    {
        std::string extremePkt = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 5, "type": "input",
            "delta": {"x": 500.0, "y": -200.0, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(extremePkt.data()), extremePkt.size(), "127.0.0.1:5000"));
        InputState s = rx.getLatestState();
        assert(s.deltaX == 5.0f);  // Clamped to max_delta_per_packet
        assert(s.deltaY == -5.0f); // Clamped
        std::cout << "[PASS] 5. Extreme movement deltas properly clamped." << std::endl;
    }

    // 6. Duplicate and out-of-order sequence rejection
    {
        // Resend seq 5
        std::string dupPkt = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 5, "type": "input",
            "delta": {"x": 1.0, "y": 1.0, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(dupPkt.data()), dupPkt.size(), "127.0.0.1:5000"));

        // Send seq 3 (older)
        std::string oldPkt = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 3, "type": "input",
            "delta": {"x": 1.0, "y": 1.0, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(oldPkt.data()), oldPkt.size(), "127.0.0.1:5000"));

        // Jump sequence from 5 to 10 (skipping 4 packets)
        std::string jumpPkt = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 10, "type": "input",
            "delta": {"x": 1.0, "y": 1.0, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(jumpPkt.data()), jumpPkt.size(), "127.0.0.1:5000"));
        ReceiverStats st = rx.getStats();
        assert(st.missingSequenceCount == 4);
        std::cout << "[PASS] 6. Stale/duplicate sequence numbers rejected, missing sequences counted." << std::endl;
    }

    // 7. Left click edge detection
    {
        rx.drainEvents(); // Clear any existing events

        // Press left
        std::string pressLeft = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 11, "type": "input",
            "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": true, "orbit": false, "zoom": false, "calibrate": false}
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(pressLeft.data()), pressLeft.size(), "127.0.0.1:5000"));
        auto events = rx.drainEvents();
        bool foundClick = false;
        for (const auto& ev : events) {
            if (ev.type == InputEventType::LeftClick) foundClick = true;
        }
        assert(foundClick);

        // Keep left held (seq 12) -> should NOT emit another click event
        std::string holdLeft = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 12, "type": "input",
            "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": true, "orbit": false, "zoom": false, "calibrate": false}
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(holdLeft.data()), holdLeft.size(), "127.0.0.1:5000"));
        auto events2 = rx.drainEvents();
        bool repeatClick = false;
        for (const auto& ev : events2) {
            if (ev.type == InputEventType::LeftClick) repeatClick = true;
        }
        assert(!repeatClick);
        std::cout << "[PASS] 7. Left-click edge detection verified (single event per press)." << std::endl;
    }

    // 8. Calibration validation (good and bad)
    {
        // Valid calibration
        std::string goodCalib = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 13, "type": "calibrate",
            "reference": {
                "forward": [0.0, 0.0, 1.0],
                "up": [0.0, 1.0, 0.0],
                "right": [1.0, 0.0, 0.0]
            }
        })";
        assert(rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(goodCalib.data()), goodCalib.size(), "127.0.0.1:5000"));
        auto evs = rx.drainEvents();
        bool foundCalib = false;
        for (const auto& ev : evs) {
            if (ev.type == InputEventType::CalibrationRequested) foundCalib = true;
        }
        assert(foundCalib);

        // Invalid calibration: parallel / non-orthogonal vectors
        std::string badCalib = R"({
            "version": 1, "device_id": "test-dev-01", "seq": 14, "type": "calibrate",
            "reference": {
                "forward": [1.0, 0.0, 0.0],
                "up": [1.0, 0.0, 0.0],
                "right": [1.0, 0.0, 0.0]
            }
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(badCalib.data()), badCalib.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 8. Calibration vector orthogonality validation passed." << std::endl;
    }

    // 9. Unauthorized device rejection
    {
        // Currently paired with "test-dev-01", so packet from "rogue-dev" should be rejected
        std::string rogue = R"({
            "version": 1, "device_id": "rogue-dev", "seq": 1, "type": "input",
            "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": false, "orbit": false, "zoom": false, "calibrate": false}
        })";
        assert(!rx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(rogue.data()), rogue.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 9. Unauthorized device rejected during active pairing." << std::endl;
    }

    // 10. HMAC authentication
    {
        InputConfig authCfg;
        authCfg.enabled = true;
        authCfg.authentication_enabled = true;
        authCfg.shared_secret = "kjd_secret_token_1234";

        InputReceiver authRx;

        // Compute valid HMAC: hmac("kjd_secret_token_1234", "mobile-01:1:input")
        std::string validCanonical = "mobile-01:1:input";
        std::string validAuth = Sha256::hmac(authCfg.shared_secret, validCanonical);

        std::string authPacket = "{\"version\": 1, \"device_id\": \"mobile-01\", \"seq\": 1, \"type\": \"input\", \"auth\": \"" + validAuth + "\", \"delta\": {\"x\": 0.0, \"y\": 0.0, \"z\": 0.0}, \"buttons\": {\"left\": false, \"orbit\": false, \"zoom\": false, \"calibrate\": false}}";
        assert(authRx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(authPacket.data()), authPacket.size(), "127.0.0.1:5000"));

        // Send packet with forged/tampered auth token
        std::string tamperedPacket = "{\"version\": 1, \"device_id\": \"mobile-01\", \"seq\": 2, \"type\": \"input\", \"auth\": \"wrong_hmac_signature_value\", \"delta\": {\"x\": 0.0, \"y\": 0.0, \"z\": 0.0}, \"buttons\": {\"left\": false, \"orbit\": false, \"zoom\": false, \"calibrate\": false}}";
        assert(!authRx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(tamperedPacket.data()), tamperedPacket.size(), "127.0.0.1:5000"));
        std::cout << "[PASS] 10. HMAC-SHA256 authentication correctly validated and forged tokens rejected." << std::endl;
    }

    // 11. Network startup, shutdown, and timeout reset test
    {
        InputConfig netCfg;
        netCfg.enabled = true;
        netCfg.bind_address = "127.0.0.1";
        netCfg.port = 45899; // Test port
        netCfg.timeout_ms = 150;

        InputReceiver netRx;
        assert(netRx.start(netCfg));
        assert(netRx.isRunning());

        // Send a valid packet via buffer directly
        std::string pkt = R"({
            "version": 1, "device_id": "mobile-net", "seq": 1, "type": "input",
            "delta": {"x": 1.0, "y": 0.0, "z": 0.0},
            "buttons": {"left": false, "orbit": true, "zoom": false, "calibrate": false}
        })";
        assert(netRx.processDatagramBuffer(reinterpret_cast<const uint8_t*>(pkt.data()), pkt.size(), "127.0.0.1:5000"));
        assert(netRx.getLatestState().connected);

        // Wait past timeout (200 ms > 150 ms timeout)
        std::this_thread::sleep_for(std::chrono::milliseconds(220));

        // State should now be disconnected with all buttons cleared
        InputState s = netRx.getLatestState();
        assert(!s.connected);
        assert(!s.orbitPressed);
        assert(!s.leftPressed);

        auto evs = netRx.drainEvents();
        bool foundDisconnect = false;
        for (const auto& ev : evs) {
            if (ev.type == InputEventType::DeviceDisconnected) foundDisconnect = true;
        }
        assert(foundDisconnect);

        // Test clean deterministic shutdown while worker is listening
        netRx.stop();
        assert(!netRx.isRunning());
        std::cout << "[PASS] 11. Network bind, timeout state reset, and clean thread shutdown verified." << std::endl;
    }

    std::cout << "==========================================" << std::endl;
    std::cout << " ALL UNIT TESTS PASSED SUCCESSFULLY!     " << std::endl;
    std::cout << "==========================================" << std::endl;
}

int main() {
    try {
        runTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
