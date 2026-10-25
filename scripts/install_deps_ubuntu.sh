#!/usr/bin/env bash
# ==============================================================================
# KJD Ubuntu Server Dependency Installer
# ==============================================================================
# Installs all required packages to build and run KJD on Ubuntu Server (22.04 / 24.04).
# Usage: sudo ./scripts/install_deps_ubuntu.sh
# ==============================================================================

set -euo pipefail

if [ "$EUID" -ne 0 ]; then
    echo "This script must be run as root (or with sudo) to install apt packages."
    echo "Usage: sudo ./scripts/install_deps_ubuntu.sh"
    exit 1
fi

echo "=================================================="
echo " Installing KJD Dependencies on Ubuntu Server"
echo "=================================================="

export DEBIAN_FRONTEND=noninteractive

echo "[1/3] Updating package repositories..."
apt-get update -y

echo "[2/3] Installing build tools and core libraries..."
apt-get install -y \
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
    pciutils

echo "[3/3] Installing display & Wayland launcher tools..."
# 'cage' is a lightweight Wayland kiosk compositor that allows launching KJD
# fullscreen directly from a server TTY without installing a heavyweight desktop.
apt-get install -y cage || {
    echo "Notice: cage was not found in standard repositories. Falling back to xorg/xinit."
    apt-get install -y xorg xinit || true
}

if [ -n "${SUDO_USER:-}" ]; then
    echo "Adding user '${SUDO_USER}' to video, render, and input groups for direct DRM/GPU access..."
    usermod -aG video,render,input "${SUDO_USER}" || true
fi

echo "=================================================="
echo " All Ubuntu Server dependencies installed successfully!"
echo " Next step: Pull and build KJD natively on this server:"
echo "   ./build.sh"
echo "=================================================="
