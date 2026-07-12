#pragma once
#include <Arduino.h>

// Raw sensor counts. The detector works in counts, so there is no float
// conversion anywhere in the sample path.
struct ImuSample {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};

bool mpuInit();
bool mpuRead(ImuSample& s);

// Squared magnitudes, counts^2. uint32 because 3 * 32767^2 overflows int32.
uint32_t accelMag2(const ImuSample& s);
uint32_t gyroMag2(const ImuSample& s);
