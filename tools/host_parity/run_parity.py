"""Check firmware/nano/detect.cpp and pipeline/features.py agree.

    python run_parity.py

Compiles the real detector for the host and feeds both it and the Python
version the same vectors. The tree is fitted on the Python features and runs
against the C ones, so drift between them changes what the device does.

These vectors are test inputs, not data.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "pipeline"))

from features import FEATURE_NAMES, detect_events  # noqa: E402

LSB_G = 2048


def vec_freefall_impact_still(rng, *, ff_g, peak_g, still_sd, spin):
    """Free-fall, impact spike, then rest."""
    parts = []
    parts.append(np.tile([0, 0, int(1.0 * LSB_G)], (80, 1)))          # upright
    parts.append(np.tile([0, 0, int(ff_g * LSB_G)], (25, 1)))         # 250 ms free-fall
    spike = np.zeros((8, 3), dtype=int)
    spike[:, 2] = int(peak_g * LSB_G)
    parts.append(spike)
    rest = np.tile([int(0.98 * LSB_G), 0, 0], (140, 1))               # lying on its side
    rest = rest + rng.normal(0, still_sd * LSB_G, rest.shape).astype(int)
    parts.append(rest)
    acc = np.vstack(parts)
    gyr = rng.normal(0, spin, acc.shape).astype(int)
    return acc, gyr


def vec_walking(rng):
    """Never gets near free-fall - expect no events."""
    t = np.arange(600) / 100.0
    acc = np.zeros((600, 3), dtype=int)
    acc[:, 2] = (LSB_G * (1.0 + 0.25 * np.sin(2 * np.pi * 1.8 * t))).astype(int)
    acc[:, 0] = (LSB_G * 0.18 * np.sin(2 * np.pi * 1.8 * t)).astype(int)
    gyr = rng.normal(0, 400, acc.shape).astype(int)
    return acc, gyr


def vec_short_bump(rng):
    """Too brief - should be rejected by FREEFALL_MIN_MS."""
    parts = [np.tile([0, 0, LSB_G], (60, 1)),
             np.tile([0, 0, 200], (3, 1)),           # 30 ms
             np.tile([0, 0, 3 * LSB_G], (5, 1)),
             np.tile([0, 0, LSB_G], (150, 1))]
    acc = np.vstack(parts)
    gyr = rng.normal(0, 100, acc.shape).astype(int)
    return acc, gyr


def vec_no_impact(rng):
    """Free-fall then a soft stop - stick lowered."""
    parts = [np.tile([0, 0, LSB_G], (60, 1)),
             np.tile([0, 0, int(0.3 * LSB_G)], (25, 1)),
             np.tile([0, 0, int(1.2 * LSB_G)], (200, 1))]
    acc = np.vstack(parts)
    gyr = rng.normal(0, 100, acc.shape).astype(int)
    return acc, gyr


def build() -> Path:
    exe = HERE / "host_detect"
    cmd = ["g++", "-std=c++11", "-O2", "-Wall", "-Wextra",
           "-I", str(HERE), str(HERE / "main.cpp"),
           str(ROOT / "firmware/nano/detect.cpp"), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout)
        print(r.stderr)
        raise SystemExit("build failed")
    if r.stderr.strip():
        print("compiler warnings:\n" + r.stderr)
    return exe


def run_c(exe: Path, acc, gyr):
    rows = "\n".join(f"{a[0]},{a[1]},{a[2]},{g[0]},{g[1]},{g[2]}"
                     for a, g in zip(acc, gyr))
    r = subprocess.run([str(exe)], input=rows, capture_output=True, text=True)
    out = []
    for line in r.stdout.strip().splitlines():
        if line:
            out.append([int(v) for v in line.split(",")])
    return out


def main() -> None:
    exe = build()
    rng = np.random.default_rng(0)

    cases = {
        "hard fall, settles":   vec_freefall_impact_still(
            rng, ff_g=0.10, peak_g=7.0, still_sd=0.01, spin=300),
        "soft fall, restless":  vec_freefall_impact_still(
            rng, ff_g=0.20, peak_g=3.0, still_sd=0.08, spin=200),
        "walking":              vec_walking(rng),
        "short bump":           vec_short_bump(rng),
        "lowered, no impact":   vec_no_impact(rng),
    }

    failures = 0
    for name, (acc, gyr) in cases.items():
        c_events = run_c(exe, acc, gyr)
        py_events = [[int(v) for v in e.features] for e in detect_events(acc, gyr)]

        if c_events == py_events:
            detail = (f"{len(c_events)} event(s)" if c_events else "no events")
            print(f"  ok    {name:<22} {detail}")
        else:
            failures += 1
            print(f"  FAIL  {name}")
            print(f"          C      : {c_events}")
            print(f"          Python : {py_events}")

    print()
    if failures:
        print(f"{failures} case(s) disagree -- detect.cpp and features.py have "
              f"drifted")
        raise SystemExit(1)
    print(f"all {len(cases)} cases agree "
          f"({', '.join(FEATURE_NAMES)})")


if __name__ == "__main__":
    main()
