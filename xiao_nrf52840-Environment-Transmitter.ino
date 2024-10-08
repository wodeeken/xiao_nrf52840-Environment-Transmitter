#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#include <Wire.h>
#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>
#include <math.h>
const int CS = D1;
bool is_header = false;
int loopCount = 0;
ArduCAM myCAM( OV2640, CS );

// Setup Custom Camera Service and Characteristic.
BLEService        cameraService = BLEService("e334b051-66f2-487f-8d55-9d2dda3a17cd");
BLECharacteristic cameraCharacteristic = BLECharacteristic("9a82b386-3169-475a-935a-2394cd7a4d1d");

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas batteryService;    // Battery service

// Store image data here.
char CameraData[150000];
int imageLength = 0;
/// GENERAL WORKFLOW:
/// 1. BLE Client writes value 0xFF,0xFF,0xFF,0xFF,0xFF,0x74,0x00,0x00 value to Camera Characteristic to trigger camera.
/// 2. Board writes total count of image packets ceil(Image Length / 512) to Camera Characteristic in format 0xFF,0xEF,0xDF,0xCF,0xBF,<Count: 0 to 255>,0x00,0x00.
/// 3. BLE Client writes a integer between 0 and ceiling(Image Length / 512) to get a packet of image data.
/// 4. Board maintains no state. If the trigger command is received during image transfer, the existing image is overwritten.
void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(9600);
  // Setup ArduCAM
  Serial.println("Now setting up ArduCAM");
  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin();

  SPI.begin();
  
  pinMode(CS, OUTPUT);

  delay(1000);
  digitalWrite(CS, LOW);
  SPI.begin();
  
  //Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  while(1){
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55){
      Serial.println(temp);
      Serial.println(F("ACK CMD SPI interface Error! END"));
      delay(1000);
      continue;
    }else{
      Serial.println(temp);
      Serial.println(F("ACK CMD SPI interface OK. END"));
      break;
    }
  }
 
  while(1){
    Serial.println("Loop!");
    delay(2000);
    //Check if the camera module type is OV2640
    myCAM.wrSensorReg8_8(0xff, 0x01);
    
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
   
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      Serial.println(F("ACK CMD Can't find OV2640 module! END"));
      delay(1000);
      continue;
    }
    else{
      Serial.println(F("ACK CMD OV2640 detected. END"));
      break;
    } 
  }
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_1600x1200);
  delay(1000);
  myCAM.clear_fifo_flag();
  Serial.println("ArduCAM setup complete.");
  Serial.println("Now setting up BLE and its services.");

  
  Bluefruit.begin();
  Bluefruit.setName("ArduCAM");
  // Set the connect/disconnect callback handlers
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  
  // Configure and Start the Device Information Service
  Serial.println("Now configuring the Device Information Service");
  bledis.setManufacturer("MyManufacturer");
  bledis.setModel("MyBoard");
  bledis.begin();
  Serial.println("Device Information Service setup is complete.");
  Serial.println("Now configuring the Battery Service");
  batteryService.begin();
  batteryService.write(100);
  Serial.println("Battery Service setup is complete.");
  
  Serial.println("Now configuring the custom Camera Service and Characteristic.");
  cameraService.begin();
  cameraCharacteristic.setProperties(CHR_PROPS_WRITE | CHR_PROPS_READ);
  cameraCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  cameraCharacteristic.notifyEnabled(true);
  //cameraCharacteristic.setFixedLen(1);
  cameraCharacteristic.begin();
  Serial.println("Camera Service and Characteristic setup is complete.");
  // Setup the advertising packet(s)
  Serial.println("Now setting up BLE Advertising.");
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include Camera Service.
  Bluefruit.Advertising.addService(cameraService);
  Bluefruit.Advertising.addName();
  
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); 
  Bluefruit.Advertising.setFastTimeout(30);      
  Bluefruit.Advertising.start(0);             

  Serial.println("Advertising setup is complete.");
  Serial.println("Setup and configuration is complete.");
}
void connect_callback(uint16_t conn_handle)
{
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);
}


