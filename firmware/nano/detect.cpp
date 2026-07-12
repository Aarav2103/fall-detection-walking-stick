#include "detect.h"
#include "config.h"

void Detector::enter(DetectState s, uint32_t nowMs) {
  state_ = s;
  stateEnteredMs_ = nowMs;
}

void Detector::reset() {
  state_ = ST_IDLE;
  stateEnteredMs_ = 0;
  freefallMs_ = 0;
  ffDepth_ = 0xFFFFFFFFUL;
  impactPeak_ = 0;
  gyrPeak_ = 0;
  stillMin_ = 0xFFFFFFFFUL;
  stillMax_ = 0;
  stillCount_ = 0;
}

bool Detector::push(const ImuSample& s, uint32_t nowMs, uint32_t* feats) {
  const uint32_t a2 = accelMag2(s);
  const uint32_t g2 = gyroMag2(s);
  const uint32_t inState = nowMs - stateEnteredMs_;

  switch (state_) {
    case ST_IDLE:
      if (a2 < FREEFALL_A2) {
        reset();
        enter(ST_FREEFALL, nowMs);
        ffDepth_ = a2;
        gyrPeak_ = g2;
      }
      break;

    case ST_FREEFALL:
      if (g2 > gyrPeak_) gyrPeak_ = g2;
      if (a2 < ffDepth_) ffDepth_ = a2;

      if (a2 >= FREEFALL_A2) {
        freefallMs_ = inState;
        // Too short = footstep/knock, too long = lift or bag.
        if (freefallMs_ < FREEFALL_MIN_MS || freefallMs_ > FREEFALL_MAX_MS) {
          enter(ST_IDLE, nowMs);
        } else {
          enter(ST_IMPACT_WAIT, nowMs);
        }
      } else if (inState > FREEFALL_MAX_MS) {
        enter(ST_IDLE, nowMs);
      }
      break;

    case ST_IMPACT_WAIT:
      if (g2 > gyrPeak_) gyrPeak_ = g2;

      if (a2 > IMPACT_A2) {
        impactPeak_ = a2;
        stillMin_ = 0xFFFFFFFFUL;
        stillMax_ = 0;
        stillCount_ = 0;
        enter(ST_SETTLING, nowMs);
      } else if (inState > IMPACT_WINDOW_MS) {
        // Free-fall, no impact - stick was lowered.
        enter(ST_IDLE, nowMs);
      }
      break;

    case ST_SETTLING:
      if (a2 > impactPeak_) impactPeak_ = a2;   // peak can arrive late
      if (g2 > gyrPeak_) gyrPeak_ = g2;

      stillCount_++;
      // See STILL_SKIP_MS.
      if (stillCount_ > STILL_SKIP_SAMPLES) {
        if (a2 < stillMin_) stillMin_ = a2;
        if (a2 > stillMax_) stillMax_ = a2;
      }

      if (stillCount_ >= STILL_SAMPLES) {
        feats[F_FF_DEPTH]     = ffDepth_;
        feats[F_IMPACT_PEAK]  = impactPeak_;
        feats[F_STILL_RANGE]  = stillMax_ - stillMin_;
        feats[F_GYR_PEAK]     = gyrPeak_;

        enter(ST_IDLE, nowMs);
        return true;
      }
      break;
  }

  return false;
}
