# xiao_nrf52840-Environment-Transmitter
Arduino sketch for the Seeed Xiao nRF52840 board, a Adafruit BMP-390 Temp/Pressure sensor, a DHT11 Humidity/Temperature sensor, a MAX9814 microphone, and a ArduCam Mini-2MP-Plus Camera that can be triggered by and transfer data over BLE. This can be used in conjunction with the xiao_nrf52840-Environment-Host-App project (https://github.com/wodeeken/xiao_nrf52840-Environment-Host-App) to capture camera/audio data and to read in air monitor values. 


# Arduino IDE Notes
1. In Arduino IDE, use the following board manager: https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json.
2. The board manager used is the "Seeed nRF52 Boards" package (built with version 1.1.8).
3. You will need to install the ArduCAM library (built with version 1.0.0.0).
4. After installing the ArduCAM library, you must open the memorysaver.h file and uncomment those macros for the OV2640 camera (more information is located in the README here: https://github.com/ArduCAM/Arduino).
5. You will need to install the Adafruit BMP3XX Library (and all dependencies including Adafruit BusIO and Adafruit Unified Sensor - these should be installed automatically when installing BMP3XX library). This project was build with version 2.1.5.
6. You will need to install the Adafruit DHT sensor library (and dependency Adafruit Unified Sensor). This project was built with version 1.4.6.
7. This sketch uses Bluefruit for BLE implementation, which should not need installed if the proper board manager is selected.

# Hardware Details
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

3. The BMP-390 Sensor is wired to the XIAO nRF52840 according to the following table:

| BMP-390 | XIAO nRF52840 (Printed Labels) |
|---------|--------------------------------|
| GND     | GND                            |
| VCC     | 3v3                            |
| SDA     | 4                              |
| SCL     | 5                              |

4. The DHT11 Sensor is wired to the XIAO nRF52840 according to the following table:

| DHT11   | XIAO nRF52840 (Printed Labels) |
|---------|--------------------------------|
| GND     | GND                            |
| VCC     | 3v3                            |
| DATA    | 0                              |

5. The MAX9814 Microphone is wired to the XIAO nRF52840 according to the following table:

| MAX9814   | XIAO nRF52840 (Printed Labels)              |
|-----------|---------------------------------------------|
| GND       | GND                                         |
| VDD       | 3v3                                         |
| OUT       | 3                                           |
| GAIN      | 3v3 (Optional, audio sounds best @ 40dB)    |


# Usage
1. The following custom BLE services and characteristics are made available by this sketch:

| Service Description | Service UUID                         | Characteristic Description      | Characteristic UUID                  | Permissions | Max Data Length (bytes) |
|---------------------|--------------------------------------|---------------------------------|--------------------------------------|-------------|-------------------------|
| Camera              | e334b051-66f2-487f-8d55-9d2dda3a17cd |                                 |                                      |             |                         |
|                     |                                      | Camera Trigger/Data             | 9a82b386-3169-475a-935a-2394cd7a4d1d | Read/Write  | 244                     |
| Audio               | 8cdcb857-715e-4a72-aa99-8ed0c6b46d2a |                                 |                                      |             |                         |
|                     |                                      | Microphone Trigger/Data         | e9a68cde-2ac4-4cf4-b920-bf03786aee62 | Read/Write  | 244                     |
|                     |                                      | Audio Sample Rate (Hz)          | 0e70dcba-ced1-47bb-a269-af30eb979f12 | Read        | 2                       |
| Air Monitor         | 48ee2739-61c5-4594-b912-45a1646bef09 |                                 |                                      |             |                         |
|                     |                                      | Enclosure Temperature (Celsius) | 54eae144-c9b0-448e-9546-facb32a8bc75 | Read        | 2                       |
|                     |                                      | Air Pressure (hPa)              | 6d5c74ff-9853-4350-8970-456607fddcf8 | Read        | 2                       |
|                     |                                      | Relative Humidity (%)           | b2ecd36f-6730-45a8-a5fe-351191642c24 | Read        | 2                       |
|                     |                                      | Outer Temperature (Celsius)     | eec2eb81-ebb1-4352-8420-047304011fdb | Read        | 2                       |

2. The Air Monitor values are refreshed every few minutes.

3. The Camera Trigger/Data characteristic is used to both trigger the camera and to transfer image data in JPG format.

4. The Camera Trigger/Data characteristic should be used by the BLE Client/Host App in the following manner:
   
    a. BLE Client (Host App) writes the following values to the characteristic to trigger the camera: 0xFF,0xFF,0xFF,0xFF,0xFF,0x74,0x00,0x00.
   
    b. BLE Server (Xiao) will take the image, and then will write to characteristic two bytes in big-endian order representing the number of packets required to transfer the entire image (ceiling of Image Size / 244).
   
    c. BLE Client waits until a non-zero value in read in from characteristic, and then writes the values 0xFF,0xEF,0xDF,0xCF,0xBF,<CountHighByte>,<CountLowByte>,0x00,0x00, where <CountHighByte> and <CountLowByte> are the high and low bytes of the requested packet index between 0 and the value read from characteristic in the previous step.
   
    d. BLE Server writes to characteristic 244 bytes of the image data (possibly less if last packet) located between indices (requested packet * 244) and (requested packet * 244 + 243).
   
    e. BLE Client waits until image data is populated, saves data in the proper order, and then requests next packet if not already at the last packet.
   
    f. NOTE: the XIAO board will not prevent the client from triggering the camera or microphone before all packets have been requested. In this case, all previous image and audio data is overwritten and lost.

6. The Audio Trigger/Data characteristic is used to both trigger the microphone and to transfer ~12 seconds of raw audio data. The Audio Sample Rate characteristic contains the actual sample rate of the recording, which can be used by the server to playback the audio. 

7. The Audio Trigger/Data characteristic should be used by the BLE Client/Host App in the following manner:
   
    a. BLE Client (Host App) writes the following values to the characteristic to trigger the camera: 0xFF,0xFF,0xFF,0xFF,0xFF,0x75,0x00,0x00.
   
    b. BLE Server (Xiao) will begin to record audio, and after the recording is finished (roughly ~12 seconds) will write to characteristic two bytes in big-endian order representing the number of packets required to transfer the entire raw audio file (ceiling of Audio Size / 244). The audio file size is currently fixed at 200Kb, so the number of packets will always be 820.
   
    c. BLE Client waits until a non-zero value in read in from characteristic, and then writes the values 0xFF,0xEF,0xDF,0xCF,0xBF,<CountHighByte>,<CountLowByte>,0x00,0x00, where <CountHighByte> and <CountLowByte> are the high and low bytes of the requested packet index between 0 and the value read from characteristic in the previous step.
   
    d. BLE Server writes to characteristic 244 bytes of the audio data (possibly less if last packet) located between indices (requested packet * 244) and (requested packet * 244 + 243).
   
    e. BLE Client waits until audio data is populated, saves data in the proper order, and then requests next packet if not already at the last packet.
   
    f. BLE Server write the audio sample rate to the Audio Sample Rate characteristic after the recording is finished, measured in Hz.
   
    g. NOTE: the XIAO board will not prevent the client from triggering the microphone or camera before all packets have been requested. In this case, all previous image and audio data is overwritten and lost.
   


