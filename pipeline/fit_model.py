"""Fit the on-device decision tree from a public fall-detection dataset.

    python fit_model.py --data <dir> --out model.pkl

One CSV per trial, six columns of raw counts (ax,ay,az,gx,gy,gz). Label comes
from the filename - anything starting "fall" is a fall. Depth is capped at 4
because it has to compile to a few integer compares on an ATmega328P.
"""
from __future__ import annotations

import argparse
import pickle
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.metrics import confusion_matrix
from sklearn.model_selection import GroupShuffleSplit
from sklearn.tree import DecisionTreeClassifier

from features import FEATURE_NAMES, detect_events

RAW_COLUMNS = ["ax", "ay", "az", "gx", "gy", "gz"]


def load_trial(path: Path, rescale: float) -> tuple[np.ndarray, np.ndarray]:
    df = pd.read_csv(path, comment="#")
    if len(df.columns) < 6:
        raise ValueError(f"{path.name}: expected 6 columns, got {len(df.columns)}")
    cols = df.columns[:6] if not set(RAW_COLUMNS) <= set(df.columns) else RAW_COLUMNS
    raw = df[list(cols)].to_numpy(dtype=np.float64) * rescale
    raw = np.clip(np.rint(raw), -32768, 32767).astype(np.int64)
    return raw[:, :3], raw[:, 3:]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=Path("model.pkl"))
    ap.add_argument("--max-depth", type=int, default=4)
    ap.add_argument("--rescale", type=float, default=1.0,
                    help="multiply raw values by this to reach +/-16g counts "
                         "(2048 LSB/g); e.g. 0.5 for a dataset recorded at "
                         "+/-8g")
    ap.add_argument("--test-frac", type=float, default=0.25)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    files = sorted(args.data.glob("*.csv"))
    if not files:
        raise SystemExit(f"no CSVs in {args.data}")

    X, y, groups = [], [], []
    n_missed = 0
    for f in files:
        is_fall = f.name.lower().startswith("fall")
        acc, gyr = load_trial(f, args.rescale)
        events = detect_events(acc, gyr)

        if is_fall and not events:
            # State machine never triggered - a miss no tree can recover.
            n_missed += 1
            continue

        for ev in events:
            X.append(ev.features)
            y.append(1 if is_fall else 0)
            groups.append(f.stem)

    if not X:
        raise SystemExit("no events extracted -- check --rescale and the "
                         "column order")

    X = np.vstack(X).astype(np.float64)
    y = np.array(y)
    groups = np.array(groups)

    print(f"{len(files)} trials -> {len(X)} candidate events")
    print(f"  fall events   {int((y == 1).sum())}")
    print(f"  normal events {int((y == 0).sum())}")
    print(f"  fall trials the state machine never triggered on: {n_missed}")

    splitter = GroupShuffleSplit(n_splits=1, test_size=args.test_frac,
                                 random_state=args.seed)
    train_idx, test_idx = next(splitter.split(X, y, groups))

    clf = DecisionTreeClassifier(max_depth=args.max_depth,
                                 min_samples_leaf=5,
                                 class_weight="balanced",
                                 random_state=args.seed)
    clf.fit(X[train_idx], y[train_idx])

    pred = clf.predict(X[test_idx])
    cm = confusion_matrix(y[test_idx], pred, labels=[0, 1])
    print("\nheld-out confusion (rows = truth, cols = predicted)")
    print(f"  {'':<8}{'normal':>9}{'fall':>9}")
    for i, name in enumerate(["normal", "fall"]):
        print(f"  {name:<8}" + "".join(f"{v:>9d}" for v in cm[i]))

    n_fall = cm[1].sum()
    n_norm = cm[0].sum()
    recall_events = cm[1, 1] / n_fall if n_fall else float("nan")
    print(f"\n  recall on detected events   {recall_events:6.1%}")
    print(f"  false alarms on normal      "
          f"{cm[0, 1] / n_norm if n_norm else float('nan'):6.1%}")

    print("\nfeature importance:")
    for i in np.argsort(clf.feature_importances_)[::-1]:
        print(f"  {FEATURE_NAMES[i]:<14}{clf.feature_importances_[i]:.4f}")

    with args.out.open("wb") as fh:
        pickle.dump(clf, fh)
    print(f"\nsaved {args.out}")
    print(f"next: python export_tree.py --model {args.out} "
          f"--out ../firmware/nano/model.h")


if __name__ == "__main__":
    main()
