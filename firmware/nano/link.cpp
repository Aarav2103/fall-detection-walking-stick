#include "link.h"
#include "config.h"
#include <SoftwareSerial.h>

#define RETRY_INTERVAL_MS 1500
#define MAX_RETRIES       4
#define RX_BUF_LEN        32

static SoftwareSerial s_link(PIN_LINK_RX, PIN_LINK_TX);

static char     s_pending[48];
static bool     s_havePending = false;
static bool     s_acked = true;
static uint16_t s_pendingSeq = 0;
static uint8_t  s_retries = 0;
static uint32_t s_lastTxMs = 0;

static char    s_rx[RX_BUF_LEN];
static uint8_t s_rxLen = 0;

static uint8_t checksum(const char* body) {
  uint8_t ck = 0;
  for (const char* p = body; *p; p++) ck ^= (uint8_t)*p;
  return ck;
}

void linkInit() {
  s_link.begin(LINK_BAUD);
  s_havePending = false;
  s_acked = true;
  s_rxLen = 0;
}

void linkSendFall(uint16_t seq, uint32_t peak, uint16_t freefallMs) {
  char body[40];
  snprintf(body, sizeof(body), "FALL,%u,%lu,%u",
           seq, (unsigned long)peak, freefallMs);
  snprintf(s_pending, sizeof(s_pending), "$%s*%02X\n", body, checksum(body));

  s_pendingSeq = seq;
  s_havePending = true;
  s_acked = false;
  s_retries = 0;
  s_lastTxMs = 0;      // forces an immediate first send from linkPoll
}

static void handleLine(char* line) {
  // Expect $ACK,<seq>*<xor>
  if (line[0] != '$') return;
  char* star = strchr(line, '*');
  if (!star) return;
  *star = '\0';

  if ((uint8_t)strtol(star + 1, NULL, 16) != checksum(line + 1)) return;

  if (strncmp(line + 1, "ACK,", 4) != 0) return;
  if ((uint16_t)atoi(line + 5) == s_pendingSeq) {
    s_acked = true;
    s_havePending = false;
  }
}

void linkPoll(uint32_t nowMs) {
  while (s_link.available()) {
    char c = (char)s_link.read();
    if (c == '\n' || c == '\r') {
      if (s_rxLen) {
        s_rx[s_rxLen] = '\0';
        handleLine(s_rx);
        s_rxLen = 0;
      }
    } else if (s_rxLen < RX_BUF_LEN - 1) {
      s_rx[s_rxLen++] = c;
    } else {
      s_rxLen = 0;   // overlong line, drop it
    }
  }

  if (!s_havePending) return;
  if (s_lastTxMs != 0 && (nowMs - s_lastTxMs) < RETRY_INTERVAL_MS) return;

  if (s_retries >= MAX_RETRIES) {
    // Give up. The buzzer already fired, which is the part that matters.
    s_havePending = false;
    return;
  }

  s_link.print(s_pending);
  s_lastTxMs = nowMs;
  s_retries++;
}

bool linkAcked() { return s_acked; }
