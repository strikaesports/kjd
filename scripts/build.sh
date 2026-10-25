#!/usr/bin/env bash
# ==============================================================================
# KJD Target Build & Service Setup Script
# ==============================================================================
# Run by server operators on the Ubuntu Server target machine after `git pull`.
#
# What this script does:
#  1. Verifies build dependencies (CMake, compiler, SDL2, GLEW, OpenGL).
#  2. Compiles KJD natively on the server (Release mode).
#  3. Installs binary to ~/.local/bin/kjd (with automatic backup of old binary).
#  4. Installs shaders to ~/.local/share/kjd/shaders/.
#  5. Configures default TOML at ~/.config/kjd/config.toml (if not present).
#  6. Installs systemd user unit to ~/.config/systemd/user/kjd.service.
#  7. Reloads systemd daemon and restarts service if currently active.
#
# Does NOT require sudo.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_DIR}"

echo "=========================================================="
echo "           KJD Server Build & Setup (Phase 0)             "
echo "=========================================================="

# 1. Dependency checks
echo "[1/5] Checking build dependencies..."

MISSING_DEPS=0
for tool in cmake pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "  [!] Missing tool: $tool"
        MISSING_DEPS=1
    fi
done

if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    echo "  [!] Missing C++ compiler (g++ or clang++)"
    MISSING_DEPS=1
fi

if [[ $MISSING_DEPS -eq 1 ]]; then
    echo ""
    echo "ERROR: Missing required build tools."
    echo "Please install them on Ubuntu Server via:"
    echo "  sudo ./scripts/install_deps_ubuntu.sh"
    echo "  or: sudo apt update && sudo apt install -y build-essential cmake ninja-build pkg-config libsdl2-dev libglew-dev libgl1-mesa-dev"
    exit 1
fi

# Detect build generator (prefer Ninja, fallback to Make)
GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi
echo "  [+] Build generator: ${GENERATOR}"

# 2. Native compilation
echo "[2/5] Configuring and building KJD natively..."
BUILD_DIR="${REPO_DIR}/build"
cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" -DCMAKE_BUILD_TYPE=Release

CPU_CORES=$(nproc 2>/dev/null || echo 2)
echo "  Compiling with ${CPU_CORES} cores..."
cmake --build "${BUILD_DIR}" -j"${CPU_CORES}"

if [ ! -f "${BUILD_DIR}/kjd" ]; then
    echo "ERROR: Build failed to produce binary at ${BUILD_DIR}/kjd"
    exit 1
fi
echo "  [+] Build successful: ${BUILD_DIR}/kjd"

# 3. User directory preparation
echo "[3/5] Setting up target user directories..."
mkdir -p "${HOME}/.local/bin"
mkdir -p "${HOME}/.local/share/kjd/shaders"
mkdir -p "${HOME}/.config/kjd"
mkdir -p "${HOME}/.config/systemd/user"

# 4. Installation of binary and assets
echo "[4/5] Installing binary, shaders, and configuration..."

# Backup existing binary if present
if [ -f "${HOME}/.local/bin/kjd" ]; then
    cp -f "${HOME}/.local/bin/kjd" "${HOME}/.local/bin/kjd.bak"
    echo "  Backed up previous binary to: ${HOME}/.local/bin/kjd.bak"
fi

install -m 755 "${BUILD_DIR}/kjd" "${HOME}/.local/bin/kjd"
install -m 755 scripts/kjd-session.sh "${HOME}/.local/bin/kjd-session"
cp -f shaders/* "${HOME}/.local/share/kjd/shaders/"
echo "  [+] Installed executable to: ${HOME}/.local/bin/kjd"
echo "  [+] Installed session to:    ${HOME}/.local/bin/kjd-session"
echo "  [+] Installed shaders to:    ${HOME}/.local/share/kjd/shaders/"

# Ensure default configuration is installed without overwriting user changes
if [ ! -f "${HOME}/.config/kjd/config.toml" ]; then
    cp config/default.toml "${HOME}/.config/kjd/config.toml"
    echo "  [+] Created default config at: ${HOME}/.config/kjd/config.toml"
else
    echo "  [+] Preserved existing config at: ${HOME}/.config/kjd/config.toml"
fi

# 5. Systemd user service installation and lifecycle
echo "[5/5] Configuring systemd user service..."
cp -f systemd/kjd.service "${HOME}/.config/systemd/user/kjd.service"

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload
    echo "  [+] Reloaded systemd user daemon."

    # If already running, restart it with the new build
    if systemctl --user is-active --quiet kjd.service 2>/dev/null; then
        echo "  [+] kjd.service is currently active. Restarting service..."
        systemctl --user restart kjd.service
        sleep 1
        systemctl --user status kjd.service --no-pager || true
    else
        echo "  [i] kjd.service is not currently active."
    fi
else
    echo "  [!] Note: systemctl not found in current environment."
fi

echo ""
echo "=========================================================="
echo "             Build & Installation Complete!               "
echo "=========================================================="
echo "Binary location:  ${HOME}/.local/bin/kjd"
echo "Configuration:    ${HOME}/.config/kjd/config.toml"
echo ""
echo "Service Management Commands:"
echo "  Start:          systemctl --user start kjd.service"
echo "  Stop:           systemctl --user stop kjd.service"
echo "  Restart:        systemctl --user restart kjd.service"
echo "  Status:         systemctl --user status kjd.service"
echo "  View logs:      journalctl --user -u kjd.service -f"
echo ""
echo "Direct Console / Session Initiation (Ubuntu Server TTY):"
echo "  ./scripts/kjd-session.sh"
echo "=========================================================="
