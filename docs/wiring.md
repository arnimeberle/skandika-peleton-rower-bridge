# Wiring

## Reed contact

The reed contact detects rowing strokes. Connect it between ESP32 GPIO 4 and GND.

| Reed contact | ESP32 |
|---|---|
| One contact | GPIO 4 (`REED_PIN`) |
| Other contact | GND |

The firmware uses the ESP32's internal pull-up. No external voltage should be applied to the reed-contact input.

Position the reed contact and magnet so that the contact produces one clean event per rowing stroke. Secure the sensor and wiring away from moving parts.

## Optional SSD1306 OLED

The firmware uses an SSD1306 OLED over I2C.

| OLED pin | ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | Supply voltage supported by the OLED module |

The configured I2C address is `0x3C`. Check the voltage requirements and pin labels of your specific OLED module before connecting it.

## Notes

- Confirm the pinout of your specific ESP32 board before wiring.
- Keep all connections secure and insulated.
- Do not connect the reed-contact input to 3.3 V or 5 V; the switch connects GPIO 4 to GND.
- Test the sensor in simulation mode or with serial logging before using the rower.

## Disclaimer

This wiring guide is provided for informational purposes only. The project author provides no warranty or guarantee regarding the wiring instructions, hardware compatibility, or fitness for a particular purpose. You are responsible for checking your components and connections before powering the device. The project author is not liable for damage, injury, or loss resulting from the use of this guide or project.