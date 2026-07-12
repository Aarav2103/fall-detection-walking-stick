#include "mpu6050.h"
#include "config.h"
#include <Wire.h>

#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75
#define REG_ACCEL_XOUT_H 0x3B

static bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuInit() {
  Wire.begin();
  Wire.setClock(400000L);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_WHO_AM_I);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1) != 1) return false;
  uint8_t who = Wire.read();
  if (who != 0x68 && who != 0x72) return false;

  wr(REG_PWR_MGMT_1, 0x80);   // reset
  delay(100);
  wr(REG_PWR_MGMT_1, 0x01);   // wake, PLL with X gyro
  delay(10);

  // DLPF 44Hz, under Nyquist for 100Hz. Base rate is 1kHz with DLPF on.
  wr(REG_CONFIG, 0x03);
  wr(REG_SMPLRT_DIV, (uint8_t)((1000 / SAMPLE_RATE_HZ) - 1));

  wr(REG_GYRO_CONFIG,  0x18); // +/-2000 dps
  wr(REG_ACCEL_CONFIG, 0x18); // +/-16 g

  return true;
}

bool mpuRead(ImuSample& s) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  // 14 bytes: accel, temp, gyro. Temp discarded - skipping it would need
  // a second transaction.
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14) != 14) return false;

  s.ax = (int16_t)((Wire.read() << 8) | Wire.read());
  s.ay = (int16_t)((Wire.read() << 8) | Wire.read());
  s.az = (int16_t)((Wire.read() << 8) | Wire.read());
  Wire.read(); Wire.read();
  s.gx = (int16_t)((Wire.read() << 8) | Wire.read());
  s.gy = (int16_t)((Wire.read() << 8) | Wire.read());
  s.gz = (int16_t)((Wire.read() << 8) | Wire.read());
  return true;
}

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
