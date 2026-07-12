#pragma once

// ESP8266 (NodeMCU). Network only - never sees the accelerometer.
// Blocking for seconds here is fine; on the Nano it would not be.

#define LINK_BAUD  9600L

#define WIFI_CONNECT_TIMEOUT_MS 20000UL
#define HTTP_TIMEOUT_MS         8000UL

// Credentials live in secrets.h, which is gitignored. Copy secrets.example.h.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Missing secrets.h -- copy secrets.example.h to secrets.h and fill it in."
#endif
