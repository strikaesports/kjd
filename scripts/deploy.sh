#!/usr/bin/env bash
# ==============================================================================
# KJD Deployment Script
# ==============================================================================
# Builds KJD natively, installs user binaries and services, and reloads systemd.
# Does NOT use or require sudo.
# ==============================================================================

set -euo pipefail

# Robustly locate the repository root containing CMakeLists.txt
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
    DIR="$(cd -P "$(dirname "$SOURCE")" && pwd)"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR="$(cd -P "$(dirname "$SOURCE")" && pwd)"

if [ -f "${SCRIPT_DIR}/CMakeLists.txt" ]; then
    REPO_DIR="${SCRIPT_DIR}"
elif [ -f "${SCRIPT_DIR}/../CMakeLists.txt" ]; then
    REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
elif [ -f "$(pwd)/CMakeLists.txt" ]; then
    REPO_DIR="$(pwd)"
else
    echo "ERROR: Cannot locate directory containing CMakeLists.txt"
    exit 1
fi

cd "${REPO_DIR}"

echo "=================================================="
echo " Starting KJD Deployment"
echo "=================================================="

# 1. Check Git state
SKIP_GIT=0
for arg in "$@"; do
    if [[ "$arg" == "--skip-git" ]] || [[ "$arg" == "--force" ]]; then
        SKIP_GIT=1
    fi
done

if [[ $SKIP_GIT -eq 0 ]] && [ -d ".git" ]; then
    echo "[1/6] Checking Git status..."
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        echo "WARNING: Uncommitted changes detected in working directory."
        echo "Refusing to automatically pull remote changes."
        echo "Pass --skip-git or --force to continue with local changes."
        exit 1
    fi

    # Attempt fast-forward pull if remote is configured
    UPSTREAM=$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null || true)
    if [[ -n "$UPSTREAM" ]]; then
        echo "Pulling latest changes from ${UPSTREAM}..."
        git pull --ff-only || {
            echo "ERROR: Fast-forward pull failed. Please resolve branches manually."
            exit 1
        }
    else
        echo "No upstream tracking branch configured. Continuing with local checkout."
    fi
else
    echo "[1/6] Skipping Git sync (--skip-git or --force requested)."
fi

# 2. Build with CMake and Ninja
echo "[2/6] Compiling KJD (Release mode)..."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
CORES=$(nproc 2>/dev/null || echo 2)
cmake --build build -j"${CORES}"

if [ ! -f "build/kjd" ]; then
    echo "ERROR: Build failed to produce executable at build/kjd"
    exit 1
fi
echo "Build succeeded."

# 3. Create destination directories
echo "[3/6] Preparing user directories..."
mkdir -p "${HOME}/.local/bin"
mkdir -p "${HOME}/.local/share/kjd/shaders"
mkdir -p "${HOME}/.config/kjd"
mkdir -p "${HOME}/.config/systemd/user"

# 4. Deploy binaries and assets
echo "[4/6] Installing binaries and assets..."
if [ -f "${HOME}/.local/bin/kjd" ]; then
    cp -f "${HOME}/.local/bin/kjd" "${HOME}/.local/bin/kjd.bak"
    echo "Backed up previous binary to ${HOME}/.local/bin/kjd.bak"
fi

install -m 755 build/kjd "${HOME}/.local/bin/kjd"
cp -f shaders/* "${HOME}/.local/share/kjd/shaders/"

# Copy config if user doesn't already have one
if [ ! -f "${HOME}/.config/kjd/config.toml" ]; then
    cp config/default.toml "${HOME}/.config/kjd/config.toml"
    echo "Installed default configuration to ${HOME}/.config/kjd/config.toml"
else
    echo "Preserving existing user configuration at ${HOME}/.config/kjd/config.toml"
fi

# 5. Install systemd user service unit
echo "[5/6] Updating systemd user service..."
cp -f systemd/kjd.service "${HOME}/.config/systemd/user/kjd.service"
systemctl --user daemon-reload

# 6. Service lifecycle handling
echo "[6/6] Checking service status..."
if systemctl --user is-active --quiet kjd.service 2>/dev/null; then
    echo "Restarting active kjd.service..."
    systemctl --user restart kjd.service
    sleep 1
    systemctl --user status kjd.service --no-pager || true
else
    echo "kjd.service is currently not running."
    echo "To start it inside your graphical session:"
    echo "  systemctl --user start kjd.service"
fi

echo "=================================================="
echo " KJD Deployment complete!"
echo " Binary installed at: ${HOME}/.local/bin/kjd"
echo " Configuration:       ${HOME}/.config/kjd/config.toml"
echo "=================================================="
