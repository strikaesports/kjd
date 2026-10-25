# KJD Desktop Environment

**KJD** is an experimental Linux desktop environment built around a **3D workspace** rather than a traditional 2D desktop.

## Phase 0: Black Void + 3D Cube

This repository contains the **Phase 0** prototype:
- Opens a fullscreen graphical surface with a pitch-black void background.
- Renders a 3D cube with perspective, directional and ambient lighting, and smooth rotation.
- Probes and reports active graphics context (API, vendor, renderer, hardware acceleration vs software rasterizer).
- Periodically tracks and logs performance metrics (average FPS, frame times, min/max).
- Runs as a `systemd` user service or standalone session.
- Optimized for deployment on **Ubuntu Server** laptops as well as modern Linux desktops.

---

## 1. Development & Deployment Workflow

The development loop is separated between your development machine and the target Ubuntu Server:

```text
[ Development Machine ]
   Edit source
       │
       ▼
   git commit & push
       │
       ▼
[ Ubuntu Server Target ]
   git pull
       │
       ▼
   ./build.sh
       │
       ▼
   systemctl --user start kjd.service  (or ./scripts/kjd-session.sh)
       │
       ▼
   Inspect logs / 3D cube render
```

---

## 2. Server Setup (For the Server Operators)

On the target **Ubuntu Server** machine:

### A. One-Time Dependency Installation
```bash
sudo ./scripts/install_deps_ubuntu.sh
```
*(Installs `build-essential`, `cmake`, `ninja-build`, `libsdl2-dev`, `libglew-dev`, `libgl1-mesa-dev`, and `cage`)*

### B. Pull and Build
Whenever updates are pushed to Git:
```bash
git pull
./build.sh
```

`./build.sh` will:
1. Validate dependencies and select the fastest build generator (Ninja or Make).
2. Compile KJD natively on the server.
3. Install the binary to `~/.local/bin/kjd`.
4. Install shaders and configuration (`~/.local/share/kjd/shaders/` and `~/.config/kjd/config.toml`).
5. Install and reload the `systemd` user service unit `kjd.service`.
6. Restart the service if it was already running.

---

## 3. Initiating the DE on Ubuntu Server

When initiating the DE on the Ubuntu Server:

### Option 1: Direct Session Initiation (Console / TTY)
From the server TTY console, launch the KJD session wrapper:
```bash
./scripts/kjd-session.sh
```
This automatically leverages `cage` (a lightweight Wayland kiosk compositor) to acquire direct DRM/KMS control and render KJD fullscreen on the laptop display without needing any 2D desktop installed.

### Option 2: Systemd User Service Initiation
If running inside a graphical session managed by systemd:
```bash
# Start KJD
systemctl --user start kjd.service

# Check status
systemctl --user status kjd.service

# Stop KJD
systemctl --user stop kjd.service
```

### Option 3: Autologin on Boot (Boot to 3D Cube)
Refer to the [Ubuntu Server Setup Guide](docs/ubuntu_server_setup.md) for instructions on booting directly into the 3D cube upon machine power-on.

---

## 4. Runtime Controls

- **Esc / Q**: Exit cleanly and restore terminal/session.
- **F11**: Toggle fullscreen.
- **Space**: Pause / Resume cube rotation.

---

## 5. UDP Input Receiver & Mobile App Integration

KJD incorporates a high-performance, nonblocking UDP input receiver on port **45821** (default).

### Network Configuration:
- **Port**: `45821/udp` (configurable via `~/.config/kjd/config.toml` in `[input]`).
- **Firewall**: Allow incoming UDP traffic on the target machine:
  ```bash
  sudo ufw allow 45821/udp
  ```

### Protocol v1 Datagram Schema:
Packets are transmitted as UTF-8 encoded JSON over UDP:

#### 1. Movement & Button Packet (`"type": "input"`):
```json
{
  "version": 1,
  "device_id": "phone-alpha-01",
  "seq": 1042,
  "type": "input",
  "delta": {
    "x": 0.45,
    "y": -0.12,
    "z": 0.0
  },
  "buttons": {
    "left": false,
    "orbit": true,
    "zoom": false,
    "calibrate": false
  },
  "auth": "optional_hmac_sha256_signature"
}
```

#### 2. Calibration Packet (`"type": "calibrate"`):
```json
{
  "version": 1,
  "device_id": "phone-alpha-01",
  "seq": 1043,
  "type": "calibrate",
  "reference": {
    "forward": [0.0, 0.0, 1.0],
    "up": [0.0, 1.0, 0.0],
    "right": [1.0, 0.0, 0.0]
  }
}
```

