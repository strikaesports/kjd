#!/usr/bin/env python3
"""
KJD UDP Input Sender & Test Utility
===================================
Simulates a mobile controller transmitting Protocol v1 UDP packets to KJD.

Usage:
  python3 test_udp_sender.py --host <server_ip> [--port 9000] [--mode orbit|zoom|click|calibrate|interactive|all]

Features:
  - Supports Protocol v1 JSON datagrams:
      {
        "version": 1,
        "device_id": "...",
        "seq": N,
        "type": "input" | "calibrate",
        "delta": {"x": 0.0, "y": 0.0, "z": 0.0},
        "buttons": {"left": bool, "orbit": bool, "zoom": bool, "calibrate": bool},
        "auth": "<hmac_sha256>" (optional)
      }
  - Supports HMAC-SHA256 authentication if a secret is provided.
  - Interactive test modes: Orbiting, Zooming, Clicking, Calibrating, Automated Smoke Test.
"""

import sys
import time
import json
import socket
import hmac
import hashlib
import argparse

class KjdSender:
    def __init__(self, host: str, port: int, device_id: str, secret: str = ""):
        self.host = host
        self.port = port
        self.device_id = device_id
        self.secret = secret
        self.seq = 1
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def _sign(self, seq: int, pkt_type: str) -> str:
        if not self.secret:
            return ""
        # Canonical string: device_id:seq:type
        canonical = f"{self.device_id}:{seq}:{pkt_type}"
        return hmac.new(self.secret.encode('utf-8'), canonical.encode('utf-8'), hashlib.sha256).hexdigest()

    def send_input(self, dx: float = 0.0, dy: float = 0.0, dz: float = 0.0,
                   left: bool = False, orbit: bool = False, zoom: bool = False, calibrate: bool = False):
        seq = self.seq
        self.seq += 1

        payload = {
            "version": 1,
            "device_id": self.device_id,
            "seq": seq,
            "type": "input",
            "delta": {
                "x": round(float(dx), 4),
                "y": round(float(dy), 4),
                "z": round(float(dz), 4)
            },
            "buttons": {
                "left": bool(left),
                "orbit": bool(orbit),
                "zoom": bool(zoom),
                "calibrate": bool(calibrate)
            }
        }

        if self.secret:
            payload["auth"] = self._sign(seq, "input")

        data = json.dumps(payload).encode('utf-8')
        self.sock.sendto(data, (self.host, self.port))

    def send_calibrate(self, forward=(0.0, 0.0, 1.0), up=(0.0, 1.0, 0.0), right=(1.0, 0.0, 0.0)):
        seq = self.seq
        self.seq += 1

        payload = {
            "version": 1,
            "device_id": self.device_id,
            "seq": seq,
            "type": "calibrate",
            "reference": {
                "forward": list(forward),
                "up": list(up),
                "right": list(right)
            }
        }

        if self.secret:
            payload["auth"] = self._sign(seq, "calibrate")

        data = json.dumps(payload).encode('utf-8')
        self.sock.sendto(data, (self.host, self.port))

def run_orbit_test(sender: KjdSender, duration: float = 3.0):
    print(f"\n[ORBIT TEST] Holding orbit button and spinning cube for {duration:.1f}s...")
    start = time.time()
    dt = 1.0 / 60.0 # 60 Hz
    while time.time() - start < duration:
        # Send orbit gesture (dx = 1.5, dy = 0.8)
        sender.send_input(dx=1.5, dy=0.8, dz=0.0, orbit=True)
        time.sleep(dt)
    # Release button
    sender.send_input(dx=0.0, dy=0.0, dz=0.0, orbit=False)
    print("  -> Orbit test complete. Cube should have rotated smoothly.")

def run_zoom_test(sender: KjdSender, duration: float = 3.0):
    print(f"\n[ZOOM TEST] Holding zoom button and moving camera for {duration:.1f}s...")
    start = time.time()
    dt = 1.0 / 60.0
    while time.time() - start < duration:
        # Alternate zoom in and zoom out
        elapsed = time.time() - start
        dz = 0.5 if elapsed < (duration / 2.0) else -0.5
        sender.send_input(dx=0.0, dy=0.0, dz=dz, zoom=True)
        time.sleep(dt)
    # Release
    sender.send_input(dx=0.0, dy=0.0, dz=0.0, zoom=False)
    print("  -> Zoom test complete. Camera distance should have adjusted.")

def run_click_test(sender: KjdSender, count: int = 3):
    print(f"\n[CLICK TEST] Emitting {count} left clicks with edge detection...")
    for i in range(count):
        print(f"  -> Click #{i + 1}: Pressing left button...")
        sender.send_input(left=True)
        time.sleep(0.05)
        sender.send_input(left=True) # Held
        time.sleep(0.05)
        print(f"  -> Click #{i + 1}: Releasing left button...")
        sender.send_input(left=False)
        time.sleep(0.3)
    print("  -> Click test complete.")

