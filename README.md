# xiao_nrf52840-ArduCam-Transmitter
October 6, 2024 - WORK IN PROGRESS
Arduino sketch for the Seeed Xiao nRF52840 board and the ArduCam Mini-2MP-Plus Camera that can be triggered by and transfer image data over BLE. 

# Arduino IDE Notes
1. In Arduino IDE, use the following board manager: https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json.
2. The board manager used is the "Seeed nRF52 Boards" package (built with version 1.1.8).
3. You will need to install the ArduCAM library (built with version 1.0.0.0).
4. After installing the ArduCAM library, you must open the memorysaver.h file and uncomment those macros for the OV2640 camera (more information is located in the README here: https://github.com/ArduCAM/Arduino).
5. This sketch uses Bluefruit for BLE implementation, which should not need installed if the proper board manager is selected.

# Usage
1. This sketch was built for the XIAO nRF52840 board, though any board that is supported by the Adafruit Bluefruit library should work with minimal adjustments.
2. The ArduCAM is wired to the XIAO nRF52840 board according to the following table:

| ArduCAM | XIAO nRF52840 (Printed Labels) |
|---------|--------------------------------|
| CS      | 1                              |
| MOSI    | 10                             |
| MISO    | 9                              |
| SCK     | 8                              |
| GND     | GND                            |
| VCC     | 3v3                            |
| SDA     | 4                              |
| SCL     | 5                              |

