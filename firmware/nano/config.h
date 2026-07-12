#pragma once
#include <Arduino.h>

// Arduino Nano (ATmega328P). 2KB SRAM, no FPU.

// Pins
#define PIN_BUZZER      8    // via NPN, HIGH = sound
#define PIN_LINK_RX     10   // <- ESP8266 TX
#define PIN_LINK_TX     11   // -> ESP8266 RX, through divider (5V -> 3.3V)
#define PIN_LED         13

#define MPU_ADDR        0x68
#define ACCEL_LSB_PER_G 2048L    // +/-16g; impacts clip at 8g
#define GYRO_LSB_PER_DPS 16L

#define SAMPLE_RATE_HZ  100
#define SAMPLE_PERIOD_MS 10

// Thresholds as SQUARED magnitudes, so the detector never calls sqrt.
#define SQ(counts)      ((uint32_t)(counts) * (uint32_t)(counts))

#define FREEFALL_COUNTS ((int32_t)(0.5 * ACCEL_LSB_PER_G))
#define IMPACT_COUNTS   ((int32_t)(2.5 * ACCEL_LSB_PER_G))
#define FREEFALL_A2     SQ(FREEFALL_COUNTS)
#define IMPACT_A2       SQ(IMPACT_COUNTS)

// Event timing
#define FREEFALL_MIN_MS   80    // shorter = a knock
#define FREEFALL_MAX_MS  800    // longer = a lift or a bag
#define IMPACT_WINDOW_MS 400    // impact must follow free-fall within this
#define STILL_WINDOW_MS 1000
#define STILL_SKIP_MS    300    // stick is still bouncing, ignore

#define STILL_SAMPLES      (STILL_WINDOW_MS / SAMPLE_PERIOD_MS)
#define STILL_SKIP_SAMPLES (STILL_SKIP_MS / SAMPLE_PERIOD_MS)

// Otherwise stillMax - stillMin underflows.
static_assert(STILL_SKIP_SAMPLES < STILL_SAMPLES, "skip must leave samples");

#define ALERT_COOLDOWN_MS 30000UL

#define BUZZER_ON_MS    400
#define BUZZER_OFF_MS   200
#define BUZZER_BEEPS    8

#define SERIAL_BAUD     115200L
#define LINK_BAUD       9600L   // SoftwareSerial drops bits much above this
