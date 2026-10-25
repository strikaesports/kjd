# KJD Troubleshooting & Recovery Guide

This document covers common failure modes, diagnostic steps, and recovery procedures for KJD.

---

## 1. Quick Emergency Recovery

If KJD has taken over the screen or is failing to respond:

1. **Keyboard Exit**: Press `Esc` or `Q`.
2. **Switch Virtual Terminal**: Press `Ctrl + Alt + F2` (or `F3` through `F6`) to drop into a Linux TTY console.
3. **Stop the Systemd Service**:
   ```bash
   systemctl --user stop kjd.service
   ```
4. **Disable Autostart**:
   ```bash
   systemctl --user disable kjd.service
   ```
5. **Inspect the Journal**:
   ```bash
   journalctl --user -u kjd.service -b -e
   ```

---

## 2. Common Issues & Solutions

### Issue A: "Neither WAYLAND_DISPLAY nor DISPLAY is set"

**Cause**: KJD was started in a terminal or background service where no graphical display environment was exported.

**Solution**:
- If launching from an Ubuntu Server console/TTY: use the session launcher:
  ```bash
  ./scripts/kjd-session.sh
  ```
- If launching inside an existing desktop session (e.g., Hyprland, Sway, or GNOME):
  Ensure your display session imports environment variables to systemd:
  ```bash
  systemctl --user import-environment WAYLAND_DISPLAY DISPLAY XDG_RUNTIME_DIR
  ```

---

### Issue B: "Permission denied on /dev/dri/renderD128" or Direct Rendering Fails

**Cause**: The user account does not have permission to access the direct rendering GPU node.

**Solution**:
Add your user to the `video` and `render` groups:
```bash
sudo usermod -aG video,render "$USER"
```
Log out and log back in for group changes to take effect.

---

### Issue C: Mesa LLVMpipe / Software Rendering Active

**Symptoms**: Log indicates `Mode: SOFTWARE RENDERING (Matched Mesa LLVMpipe software rasterizer)` and higher CPU utilization.

**Cause**: No hardware-accelerated GPU driver is active for the current context.

**Resolution**:
1. Check PCI graphics devices:
   ```bash
   lspci -nnk | grep -iA2 -E "vga|3d|display"
   ```
2. On Intel systems, ensure `mesa-vulkan-drivers` and `libgl1-mesa-dri` are installed.
3. On NVIDIA systems, install proprietary drivers or ensure the kernel module is loaded:
   ```bash
   nvidia-smi
   ```
4. If running on low-end hardware (e.g., Intel Celeron), software rendering can be used safely by capping the target frame rate to 30 FPS in `~/.config/kjd/config.toml`:
   ```toml
   [render]
   target_fps = 30
   scale = 0.75
   ```

---

### Issue D: Systemd Restart Loop (`kjd.service start request repeated too quickly`)

**Cause**: KJD failed to initialize graphics repeatedly, triggering systemd's safety rate limit (`StartLimitBurst=3`).

**Resolution**:
1. Inspect the exact initialization error:
   ```bash
   journalctl --user -u kjd.service -n 50 --no-pager
   ```
2. Reset systemd's failure rate counter:
   ```bash
   systemctl --user reset-failed kjd.service
   ```
3. Test manual invocation before starting the service:
   ```bash
   ~/.local/bin/kjd -w
   ```

---

### Issue E: Cube Still Spins After Update

**Symptoms**: The cube continues to auto-rotate even after code update.

**Cause**: The deployment script intentionally preserves existing user configuration at `~/.config/kjd/config.toml` so user changes are not lost. If you had installed an earlier version, `rotate_cube = true` remains in your local user config.

**Resolution**:
Update your user config file, or copy the new default:
```bash
# Option A: Replace with the new default configuration
cp config/default.toml ~/.config/kjd/config.toml

# Option B: Edit ~/.config/kjd/config.toml and set under [scene]:
# rotate_cube = false

# Restart the service to apply:
systemctl --user restart kjd.service
```

---

### Issue F: UDP Packets Not Received / Mobile Controller Not Connecting

**Symptoms**: `[Input Stats] Connected: No | Packets: 0 accepted` remains at 0 even when the mobile app or sender script is running.

**Resolution**:
1. Check if the Ubuntu Server firewall is blocking UDP port 45821:
   ```bash
   sudo ufw status
   sudo ufw allow 45821/udp
   ```
2. Verify KJD is listening on `0.0.0.0:45821`:
   ```bash
   ss -ulnp | grep 45821
   ```
3. Check for Wi-Fi AP isolation on your router: Some home/office routers isolate Wi-Fi devices from talking to other local IPs.
4. Test locally on the server using the test sender tool:
   ```bash
   python3 scripts/test_udp_sender.py --host 127.0.0.1 --mode all
   ```

---

## 3. Rollback Procedure

To roll back to a previously working build without rebuilding the OS:

1. Restore the binary backup created by `deploy.sh`:
   ```bash
   cp -f ~/.local/bin/kjd.bak ~/.local/bin/kjd
   systemctl --user restart kjd.service
   ```

2. Or check out a previous Git commit and redeploy:
   ```bash
   git log --oneline -n 5
   git checkout <commit-hash>
   ./scripts/deploy.sh --skip-git
   ```