void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  Serial.println("Resuming Advertising.");
}
void loop() {
  // Read bat level only every ~30 seconds.
  if(loopCount % 30 == 0){
    Serial.println("Time to read battery.");
    int vbatt = analogRead(PIN_VBAT);
    float adcVoltage = 2.961 * 3.6 * vbatt / 4096;
    //very rough assumptions: assume full bat = 3.6V,  dead battery = 2 V.
    int batteryLevel = (2.25 * (adcVoltage - 2)/3.6) * 100;
    Serial.print("Battery level: ");Serial.println(batteryLevel);
    batteryService.write(batteryLevel);

  }
  delay(1000);
  String myInput = Serial.readString();
  myInput.trim();
  if ( Bluefruit.connected() ) {
      char buffer[512];
      // Are values == trigger sequence? (0xFF,0xFF,0xFF,0xFF,0xFF,0x74,0x00,0x00)
      if (cameraCharacteristic.read(&buffer, 8) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xFF &&
          buffer[2] == 0xFF &&
          buffer[3] == 0xFF &&
          buffer[4] == 0xFF &&
          buffer[5] == 0x74 && 
          buffer[6] == 0x00 &&
          buffer[7] == 0x00 ){
      // Zero out characteristic.
      Serial.println("Triggering camera.");
      cameraCharacteristic.write8(0);
       // Trigger camera
        myCAM.flush_fifo();
        myCAM.clear_fifo_flag();
        //Start capture
        delay(250);
        myCAM.start_capture();
        delay(250);
        if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
        {
          delay(50);
          read_fifo_burst(myCAM);
          //Clear the capture done flag
          myCAM.clear_fifo_flag();
        }
        // Write image length.
        Serial.println("Image length: ");
        Serial.println(imageLength);
        Serial.println("Packet Count: ");
        Serial.println(ceil((float)imageLength/512));
        cameraCharacteristic.write8(ceil((float)imageLength/512));
  // Are values == packet count? (0xFF,0xEF,0xDF,0xCF,0xBF,<Count>,0x00,0x00)
  }else if(cameraCharacteristic.read(&buffer, 8) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xEF &&
          buffer[2] == 0xDF &&
          buffer[3] == 0xCF &&
          buffer[4] == 0xBF &&
          buffer[6] == 0x00 &&
          buffer[7] == 0x00){
        Serial.print("Write packet #");
        Serial.println(buffer[5], DEC);
        char writeBuffer[512];
        int  y = 0;
        for(int i = buffer[5] * 512; i < (buffer[5] * 512) + 512; i++){
          Serial.print("0x");Serial.print(CameraData[i], HEX);Serial.println(",");
          writeBuffer[y] = CameraData[i];
          y++;
        }
        cameraCharacteristic.write(writeBuffer, 512);
  
  }
  }
  Serial.print("Loop Count: ");Serial.println(loopCount);
  delay(1000);
  loopCount++;

}
uint8_t read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  length = myCAM.read_fifo_length();
  if (length >= MAX_FIFO_SIZE) //512 kb
  {
    Serial.println(F("ACK CMD Over size. END"));
    return 0;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println(F("ACK CMD Size is 0. END"));
    return 0;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  temp =  SPI.transfer(0x00);
  length --;
  int index = 0;
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    if (is_header == true)
    {
      //Serial.print("0x");
      //Serial.print(temp, HEX);
      //Serial.write(",");
      CameraData[index] = temp;
      index++;
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      CameraData[index]  = temp_last;
      index++;
      CameraData[index] = temp;
      index++;
      // Serial.print("0x");
      // Serial.print(temp_last, HEX);
      // Serial.print(",");
      // Serial.print("0x");
      // Serial.print(temp, HEX);
      // Serial.print(",");
    }
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    break;
    delayMicroseconds(15);
  }
  myCAM.CS_HIGH();
  is_header = false;
  imageLength = index;
  return 1;
}