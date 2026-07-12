# Fall-detection walking stick

Assistive cane that detects a fall, sounds a buzzer, and messages a caretaker
on Telegram.

Two boards: an Arduino Nano reads the MPU6050 and does the detection, an
ESP8266 does Wi-Fi. They talk over a UART link. The split matters: a TLS handshake
blocks for seconds, and it can't sit between the accelerometer and the buzzer.

## Notes on the AVR side

The ATmega328P has 2 KB of SRAM and no FPU, which shapes most of the code:

- No windowing. A 2 s window at 100 Hz is 200 × 6 × 2 = 2400 bytes, which
  doesn't fit. Detection is a state machine over running extrema instead, and
  `sizeof(Detector)` is 31 bytes regardless of event length.
- No `sqrt`. `|a| < 0.5 g` is tested as `ax²+ay²+az² < (0.5*2048)²`. Soft-float
  `sqrt` is ~1500 cycles, the compare is ~20.
- No floats in the decision path at all. Features are squared counts and
  `export_tree.py` rounds thresholds to `uint32`, so soft-float never links in.
- Stillness is `max - min` rather than a variance, which avoids a division.
- No malloc. `freeRam()` (`s` over serial) should read the same after an hour
  as at boot.

`uint32_t` matters here: at ±16 g a saturated axis is 32767 counts, and
3 × 32767² = 3.2e9, which overflows `int32_t`.

## Detection

Four states: free-fall, impact, then stillness.

| State | Exits when | Rejects |
|---|---|---|
| `IDLE` | `\|a\|²` below 0.5 g | |
| `FREEFALL` | `\|a\|²` recovers | <80 ms (knock), >800 ms (lift) |
| `IMPACT_WAIT` | `\|a\|²` above 2.5 g | no impact in 400 ms (stick lowered) |
| `SETTLING` | 1 s elapsed | |

An event yields four features (free-fall depth, impact peak, post-impact
range, gyro peak) and a depth-4 tree in `model.h` decides. The first 300 ms
after impact is skipped, otherwise the stillness measurement picks up the
impact itself.

`model.h` currently holds hand-set defaults (0.5 g / 2.5 g literature values),
not a fitted tree. Refit before quoting numbers.

## Wiring

| Nano | To | Notes |
|---|---|---|
| A4 / A5 | MPU6050 SDA / SCL | |
| 5V, GND | MPU6050 VCC, GND, AD0 | AD0 low = address 0x68 |
| D8 | buzzer transistor | not the buzzer directly |
| D10 | ESP8266 TX | |
| D11 | ESP8266 RX | via divider, see below |
| D13 | LED | |

The ESP8266 is not 5 V tolerant, so D11 needs a divider (1 kΩ series, 2 kΩ to
ground). The other direction is fine, 3.3 V clears the AVR's 3.0 V threshold.
Not much margin but it works.

Buzzer draws 25-30 mA, over the 20 mA an AVR pin should source, so drive it
through a BC337/2N2222 with a 1 kΩ base resistor.

The Nano's hardware UART is taken by USB, so the ESP8266 link is
SoftwareSerial at 9600. Above ~38400 it starts dropping bits.

## Link protocol

```
$FALL,<seq>,<peak>,<ffms>*<xor>    Nano -> ESP8266
$ACK,<seq>*<xor>                   ESP8266 -> Nano
```

XOR over everything between `$` and `*`. The Nano retries 4× at 1.5 s and then
gives up; the buzzer has already sounded by then.

The ESP8266 ACKs before making the HTTPS call, not after. A TLS handshake
outlasts the retry interval, so ACKing after would mean duplicate alerts.
Dedup is by sequence number.

## Building

- `firmware/nano/` builds for the Arduino Nano, no external libraries
- `firmware/esp8266/` needs the ESP8266 core. Copy `secrets.example.h` to `secrets.h`
  and fill it in (gitignored)

## Pipeline

```bash
python -m venv .venv && .venv/bin/pip install -r pipeline/requirements.txt
```

Fit the tree from a public dataset (SisFall, MobiFall, UMAFall all work once
reshaped to one CSV per trial, six columns of raw counts, filenames starting
`fall`):

```bash
cd pipeline
python fit_model.py --data <dir> --out model.pkl
python export_tree.py --model model.pkl --out ../firmware/nano/model.h
```

Fall trials the state machine never triggered on are reported separately from
the tree's errors, since those are misses no classifier can fix.

To replay a capture (press `l` over serial to dump raw counts):

```bash
python simulate.py --csv capture.csv
```

## Tests

```bash
.venv/bin/python tools/host_parity/run_parity.py
```

Compiles `detect.cpp` for the host and checks it produces the same events as
`pipeline/features.py`. They have to match, since the tree is fitted on the
Python features and runs against the C ones. This caught the stillness window
including its own impact spike.

## Layout

```
firmware/nano/       sampling, detection, buzzer
  nano.ino           main loop, serial commands
  config.h           pins, thresholds, timings
  mpu6050.{h,cpp}    I2C, squared magnitudes
  detect.{h,cpp}     state machine
  model.h            decision tree (regenerate with export_tree.py)
  link.{h,cpp}       UART protocol
firmware/esp8266/    Wi-Fi and Telegram
pipeline/            fitting, export, replay
tools/host_parity/   C vs Python check
```

Serial (115200): `s` status, `l` log raw CSV, `t` test alert.
