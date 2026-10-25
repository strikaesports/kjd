# KJD Desktop Environment — Ubuntu Server Setup Guide

This guide explains how to install, build, and initiate the **KJD Desktop Environment (Phase 0: Black Void + 3D Cube)** on **Ubuntu Server** (22.04 LTS or 24.04 LTS).

---

## 1. Overview

Ubuntu Server is a lean, headless operating system without standard desktop bloat (GNOME, desktop panels, notification daemons). 

KJD operates as the graphical shell itself. In Phase 0, when the DE is initiated, it opens a fullscreen graphical surface showing a pitch-black void with a rendered 3D cube with perspective, lighting, and optional rotation.

There are two primary ways to initiate KJD on Ubuntu Server:
1. **Direct DRM/KMS Kiosk Mode (Recommended for Server)**: Using `cage`, a lightweight Wayland compositor that directly drives the display GPU without requiring X11 or a heavy desktop.
2. **Systemd User Service**: Running inside an existing Wayland or X11 graphical session managed by systemd.

---

## 2. Prerequisites & Package Installation

Run the dependency installer on your Ubuntu Server:

```bash
sudo ./scripts/install_deps_ubuntu.sh
```

Or manually install packages via `apt`:

```bash
sudo apt-get update -y
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libsdl2-dev \
    libglew-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    mesa-utils \
    pciutils \
    cage
```

---

## 3. Building and Deploying

Deploy KJD without needing root privileges:

```bash
./scripts/deploy.sh
```

This will:
1. Compile KJD using CMake and Ninja in Release mode.
2. Install the binary to `~/.local/bin/kjd`.
3. Install default configuration to `~/.config/kjd/config.toml`.
4. Install shaders to `~/.local/share/kjd/shaders/`.
5. Install and register the systemd user service `kjd.service`.

Ensure `~/.local/bin` is in your `PATH` by adding it to `~/.bashrc`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

---

## 4. Initiating the DE on Ubuntu Server

### Method A: Direct Session Initiation (`kjd-session.sh`)

When logged into a TTY console on Ubuntu Server, initiate the DE directly:

```bash
./scripts/kjd-session.sh
```

If `cage` is installed, this directly initializes DRM/KMS on the graphics card and launches KJD in true fullscreen mode. The screen turns into a pitch-black void with the rotating 3D cube.

To stop the session and return to the TTY, press `Esc` or `Q`.

---

### Method B: Systemd User Service Initiation

If your Ubuntu Server runs a display manager (such as `greetd` or `lightdm`) or starts a Wayland session at login:

1. Import your session environment:
   ```bash
   systemctl --user import-environment WAYLAND_DISPLAY DISPLAY XDG_RUNTIME_DIR
   ```

2. Start the KJD service:
   ```bash
   systemctl --user start kjd.service
   ```

3. Check service status:
   ```bash
   systemctl --user status kjd.service
   ```

4. Stop KJD:
   ```bash
   systemctl --user stop kjd.service
   ```

---

### Method C: Autologin on Boot (Console to KJD)

To have your Ubuntu Server laptop boot directly into KJD without manual commands:

1. Configure TTY1 autologin using `systemctl edit getty@tty1`:
   ```ini
   [Service]
   ExecStart=
   ExecStart=-/sbin/agetty --autologin <your-username> --noclear %I $TERM
   ```

2. Add the following to the bottom of `~/.bash_profile` or `~/.profile`:
   ```bash
   if [ -z "$DISPLAY" ] && [ "$XDG_VTNR" = 1 ]; then
       exec ~/.local/bin/kjd-session.sh
   fi
   ```

Upon powering on, the server boots straight into the KJD 3D void and cube.

---

## 5. Runtime Controls

- **Esc / Q**: Exit cleanly and return to terminal or session manager.
- **F11**: Toggle fullscreen mode.
- **Space**: Pause / Resume cube rotation.

---

## 6. Graphics Driver & Performance Diagnostics

KJD probes the active rendering context at launch and reports whether it is running with hardware acceleration or Mesa software rasterization (`llvmpipe`).

Run the diagnostics script at any time:

```bash
./scripts/diagnostics.sh
```

### Hardware Acceleration Notes:
- **Intel Integrated Graphics (e.g. Celeron)**: Mesa's `iris` or `i965` driver is loaded automatically.
- **NVIDIA GPU**: Install proprietary drivers (`sudo ubuntu-drivers install` or `sudo apt install nvidia-driver-535`) if dedicated acceleration is desired.
- **Software Rendering Fallback**: If no GPU driver is installed, KJD automatically detects Mesa `llvmpipe` and adjusts its target frame rate to 30 FPS to minimize CPU load.