def run_calibrate_test(sender: KjdSender):
    print("\n[CALIBRATE TEST] Sending orientation reset calibration packet...")
    sender.send_calibrate(forward=(0.0, 0.0, 1.0), up=(0.0, 1.0, 0.0), right=(1.0, 0.0, 0.0))
    time.sleep(0.1)
    sender.send_input(calibrate=True)
    time.sleep(0.05)
    sender.send_input(calibrate=False)
    print("  -> Calibration sent. Cube orientation should be reset to default.")

def run_timeout_test(sender: KjdSender):
    print("\n[TIMEOUT TEST] Sending 3 packets then going silent for 0.5s...")
    for _ in range(3):
        sender.send_input(dx=0.0, dy=0.0, dz=0.0)
        time.sleep(0.03)
    print("  -> Sent packets. Now pausing for 500 ms (> 250 ms timeout threshold)...")
    time.sleep(0.5)
    print("  -> KJD logs on server should show device disconnected due to silence timeout.")

def run_all_tests(sender: KjdSender):
    print("==================================================")
    print(f" Running Automated KJD UDP Receiver Smoke Test")
    print(f" Target: {sender.host}:{sender.port} | Device: {sender.device_id}")
    print("==================================================")

    run_click_test(sender, count=2)
    time.sleep(0.5)

    run_orbit_test(sender, duration=2.5)
    time.sleep(0.5)

    run_zoom_test(sender, duration=2.0)
    time.sleep(0.5)

    run_calibrate_test(sender)
    time.sleep(0.5)

    run_timeout_test(sender)

    print("\n==================================================")
    print(" All automated test sequences dispatched!")
    print(" Check KJD service logs on the server:")
    print("   journalctl --user -u kjd.service -f")
    print("==================================================")

def run_interactive(sender: KjdSender):
    print("==================================================")
    print(f" Interactive KJD UDP Controller")
    print(f" Target: {sender.host}:{sender.port}")
    print("==================================================")
    print(" Commands:")
    print("   o [seconds] : Orbit cube for N seconds (default 2)")
    print("   z [seconds] : Zoom camera for N seconds (default 2)")
    print("   c           : Send Left Click")
    print("   r           : Send Calibration (Reset Orientation)")
    print("   t           : Test 250ms silence timeout")
    print("   q           : Quit")
    print("--------------------------------------------------")

    while True:
        try:
            cmd = input("kjd-test> ").strip().split()
            if not cmd:
                continue
            action = cmd[0].lower()
            if action in ('q', 'exit', 'quit'):
                break
            elif action == 'o':
                dur = float(cmd[1]) if len(cmd) > 1 else 2.0
                run_orbit_test(sender, duration=dur)
            elif action == 'z':
                dur = float(cmd[1]) if len(cmd) > 1 else 2.0
                run_zoom_test(sender, duration=dur)
            elif action == 'c':
                run_click_test(sender, count=1)
            elif action == 'r':
                run_calibrate_test(sender)
            elif action == 't':
                run_timeout_test(sender)
            elif action == 'all':
                run_all_tests(sender)
            else:
                print(f"Unknown command: '{action}'. Available: o, z, c, r, t, all, q")
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            break
        except Exception as e:
            print(f"Error: {e}")

def main():
    parser = argparse.ArgumentParser(description="KJD UDP Mouse/Gyro Input Test Sender")
    parser.add_argument("--host", default="127.0.0.1", help="Target host/IP running KJD (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=9000, help="Target UDP port (default: 9000)")
    parser.add_argument("--device-id", default="kjd-test-sender-01", help="Client device ID string")
    parser.add_argument("--secret", default="", help="Optional shared HMAC secret")
    parser.add_argument("--mode", choices=["orbit", "zoom", "click", "calibrate", "timeout", "all", "interactive"],
                        default="all", help="Test mode to run (default: all)")
    args = parser.parse_args()

    sender = KjdSender(host=args.host, port=args.port, device_id=args.device_id, secret=args.secret)

    if args.mode == "orbit":
        run_orbit_test(sender)
    elif args.mode == "zoom":
        run_zoom_test(sender)
    elif args.mode == "click":
        run_click_test(sender)
    elif args.mode == "calibrate":
        run_calibrate_test(sender)
    elif args.mode == "timeout":
        run_timeout_test(sender)
    elif args.mode == "interactive":
        run_interactive(sender)
    else:
        run_all_tests(sender)

if __name__ == "__main__":
    main()
