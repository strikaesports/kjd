#!/usr/bin/env bash
# ==============================================================================
# KJD Session Launcher (Ubuntu Server & Desktop)
# ==============================================================================
# Initiates the KJD Desktop Environment session.
# - If a Wayland or X11 display is active, runs KJD directly.
# - If started from a headless TTY or server console, launches KJD via 'cage'
#   (Wayland kiosk compositor) directly on DRM/KMS.
# ==============================================================================

set -euo pipefail

# Add ~/.local/bin to PATH
export PATH="${HOME}/.local/bin:${PATH}"

KJD_BIN=""
if [ -x "${HOME}/.local/bin/kjd" ]; then
    KJD_BIN="${HOME}/.local/bin/kjd"
elif [ -x "./build/kjd" ]; then
    KJD_BIN="./build/kjd"
elif command -v kjd >/dev/null 2>&1; then
    KJD_BIN="$(command -v kjd)"
else
    echo "ERROR: kjd executable not found. Please compile or deploy first:"
    echo "  ./scripts/deploy.sh"
    exit 1
fi

# Case 1: Inside active Wayland or X11 session
if [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; then
    echo "Launching KJD in existing graphical session..."
    exec "$KJD_BIN" "$@"
fi

# Case 2: Running from TTY / console on Ubuntu Server
echo "No active display session found. Initiating graphical session on Ubuntu Server..."

if command -v cage >/dev/null 2>&1; then
    echo "Starting KJD via Cage Wayland compositor on DRM/KMS..."
    exec cage -- "$KJD_BIN" "$@"
elif command -v xinit >/dev/null 2>&1 || command -v startx >/dev/null 2>&1; then
    echo "Starting KJD via X11 session..."
    exec xinit "$KJD_BIN" "$@" -- :0 vt"${XDG_VTNR:-1}"
else
    echo "ERROR: No display server or Wayland compositor found."
    echo "To run KJD directly from Ubuntu Server console, install 'cage':"
    echo "  sudo apt install -y cage"
    echo "Or start your desktop environment / display manager first."
    exit 1
fi
