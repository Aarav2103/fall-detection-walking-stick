// Minimal Arduino.h so the detection logic can be compiled and tested on a
// host machine. Only what detect.cpp, config.h and model.h actually reference
// is here -- this is a test shim, not an emulator, and nothing that touches
// hardware is compiled against it.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::int16_t;
using std::int32_t;
