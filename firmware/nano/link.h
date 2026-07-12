#pragma once
#include <Arduino.h>

// UART link to the ESP8266, 9600 baud over SoftwareSerial.
//   $FALL,<seq>,<peak>,<ffms>*<xor>   out
//   $ACK,<seq>*<xor>                  in
// <xor> covers everything between '$' and '*'.

void linkInit();

// Queues a message and returns. Retries happen in linkPoll().
void linkSendFall(uint16_t seq, uint32_t peak, uint16_t freefallMs);

// Call every loop.
void linkPoll(uint32_t nowMs);

bool linkAcked();
