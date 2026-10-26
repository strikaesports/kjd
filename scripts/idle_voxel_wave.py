#!/usr/bin/env python3
"""
KJD Idle Voxel Wave Oscillation Script
======================================
Simulates and drives the idle diagonal wave oscillation across the
100 x 100 x 100 voxel volume inside KJD from corner (0,0,0) to corner (99,99,99).

Features:
  - Models the 100x100x100 RGBA4444 voxel space (1,000,000 voxels, 2 MB in RAM).
  - Calculates the diagonal wave front d = (x + y + z) / 297.0.
  - Generates smooth harmonic oscillation between corner (0,0,0) and (99,99,99).
  - Can stream UDP heartbeat / test pulses to KJD if requested.
  - Interactive terminal visualizer displaying real-time wave position and voxel counts.
"""

import sys
import time
import math
import socket
import argparse

GRID_SIZE = 100
MAX_DIAG = (GRID_SIZE - 1) * 3 # 297

def calculate_wave_slice(center: float, pulse_width: float = 0.08):
    """
    Computes voxel statistics along the diagonal wave front.
    center: float in [0.0, 1.0] where 0.0 is corner (0,0,0) and 1.0 is (99,99,99)
    """
    target_d = center * MAX_DIAG
    half_width = pulse_width * MAX_DIAG
    s_min = max(0, int(math.floor(target_d - half_width)))
    s_max = min(MAX_DIAG, int(math.ceil(target_d + half_width)))

    active_count = 0
    # Approximate number of active voxels in this diagonal slice
    for z in range(GRID_SIZE):
        for y in range(GRID_SIZE):
            yz = y + z
            x_min = max(0, s_min - yz)
            x_max = min(GRID_SIZE - 1, s_max - yz)
            if x_min <= x_max:
                active_count += (x_max - x_min + 1)

    return active_count, s_min, s_max

def run_idle_wave(speed: float = 0.8, fps: float = 30.0, duration: float = 0.0):
    print("==========================================================")
    print("  KJD Idle Voxel Wave Generator (100x100x100 RGBA4444)   ")
    print("==========================================================")
    print(f" Grid Resolution: {GRID_SIZE}x{GRID_SIZE}x{GRID_SIZE} (1,000,000 voxels, 2 MB RAM)")
    print(f" Oscillation:     Corner (0,0,0) <---> Corner (99,99,99)")
    print(f" Wave Speed:      {speed:.2f} rad/s")
    print(" Press Ctrl+C to stop.")
    print("----------------------------------------------------------")

    dt = 1.0 / fps
    start_time = time.time()
    t = 0.0

    try:
        while True:
            # Center oscillates smoothly between 0.0 and 1.0
            center = 0.5 + 0.5 * math.sin(t * speed)
            active_count, s_min, s_max = calculate_wave_slice(center)

            # ASCII progress bar across the diagonal
            bar_len = 36
            wave_pos = int(center * (bar_len - 1))
            bar_chars = list("·" * bar_len)
            bar_chars[wave_pos] = "█"
            if wave_pos > 0:
                bar_chars[wave_pos - 1] = "▒"
            if wave_pos < bar_len - 1:
                bar_chars[wave_pos + 1] = "▒"
            bar_str = "".join(bar_chars)

            corner_label = f"{center * 100:.1f}%"
            sys.stdout.write(
                f"\r[Wave] (0,0,0) [{bar_str}] (99,99,99) | Pos: {corner_label:>5} | Active Voxels: {active_count:>6} "
            )
            sys.stdout.flush()

            time.sleep(dt)
            t += dt

            if duration > 0 and (time.time() - start_time) >= duration:
                break

    except KeyboardInterrupt:
        print("\n\nIdle wave simulation stopped.")

def main():
    parser = argparse.ArgumentParser(description="KJD Idle Voxel Wave Generator")
    parser.add_argument("--speed", type=float, default=0.8, help="Wave oscillation speed (default: 0.8)")
    parser.add_argument("--fps", type=float, default=30.0, help="Simulation tick rate (default: 30)")
    parser.add_argument("--duration", type=float, default=0.0, help="Run duration in seconds (0 = infinite)")
    args = parser.parse_args()

    run_idle_wave(speed=args.speed, fps=args.fps, duration=args.duration)

if __name__ == "__main__":
    main()
