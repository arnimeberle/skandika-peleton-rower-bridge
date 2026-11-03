# Skandika Rower Peloton APP FTMS Bridge

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
````cpp


Set SIMULATE to false to read the reed switch.
Set SIMULATE to true to test without the sensor.
Change REED_PIN if you use a different GPIO.
Adjust DEBOUNCE_US if the sensor bounces or valid strokes are missed.
METERS_PER_STROKE is an estimate. Calibrate it for your rower if you want the displayed distance to better match another device.
NOTIFY_INTERVAL controls the BLE notification interval.
The reported distance and power are estimates, not direct measurements. In particular, power is calculated from estimated speed and the configured distance per stroke.

## Build and upload firmware

Install the ESP32 board support package in the Arduino IDE.
Install the libraries listed under Software requirements.
Open firmware/SkandikaRowerFTMS/SkandikaRowerFTMS.ino.
Select your ESP32 board and its serial port.
Set SIMULATE to true for an initial sensor-free test, or false for the reed switch.
Compile and upload the sketch.
Open the Serial Monitor at 115200 baud.
Confirm that the ESP32 starts BLE advertising.
Scan for the device name configured in DEVICE_NAME.
BLE discovery and displayed metrics depend on the app and the FTMS features it supports.

## BLE services
The firmware is intended to expose these BLE services and characteristics:

Name	UUID	Purpose
Fitness Machine Service	0x1826	Main fitness-machine service
Rower Data	0x2AD1	Rower data notifications
Fitness Machine Control Point	0x2AD9	App control requests
Fitness Machine Status	0x2ADA	Machine status notifications
Training Status	0x2AD3	Training status
Device Information Service	0x180A	Device information
Manufacturer Name	0x2A29	Manufacturer string
Model Number	0x2A24	Model string
This is a DIY implementation and may not support every FTMS feature or control-point operation. An app may reject the device or display only a subset of its metrics.

## OLED values

The OLED can show the firmware's calculated stroke rate, stroke count, pace, distance, and power. Distance and power are estimated from detected strokes and the configured METERS_PER_STROKE value.

## Troubleshooting

The ESP32 does not appear in the app
Check that the ESP32 is powered and that the Serial Monitor reports advertising.
Confirm the app is scanning for compatible BLE fitness equipment.
Disconnect other devices or apps that may already be connected to the ESP32.
Check whether the app supports the FTMS Rower Data characteristic.
Stroke rate is too high or jumps
Confirm that the sensor generates one accepted event per stroke.
Check the sensor wiring and placement.
Adjust DEBOUNCE_US if contact bounce creates extra events.
Compare detected events and calculated intervals in the serial output.
Stroke rate remains at zero
Confirm SIMULATE is set to false when using the sensor.
Confirm the reed switch is wired between the configured GPIO and GND.
Check that the selected GPIO supports interrupts on your board.
Verify that the reed switch changes state while rowing.
Distance differs from another app or console
Distance is estimated from stroke count and METERS_PER_STROKE. Adjust that value only after comparing equivalent workouts and confirming the stroke count is accurate.

## Safety
Secure the ESP32, sensor, and wiring away from moving parts. Use the rower according to its manufacturer's instructions. This project is experimental and is not a replacement for the original console or safety systems.

## Contributing
Issues, corrections, and pull requests are welcome. When reporting a problem, include the rower model, console version if known, ESP32 board, app and operating-system versions, and relevant serial output.