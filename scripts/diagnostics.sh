#!/usr/bin/env bash
# ==============================================================================
# KJD Diagnostics Gathering Script
# ==============================================================================
# Inspects operating system, graphics context, service status, and recent logs.
# Completely read-only — does NOT modify system state or expose sensitive tokens.
# ==============================================================================

set -u

echo "=================================================================="
echo "                   KJD SYSTEM DIAGNOSTICS REPORT                 "
echo "=================================================================="
echo "Timestamp: $(date -u +"%Y-%m-%d %H:%M:%SZ")"

echo ""
echo "--- [1] Operating System & Hardware ---"
if [ -f "/etc/os-release" ]; then
    grep -E "^(PRETTY_NAME|ID|VERSION_ID)=" /etc/os-release || true
fi
echo "Kernel:      $(uname -sr)"
echo "Arch:        $(uname -m)"
echo "Uptime:      $(uptime -p 2>/dev/null || uptime)"
echo "Memory:      $(free -h 2>/dev/null | awk '/^Mem:/ {print $3 "/" $2}' || echo 'N/A')"

echo ""
echo "--- [2] Session & Display Environment ---"
echo "XDG_SESSION_TYPE:    ${XDG_SESSION_TYPE:-<unset>}"
echo "WAYLAND_DISPLAY:     ${WAYLAND_DISPLAY:-<unset>}"
echo "DISPLAY:             ${DISPLAY:-<unset>}"
echo "XDG_CURRENT_DESKTOP: ${XDG_CURRENT_DESKTOP:-<unset>}"
echo "XDG_RUNTIME_DIR:     ${XDG_RUNTIME_DIR:-<unset>}"

echo ""
echo "--- [3] PCI Graphics Devices ---"
if command -v lspci >/dev/null 2>&1; then
    lspci -nnk | grep -iA2 -E "vga|3d|display" || echo "No VGA/3D PCI devices found."
else
    echo "lspci utility not found."
fi

echo ""
echo "--- [4] OpenGL & Driver Status ---"
if command -v glxinfo >/dev/null 2>&1; then
    glxinfo -B 2>/dev/null || echo "glxinfo failed to query active display."
else
    echo "glxinfo not installed (install mesa-utils on Ubuntu or mesa-demos on Arch if desired)."
fi

echo ""
echo "--- [5] KJD Application Binaries ---"
KJD_BIN=""
if [ -x "${HOME}/.local/bin/kjd" ]; then
    KJD_BIN="${HOME}/.local/bin/kjd"
elif [ -x "./build/kjd" ]; then
    KJD_BIN="./build/kjd"
elif command -v kjd >/dev/null 2>&1; then
    KJD_BIN="$(command -v kjd)"
fi

if [ -n "$KJD_BIN" ]; then
    echo "Found KJD binary: $KJD_BIN"
    ls -l "$KJD_BIN"
    echo "Version: $($KJD_BIN --version 2>/dev/null || echo 'Unknown')"
else
    echo "KJD binary not found in ~/.local/bin or ./build. Run deploy.sh or cmake build first."
fi

echo ""
echo "--- [6] KJD Configuration ---"
if [ -f "${HOME}/.config/kjd/config.toml" ]; then
    echo "Config file exists at: ${HOME}/.config/kjd/config.toml"
    head -n 25 "${HOME}/.config/kjd/config.toml"
elif [ -f "config/default.toml" ]; then
    echo "Using default config template at: config/default.toml"
else
    echo "No config file found. KJD will use built-in defaults."
fi

echo ""
echo "--- [7] Systemd User Service Status ---"
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user status kjd.service --no-pager 2>&1 || true
else
    echo "systemctl not available."
fi

echo ""
echo "--- [8] Recent KJD Journal Logs ---"
if command -v journalctl >/dev/null 2>&1; then
    journalctl --user -u kjd.service -n 25 --no-pager 2>&1 || echo "No journal entries found."
else
    echo "journalctl not available."
fi

echo ""
echo "=================================================================="
echo "                    END OF DIAGNOSTICS REPORT                    "
echo "=================================================================="
