# Skandika Rower FTMS Bridge

An ESP32-based DIY project that reads rowing strokes from a reed switch and broadcasts estimated rowing data over Bluetooth Low Energy using the Fitness Machine Service (FTMS).

The project is intended for rowers whose standard console does not provide the data connection required by a training app. It reads the reed-switch signal independently; it does not connect to, modify, or replace the rower's original console.

## Compatibility note

On the Skandika rower and standard console tested for this project, the console did not connect to the Peloton App or Zwift. A Bluetooth connection alone does not guarantee that a device exposes the fitness data and protocol expected by an app.

This project provides a separate ESP32-based FTMS BLE device. App compatibility is not guaranteed and can depend on the rower model, app version, operating system, and the FTMS features supported by the app.

## Disclaimer

This wiring guide is provided for informational purposes only. The project author provides no warranty or guarantee regarding the wiring instructions, hardware compatibility, or fitness for a particular purpose. You are responsible for checking your components and connections before powering the device. The project author is not liable for damage, injury, or loss resulting from the use of this guide or project.

## Features

- ESP32 reads stroke events from a reed switch.
- Firmware estimates stroke rate, stroke count, distance, pace, and power.
- BLE advertises the Fitness Machine Service, UUID `0x1826`.
- Rower Data notifications use characteristic UUID `0x2AD1`.
- Simulation mode is available for testing without a reed switch.
- Optional SSD1306 OLED displays workout metrics.
- Serial output helps with setup and troubleshooting.

## How it works

The ESP32 detects reed-switch events and measures the time between accepted events. Stroke rate is estimated from the interval between strokes. Distance is estimated using a configurable distance-per-stroke value.

The device then sends rowing data using BLE FTMS. A compatible app may discover the ESP32 as a fitness machine and use the data it supports.

The firmware assumes that one accepted reed-switch event represents one rowing stroke. Incorrect sensor placement, contact bounce, or multiple events per stroke can result in inaccurate metrics.

## Hardware

- ESP32 development board
- Reed switch or equivalent contact sensor
- Connecting wires
- Optional SSD1306 128 × 32 I2C OLED
- USB cable for programming and serial logging

Current firmware pin configuration:

| Signal | ESP32 pin |
|---|---:|
| Reed switch | GPIO 4 |
| Reed switch ground | GND |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| OLED I2C address | `0x3C` |

The reed switch is configured with the ESP32 internal pull-up. Connect the switch between GPIO 4 and GND. Check your specific ESP32 board's pinout before wiring.

See [`docs/wiring.md`](docs/wiring.md) for the wiring notes.

## Software requirements

- Arduino IDE or another ESP32 Arduino build environment
- ESP32 Arduino core
- NimBLE-Arduino
- Adafruit GFX Library
- Adafruit SSD1306 Library

Install libraries through the Arduino IDE Library Manager or the package manager used by your development environment.

## Configuration

Review the configuration constants near the top of the sketch before compiling. Common settings include:

```cpp
#define SIMULATE          false
#define REED_PIN          4
#define DEBOUNCE_US       300000UL
#define STROKE_TIMEOUT_MS 5000UL
#define METERS_PER_STROKE 9.0f
#define DEVICE_NAME       "ESP32 Rower"
#define NOTIFY_INTERVAL   1000