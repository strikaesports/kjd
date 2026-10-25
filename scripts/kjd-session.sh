#!/usr/bin/env bash
# ==============================================================================
# KJD Session Launcher (Ubuntu Server & Desktop)
# ==============================================================================
# Initiates the KJD Desktop Environment session.
#  1. Connects to existing Wayland or X11 session if active.
#  2. Auto-detects active Wayland sockets (/run/user/UID/wayland-*) or X11 sockets.
#  3. If no display server is active on Ubuntu Server, initiates the DE using
#     'cage' (lightweight Wayland compositor) directly on DRM/KMS.
# ==============================================================================

set -euo pipefail

# Add ~/.local/bin to PATH
export PATH="${HOME}/.local/bin:${PATH}"

# Ensure XDG_RUNTIME_DIR is set
if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
    USER_ID=$(id -u)
    if [ -d "/run/user/${USER_ID}" ]; then
        export XDG_RUNTIME_DIR="/run/user/${USER_ID}"
    fi
fi

KJD_BIN=""
if [ -x "${HOME}/.local/bin/kjd" ]; then
    KJD_BIN="${HOME}/.local/bin/kjd"
elif [ -x "./build/kjd" ]; then
    KJD_BIN="./build/kjd"
elif command -v kjd >/dev/null 2>&1; then
    KJD_BIN="$(command -v kjd)"
else
    echo "ERROR: kjd executable not found. Please run build.sh first."
    exit 1
fi

# Step 1: Check if display environment variables are already set
if [ -n "${WAYLAND_DISPLAY:-}" ]; then
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland,x11}"
    echo "Launching KJD in existing Wayland session (WAYLAND_DISPLAY=${WAYLAND_DISPLAY}, SDL_VIDEODRIVER=${SDL_VIDEODRIVER})..."
    exec "$KJD_BIN" "$@"
elif [ -n "${DISPLAY:-}" ]; then
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11,wayland}"
    echo "Launching KJD in existing X11 session (DISPLAY=${DISPLAY}, SDL_VIDEODRIVER=${SDL_VIDEODRIVER})..."
    exec "$KJD_BIN" "$@"
fi

# Step 2: Auto-detect active Wayland socket in XDG_RUNTIME_DIR
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
for i in 0 1 2 3; do
    SOCK="${RUNTIME_DIR}/wayland-${i}"
    if [ -S "$SOCK" ] && [ -w "$SOCK" ]; then
        export WAYLAND_DISPLAY="wayland-${i}"
        export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland,x11}"
        echo "Auto-detected active Wayland display socket: $SOCK -> export WAYLAND_DISPLAY=wayland-${i} SDL_VIDEODRIVER=${SDL_VIDEODRIVER}"
        exec "$KJD_BIN" "$@"
    fi
done

# Step 3: Auto-detect active X11 socket in /tmp/.X11-unix/
for i in 0 1 2 3; do
    XSOCK="/tmp/.X11-unix/X${i}"
    if [ -S "$XSOCK" ]; then
        export DISPLAY=":${i}"
        export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11,wayland}"
        echo "Auto-detected active X11 display socket: $XSOCK -> export DISPLAY=:${i} SDL_VIDEODRIVER=${SDL_VIDEODRIVER}"
        exec "$KJD_BIN" "$@"
    fi
done

# Step 4: No display server is active -> Initiate KJD DE directly on Ubuntu Server
echo "No existing display server detected. Initiating KJD Desktop Environment on Ubuntu Server..."

if command -v cage >/dev/null 2>&1; then
    echo "Starting Cage Wayland compositor on DRM/KMS..."
    exec cage -s -- "$KJD_BIN" "$@"
elif command -v xinit >/dev/null 2>&1 || command -v startx >/dev/null 2>&1; then
    echo "Starting X11 session..."
    exec xinit "$KJD_BIN" "$@" -- :0 vt"${XDG_VTNR:-1}"
else
    echo "======================================================================"
    echo "ERROR: No active graphical display found, and 'cage' is not installed."
    echo ""
    echo "To initiate the KJD 3D Desktop Environment on Ubuntu Server without"
    echo "a desktop manager, please install 'cage' (lightweight Wayland compositor):"
    echo "  sudo apt update && sudo apt install -y cage"
    echo "======================================================================"
    exit 1
fi
