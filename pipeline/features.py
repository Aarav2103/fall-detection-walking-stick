"""Mirror of firmware/nano/detect.cpp - same state machine, same integer maths.

These two must agree; tools/host_parity checks that they do.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

# Must match firmware/nano/config.h
ACCEL_LSB_PER_G = 2048
SAMPLE_PERIOD_MS = 10

FREEFALL_COUNTS = int(0.5 * ACCEL_LSB_PER_G)
IMPACT_COUNTS = int(2.5 * ACCEL_LSB_PER_G)
FREEFALL_A2 = FREEFALL_COUNTS ** 2
IMPACT_A2 = IMPACT_COUNTS ** 2

FREEFALL_MIN_MS = 80
FREEFALL_MAX_MS = 800
IMPACT_WINDOW_MS = 400
STILL_WINDOW_MS = 1000
STILL_SKIP_MS = 300
STILL_SAMPLES = STILL_WINDOW_MS // SAMPLE_PERIOD_MS
STILL_SKIP_SAMPLES = STILL_SKIP_MS // SAMPLE_PERIOD_MS

FEATURE_NAMES = ["ff_depth", "impact_peak", "still_range", "gyr_peak"]
N_FEATURES = len(FEATURE_NAMES)

ST_IDLE, ST_FREEFALL, ST_IMPACT_WAIT, ST_SETTLING = range(4)


@dataclass
class Event:
    """One completed free-fall -> impact -> stillness sequence."""
    sample_index: int          # where the impact landed
    features: np.ndarray       # (N_FEATURES,) uint32


def detect_events(acc: np.ndarray, gyr: np.ndarray) -> list[Event]:
    """Run the detector over raw counts. acc, gyr: (N, 3) int."""
    acc = np.asarray(acc, dtype=np.int64)
    gyr = np.asarray(gyr, dtype=np.int64)
    a2_all = (acc ** 2).sum(axis=1)
    g2_all = (gyr ** 2).sum(axis=1)

    events: list[Event] = []
    state = ST_IDLE
    entered = 0
    ff_depth = impact_peak = gyr_peak = 0
    still_min = still_max = 0
    still_count = 0
    impact_at = 0

    for i in range(len(a2_all)):
        a2 = int(a2_all[i])
        g2 = int(g2_all[i])
        in_state_ms = (i - entered) * SAMPLE_PERIOD_MS

        if state == ST_IDLE:
            if a2 < FREEFALL_A2:
                state, entered = ST_FREEFALL, i
                ff_depth, gyr_peak = a2, g2
                impact_peak = 0

        elif state == ST_FREEFALL:
            gyr_peak = max(gyr_peak, g2)
            ff_depth = min(ff_depth, a2)
            if a2 >= FREEFALL_A2:
                if not (FREEFALL_MIN_MS <= in_state_ms <= FREEFALL_MAX_MS):
                    state, entered = ST_IDLE, i
                else:
                    state, entered = ST_IMPACT_WAIT, i
            elif in_state_ms > FREEFALL_MAX_MS:
                state, entered = ST_IDLE, i

        elif state == ST_IMPACT_WAIT:
            gyr_peak = max(gyr_peak, g2)
            if a2 > IMPACT_A2:
                impact_peak = a2
                impact_at = i
                still_min, still_max, still_count = 0xFFFFFFFF, 0, 0
                state, entered = ST_SETTLING, i
            elif in_state_ms > IMPACT_WINDOW_MS:
                state, entered = ST_IDLE, i

        elif state == ST_SETTLING:
            impact_peak = max(impact_peak, a2)
            gyr_peak = max(gyr_peak, g2)
            still_count += 1
            # Only after the bounce has died down -- see STILL_SKIP_MS.
            if still_count > STILL_SKIP_SAMPLES:
                still_min = min(still_min, a2)
                still_max = max(still_max, a2)

            if still_count >= STILL_SAMPLES:
                events.append(Event(
                    sample_index=impact_at,
                    features=np.array([ff_depth, impact_peak,
                                       still_max - still_min, gyr_peak],
                                      dtype=np.uint32)))
                state, entered = ST_IDLE, i

    return events
