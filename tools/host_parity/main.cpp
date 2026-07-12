// Host harness for the on-device detector.
//
// Reads "ax,ay,az,gx,gy,gz" rows of raw counts on stdin, runs the real
// Detector from firmware/nano/detect.cpp, and prints one line per completed
// event. run_parity.py feeds the same rows to pipeline/features.py and
// compares, which is what stops the C and Python definitions drifting apart.

#include "Arduino.h"
#include "../../firmware/nano/config.h"
#include "../../firmware/nano/mpu6050.h"
#include "../../firmware/nano/detect.h"

// Provided here rather than by linking mpu6050.cpp, which needs Wire.h.
uint32_t accelMag2(const ImuSample& s) {
  return (uint32_t)((int32_t)s.ax * s.ax)
       + (uint32_t)((int32_t)s.ay * s.ay)
       + (uint32_t)((int32_t)s.az * s.az);
}

uint32_t gyroMag2(const ImuSample& s) {
  return (uint32_t)((int32_t)s.gx * s.gx)
       + (uint32_t)((int32_t)s.gy * s.gy)
       + (uint32_t)((int32_t)s.gz * s.gz);
}

int main() {
  Detector det;
  det.reset();

  char line[128];
  uint32_t i = 0;
  uint32_t feats[N_FEATURES];

  while (fgets(line, sizeof(line), stdin)) {
    if (line[0] == '#' || line[0] == '\n') continue;

    ImuSample s;
    int n = sscanf(line, "%hd,%hd,%hd,%hd,%hd,%hd",
                   &s.ax, &s.ay, &s.az, &s.gx, &s.gy, &s.gz);
    if (n != 6) continue;

    // The sketch calls push() once per SAMPLE_PERIOD_MS, so synthesise the
    // same clock rather than reading a real one.
    uint32_t nowMs = i * SAMPLE_PERIOD_MS;
    if (det.push(s, nowMs, feats)) {
      for (int k = 0; k < N_FEATURES; k++) {
        printf("%s%lu", k ? "," : "", (unsigned long)feats[k]);
      }
      printf("\n");
    }
    i++;
  }
  return 0;
}