### Button Semantics:
- **Orbit button held**: Applies X and Y deltas to cube rotation (yaw / pitch).
- **Zoom button held**: Applies Z delta to camera distance (clamped between 1.0 and 20.0).
- **Left click**: Single click event emitted on press edge (down transition).
- **Calibrate button / packet**: Single calibration event emitted on press; resets 3D cube orientation.
- **Silence timeout**: If no packet arrives for > 250 ms, the receiver disconnects the device and clears all button states.

### Testing with Python Test Sender:
From your laptop, workstation, or phone Python environment:
```bash
# Automated smoke test against server
python3 scripts/test_udp_sender.py --host <server_ip> --mode all

# Interactive mode (test orbit, zoom, click, calibrate with keyboard commands)
python3 scripts/test_udp_sender.py --host <server_ip> --mode interactive
```

---

## 6. Local Testing & Command Line Options

To test locally on the development machine (e.g. inside Omarchy/Hyprland/Wayland):

```bash
# Build locally
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Test run windowed
./build/kjd --windowed
```

### Command Line Flags:
```text
  -h, --help            Show help message
  -v, --version         Show version information
  -c, --config <file>   Specify path to custom TOML config
  -w, --windowed        Force windowed mode (overrides config)
  -f, --fullscreen      Force fullscreen mode
      --fps <rate>      Override target FPS (e.g. 60 or 30)
      --no-rotate       Start with a stationary cube
      --debug           Enable verbose debug logging
```

---

## 7. Service Management & Logs

- **Start**: `systemctl --user start kjd.service`
- **Stop**: `systemctl --user stop kjd.service`
- **Restart**: `systemctl --user restart kjd.service`
- **Status**: `systemctl --user status kjd.service`
- **Follow logs**: `journalctl --user -u kjd.service -f`
- **Disable autostart (Recovery)**: `systemctl --user disable --now kjd.service`

---

## 8. Diagnostics & Troubleshooting

To inspect the server environment without making changes:
```bash
./scripts/diagnostics.sh
```

For common issues (e.g. GPU drivers, software rendering fallback, rate limits), see [Troubleshooting Guide](docs/troubleshooting.md).

---

## 9. Project Structure

```text
kjd/
├── CMakeLists.txt             # Build configuration
├── README.md                  # Project documentation
├── build.sh                   # Server operator build and install script
├── config/
│   └── default.toml           # Configurable display, scene, and input parameters
├── shaders/
│   ├── cube.vert              # Vertex shader with MVP transformation
│   └── cube.frag              # Balanced multi-directional lit fragment shader
├── src/
│   ├── main.cpp               # Entry point, CLI argument parsing, environment check
│   ├── app.hpp / app.cpp      # Application lifecycle, input events, render loop
│   ├── renderer.hpp / .cpp    # OpenGL context probe, shaders, 3D cube geometry
│   ├── scene.hpp / .cpp       # 3D transforms, orbit/zoom navigation, stationary state
│   ├── input_receiver.hpp/.cpp# UDP Protocol v1 input receiver worker thread
│   ├── json_helper.hpp        # Lightweight zero-dependency JSON parser
│   ├── crypto_helper.hpp      # Zero-dependency SHA256 & HMAC-SHA256
│   ├── math3d.hpp             # Zero-dependency 3D vector and matrix math
│   ├── config.hpp / .cpp      # Robust TOML configuration parser
│   └── logger.hpp / .cpp      # Timestamped severity logger (INFO, WARN, ERROR)
├── systemd/
│   └── kjd.service            # Systemd user service unit with crash rate limits
├── tests/
│   └── test_input.cpp         # Complete unit tests for UDP receiver & protocol v1
├── scripts/
│   ├── build.sh               # Target native build and user service configuration
│   ├── test_udp_sender.py     # Mobile controller simulator & automated smoke tester
│   ├── diagnostics.sh         # Read-only system, GPU, and service diagnostics
│   ├── install_deps_ubuntu.sh # Ubuntu Server dependency installation
│   └── kjd-session.sh         # DE session launcher for Ubuntu Server / KMSDRM / Wayland
└── docs/
    ├── ubuntu_server_setup.md # Ubuntu Server initiation and boot guide
    └── troubleshooting.md     # Recovery, driver notes, and rollback steps
```
