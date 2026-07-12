// Decision tree over the four event features.
//
// Hand-set defaults (0.5g / 2.5g literature values), NOT fitted. Regenerate:
//   python fit_model.py --data <dir> --out model.pkl
//   python export_tree.py --model model.pkl --out ../firmware/nano/model.h
#pragma once
#include <Arduino.h>
#include "detect.h"

#define MODEL_N_FEATURES 4
#define MODEL_IS_DEFAULT 1

#define DEEP_FREEFALL_A2   262144UL      // (0.25g * 2048)^2
#define SETTLED_RANGE_A2   1500000UL     // |a|^2 spread once settled
#define HARD_IMPACT_A2     150000000UL   // (6g * 2048)^2

// 1 = fall, 0 = normal.
static uint8_t modelPredict(const uint32_t* f) {
  if (f[F_FF_DEPTH] <= DEEP_FREEFALL_A2) {
    // Real free-fall. A person ends up mostly still; a knocked-over stick
    // keeps rattling.
    if (f[F_STILL_RANGE] <= SETTLED_RANGE_A2) {
      return 1;
    } else {
      return 0;
    }
  } else {
    // Shallow drop - sitting down hard, cane taps. Needs a heavy impact.
    if (f[F_IMPACT_PEAK] > HARD_IMPACT_A2) {
      return 1;
    } else {
      return 0;
    }
  }
}
