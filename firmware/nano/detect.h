#pragma once
#include <Arduino.h>
#include "mpu6050.h"

// Features the tree sees. Squared magnitudes in raw counts.
// pipeline/features.py computes the same four.
#define N_FEATURES 4

enum FeatureIndex {
  F_FF_DEPTH = 0,   // min |a|^2 during free-fall
  F_IMPACT_PEAK,    // max |a|^2 at impact
  F_STILL_RANGE,    // max-min |a|^2 over the tail of the settling window
  F_GYR_PEAK        // max |w|^2 during the event
};

// Free-fall -> impact -> stillness. Tracks running extrema only; a buffered
// 2s window would be 2400 bytes and the chip has 2048.
enum DetectState {
  ST_IDLE = 0,
  ST_FREEFALL,
  ST_IMPACT_WAIT,
  ST_SETTLING
};

class Detector {
 public:
  void reset();

  // True when an event completes, filling feats[]. Classification is
  // modelPredict()'s job. Call at SAMPLE_RATE_HZ.
  bool push(const ImuSample& s, uint32_t nowMs, uint32_t* feats);

  DetectState state() const { return state_; }

 private:
  DetectState state_ = ST_IDLE;
  uint32_t stateEnteredMs_ = 0;
  uint32_t freefallMs_ = 0;

  uint32_t ffDepth_ = 0;
  uint32_t impactPeak_ = 0;
  uint32_t gyrPeak_ = 0;
  uint32_t stillMin_ = 0;
  uint32_t stillMax_ = 0;
  uint16_t stillCount_ = 0;

  void enter(DetectState s, uint32_t nowMs);
};
