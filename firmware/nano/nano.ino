// Fall-detection walking stick - Arduino Nano.
// Sampling, detection and buzzer only. Wi-Fi/Telegram is on the ESP8266,
// since the network stack blocks for seconds and can't sit in this path.
//
// Serial (115200):  s = status, l = log raw CSV, t = test alert

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "mpu6050.h"
#include "detect.h"
#include "model.h"
#include "link.h"

int freeRam();

static Detector g_detector;
static bool     g_logging = false;
static uint16_t g_seq = 0;
static uint32_t g_lastAlertMs = 0;
static uint32_t g_nextSampleMs = 0;

static void buzz() {
  for (uint8_t i = 0; i < BUZZER_BEEPS; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    digitalWrite(PIN_LED, HIGH);
    delay(BUZZER_ON_MS);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED, LOW);
    delay(BUZZER_OFF_MS);
  }
}

static void raiseAlert(const uint32_t* feats, uint32_t nowMs) {
  g_seq++;

  // Buzzer regardless of whether the ESP8266 is even connected.
  Serial.print(F("# FALL seq="));
  Serial.print(g_seq);
  Serial.print(F(" peak="));
  Serial.print(feats[F_IMPACT_PEAK]);
  Serial.print(F(" still="));
  Serial.println(feats[F_STILL_RANGE]);

  linkSendFall(g_seq, feats[F_IMPACT_PEAK], 0);
  buzz();

  g_lastAlertMs = nowMs;
}

static void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 's':
        Serial.print(F("# state="));
        Serial.print((int)g_detector.state());
        Serial.print(F(" seq="));
        Serial.print(g_seq);
        Serial.print(F(" acked="));
        Serial.print(linkAcked() ? 1 : 0);
        Serial.print(F(" ram="));
        Serial.println(freeRam());
        break;
      case 'l':
        g_logging = !g_logging;
        if (g_logging) Serial.println(F("# ax,ay,az,gx,gy,gz"));
        break;
      case 't': {
        uint32_t fake[N_FEATURES] = { 0, IMPACT_A2 * 2, 1000, 100000 };
        Serial.println(F("# test alert"));
        raiseAlert(fake, millis());
        break;
      }
      default: break;
    }
  }
}

// No malloc anywhere, so this should stay flat.
int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  Serial.println(F("\n# fall-stick nano"));

  if (!mpuInit()) {
    Serial.println(F("# FATAL: MPU6050 not responding"));
    for (;;) {
      digitalWrite(PIN_LED, HIGH); delay(100);
      digitalWrite(PIN_LED, LOW);  delay(900);
    }
  }

  g_detector.reset();
  linkInit();

  Serial.print(F("# ready, free ram = "));
  Serial.println(freeRam());
}

void loop() {
  const uint32_t now = millis();

  // No delay() - the link and serial need to run between samples.
  if ((int32_t)(now - g_nextSampleMs) >= 0) {
    g_nextSampleMs = now + SAMPLE_PERIOD_MS;

    ImuSample s;
    if (mpuRead(s)) {
      if (g_logging) {
        Serial.print(s.ax); Serial.print(',');
        Serial.print(s.ay); Serial.print(',');
        Serial.print(s.az); Serial.print(',');
        Serial.print(s.gx); Serial.print(',');
        Serial.print(s.gy); Serial.print(',');
        Serial.println(s.gz);
      }

      uint32_t feats[N_FEATURES];
      if (g_detector.push(s, now, feats) && modelPredict(feats)) {
        if (g_lastAlertMs == 0 || (now - g_lastAlertMs) > ALERT_COOLDOWN_MS) {
          raiseAlert(feats, now);
        }
      }
    }
  }

  linkPoll(now);
  handleSerial();
}
