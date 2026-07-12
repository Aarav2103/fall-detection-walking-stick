// ESP8266 network board. Reads $FALL frames from the Nano over UART and
// forwards them to Telegram.
//
// ACK goes out BEFORE the HTTPS call - a TLS handshake takes longer than the
// Nano's 1.5s retry interval, so ACKing after would cause duplicates.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

#define RX_BUF_LEN 64

static char     s_rx[RX_BUF_LEN];
static uint8_t  s_rxLen = 0;
static uint16_t s_lastSeq = 0;
static bool     s_haveLastSeq = false;

static uint8_t checksum(const char* body) {
  uint8_t ck = 0;
  for (const char* p = body; *p; p++) ck ^= (uint8_t)*p;
  return ck;
}

static void sendAck(uint16_t seq) {
  char body[16];
  snprintf(body, sizeof(body), "ACK,%u", seq);
  Serial.printf("$%s*%02X\n", body, checksum(body));
}

static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) return false;
    delay(250);
  }
  return true;
}

// Percent-encode. An unencoded space would truncate the request line.
static bool urlEncode(const char* in, char* out, size_t outSize) {
  static const char kHex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (const char* p = in; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      if (o + 1 >= outSize) return false;
      out[o++] = (char)c;
    } else {
      if (o + 3 >= outSize) return false;
      out[o++] = '%';
      out[o++] = kHex[c >> 4];
      out[o++] = kHex[c & 0x0F];
    }
  }
  out[o] = '\0';
  return true;
}

static bool sendTelegram(uint16_t seq, unsigned long peak) {
  if (!ensureWifi()) return false;

  WiFiClientSecure client;
  // No cert validation. Fine on the bench, pin the fingerprint before real use.
  client.setInsecure();
  client.setTimeout(HTTP_TIMEOUT_MS);

  if (!client.connect("api.telegram.org", 443)) return false;

  char text[160];
  snprintf(text, sizeof(text),
           "FALL DETECTED\nwalking stick alert #%u\nimpact index: %lu",
           seq, peak);

  char encoded[3 * sizeof(text)];
  if (!urlEncode(text, encoded, sizeof(encoded))) return false;

  char path[640];
  snprintf(path, sizeof(path), "/bot%s/sendMessage?chat_id=%s&text=%s",
           TELEGRAM_TOKEN, TELEGRAM_CHAT, encoded);

  client.printf("GET %s HTTP/1.1\r\n"
                "Host: api.telegram.org\r\n"
                "Connection: close\r\n\r\n", path);

  uint32_t deadline = millis() + HTTP_TIMEOUT_MS;
  bool ok = false;
  while (client.connected() && millis() < deadline) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("HTTP/1.1 200")) ok = true;
    if (line == "\r") break;
  }
  client.stop();
  return ok;
}

static void handleLine(char* line) {
  if (line[0] != '$') return;
  char* star = strchr(line, '*');
  if (!star) return;
  *star = '\0';

  if ((uint8_t)strtol(star + 1, NULL, 16) != checksum(line + 1)) return;
  if (strncmp(line + 1, "FALL,", 5) != 0) return;

  char* seqStr = line + 6;
  char* peakStr = strchr(seqStr, ',');
  if (!peakStr) return;
  *peakStr++ = '\0';

  uint16_t seq = (uint16_t)atoi(seqStr);
  unsigned long peak = strtoul(peakStr, NULL, 10);

  sendAck(seq);

  // Retry that crossed the ACK, not a second fall.
  if (s_haveLastSeq && seq == s_lastSeq) return;
  s_lastSeq = seq;
  s_haveLastSeq = true;

  sendTelegram(seq, peak);
}

void setup() {
  Serial.begin(LINK_BAUD);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (s_rxLen) {
        s_rx[s_rxLen] = '\0';
        handleLine(s_rx);
        s_rxLen = 0;
      }
    } else if (s_rxLen < RX_BUF_LEN - 1) {
      s_rx[s_rxLen++] = c;
    } else {
      s_rxLen = 0;
    }
  }
}
