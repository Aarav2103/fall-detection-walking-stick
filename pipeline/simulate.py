"""Replay a capture through the detector offline.

    python simulate.py --csv capture.csv

Takes the CSV the Nano dumps in logging mode ('l' over serial) and prints the
events it would have produced. Saves re-doing a motion to see why it missed.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from features import FEATURE_NAMES, SAMPLE_PERIOD_MS, detect_events


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", type=Path, required=True)
    args = ap.parse_args()

    df = pd.read_csv(args.csv, comment="#")
    raw = df.iloc[:, :6].to_numpy(dtype=np.int64)
    acc, gyr = raw[:, :3], raw[:, 3:]

    print(f"{len(raw)} samples ({len(raw) * SAMPLE_PERIOD_MS / 1000:.1f} s)\n")

    events = detect_events(acc, gyr)
    if not events:
        print("no events -- the state machine never completed a sequence")
        return

    width = max(len(n) for n in FEATURE_NAMES)
    for i, ev in enumerate(events):
        t = ev.sample_index * SAMPLE_PERIOD_MS / 1000
        print(f"event {i}  at {t:.2f} s")
        for name, val in zip(FEATURE_NAMES, ev.features):
            print(f"  {name:<{width}}  {int(val):>12d}")
        print()


if __name__ == "__main__":
    main()
