#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#include <Wire.h>
#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <DHT.h>
// Chip select for ArduCAM.
const int CS = D6;
// Analog pin for humidity (DHT11)
const int DHT_AnalogPin = A0;
bool is_header = false;
int loopCount = 0;
// Camera
ArduCAM myCAM( OV2640, CS );
// Humidity Sensor.
DHT DHTSensor(DHT_AnalogPin, DHT11);
// Temp and Pressure Sensor.
Adafruit_BMP3XX bmp;
// Microphone pin.
int microphoneAnalogPin = A3;
// Custom Camera Service and Characteristic.
BLEService        cameraService = BLEService("e334b051-66f2-487f-8d55-9d2dda3a17cd");
BLECharacteristic cameraCharacteristic = BLECharacteristic("9a82b386-3169-475a-935a-2394cd7a4d1d");
// Custom Air Monitor Service and Characteristics.
BLEService        airMonitorService = BLEService("48ee2739-61c5-4594-b912-45a1646bef09");
BLECharacteristic temperatureCharacteristic = BLECharacteristic("54eae144-c9b0-448e-9546-facb32a8bc75");
BLECharacteristic pressureCharacteristic = BLECharacteristic("6d5c74ff-9853-4350-8970-456607fddcf8");
BLECharacteristic humidityCharacteristic = BLECharacteristic("b2ecd36f-6730-45a8-a5fe-351191642c24");
BLECharacteristic humidity_TempCharacteristic = BLECharacteristic("eec2eb81-ebb1-4352-8420-047304011fdb");

// Custom Audio Service and Characteristics.
BLEService        audioService = BLEService("8cdcb857-715e-4a72-aa99-8ed0c6b46d2a");
BLECharacteristic audioCharacteristic = BLECharacteristic("e9a68cde-2ac4-4cf4-b920-bf03786aee62");
BLECharacteristic audioSampleRateCharacteristic = BLECharacteristic("0e70dcba-ced1-47bb-a269-af30eb979f12");
BLEDis bledis;    // DIS (Device Information Service) helper class instance

// Store image and audio data here.
char Data[200000];
bool dataTransferProcess = false;
int imageLength = 0;
// Max number of loop counts from beginning of camera/audio data where there are no loop waits.
const int maxDataTransferWaitCount = 3000000;
/// GENERAL Camera/audio WORKFLOW:
/// 1. BLE Client writes value 0xFF,0xFF,0xFF,0xFF,0xFF,0x74/0x75,0x00,0x00 value to Camera / Audio Characteristic to trigger camera/microphone.
/// 2. Board writes total count of image packets ceil(Image Length / 512) to Camera Characteristic in format 0xFF,0xEF,0xDF,0xCF,0xBF,<Count: 0 to 255>,0x00,0x00.
/// 3. BLE Client writes a integer between 0 and ceiling(Image Length / 512) to get a packet of image data.
/// 4. Board maintains no state. If the trigger command is received during image transfer, the existing image is overwritten.

/// Other data is periodically sampled and written to the relevant characteristic.
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

  Serial.println("Now setting up Air Pressure/Temperature monitor.");
  while (!bmp.begin_I2C()) { 
    Serial.println("Could not find BMP.");
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  Serial.println("Air Pressure/Temperature monitor setup complete.");
  
  Serial.println("Now setting up Humidity monitor.");
  DHTSensor.begin();
  Serial.println("Humidity monitor setup complete.");

  Serial.println("Now setting up BLE and its services.");

  Serial.println("Now setting up microphone");
  pinMode(microphoneAnalogPin, INPUT);
  Serial.println("Microphone setup is complete.");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.setTxPower(8);
  Bluefruit.begin();
  Bluefruit.setName("XIAO_MONITOR");
  // Set the connect/disconnect callback handlers
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  
  // Configure and Start the Device Information Service
  Serial.println("Now configuring the Device Information Service");
  bledis.setManufacturer("MyManufacturer");
  bledis.setModel("MyBoard");
  bledis.begin();
  Serial.println("Device Information Service setup is complete.");
  
  Serial.println("Now configuring the custom Camera Service and Characteristic.");
  cameraService.begin();
  cameraCharacteristic.setProperties(CHR_PROPS_WRITE | CHR_PROPS_READ);
  cameraCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  cameraCharacteristic.setFixedLen(244);
  cameraCharacteristic.begin();
  Serial.println("Camera Service and Characteristic setup is complete.");

  Serial.println("Now configuring the custom Air Monitor Service and Characteristics");
  airMonitorService.begin();
  temperatureCharacteristic.setProperties(CHR_PROPS_READ);
  temperatureCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  temperatureCharacteristic.begin();
  pressureCharacteristic.setProperties(CHR_PROPS_READ);
  pressureCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  pressureCharacteristic.begin();
  humidityCharacteristic.setProperties(CHR_PROPS_READ);
  humidityCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  humidityCharacteristic.begin();
  humidity_TempCharacteristic.setProperties(CHR_PROPS_READ);
  humidity_TempCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  humidity_TempCharacteristic.begin();
  Serial.println("Air Monitor Service and Characteristic setup is complete.");

  Serial.println("Now configuring custom Audio Service and Characteristics");
  audioService.begin();
  audioCharacteristic.setProperties(CHR_PROPS_WRITE | CHR_PROPS_READ);
  audioCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  audioCharacteristic.setFixedLen(244);
  audioCharacteristic.begin();
  audioCharacteristic.write8(0);
  audioSampleRateCharacteristic.setProperties(CHR_PROPS_READ);
  audioSampleRateCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  audioSampleRateCharacteristic.begin();
  Serial.println("Audio Service and Characteristic setup is complete.");

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
  // Increase the bandwidth.
  connection->requestPHY();
  connection->requestDataLengthUpdate();
  connection->requestMtuExchange(247); 
  Serial.println("All requests send. Now waiting 30 secs.");
  delay(3000);
  
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
  if(!dataTransferProcess){
    // Wait 10 minutes to save energy.
    ReadEnvironmentSensors();
    myCAM.set_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);
    delay(570000);
    myCAM.clear_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);
    delay(30000);
  }else{
    if(loopCount > maxDataTransferWaitCount){
      // Stop the data transfer process.
      dataTransferProcess = false;
    }
  }
  if ( Bluefruit.connected() ) {
      char buffer[20];
      // Are values == camera trigger sequence? (0xFF,0xFF,0xFF,0xFF,0xFF,0x74,0x00,0x00)
      if (cameraCharacteristic.read(&buffer, 8) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xFF &&
          buffer[2] == 0xFF &&
          buffer[3] == 0xFF &&
          buffer[4] == 0xFF &&
          buffer[5] == 0x74 && 
          buffer[6] == 0x00 &&
          buffer[7] == 0x00 ){
      // Clear out whole cameradata array.
      for(int i = 0; i < 200000; i++)
        Data[i] = 0;
      
      dataTransferProcess = true;
      loopCount = 0;
      Serial.println("Triggering camera.");
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
        short totalPacketCount = ceil((float)imageLength/244);
        Serial.println(totalPacketCount);
        char packetCount[2] = { totalPacketCount >> 8, totalPacketCount & 0xFF};
        Serial.print("Upper byte:");Serial.println(packetCount[0], HEX);
        Serial.print("Low byte:");Serial.println(packetCount[1], HEX);
        cameraCharacteristic.write(packetCount, 2);
  // Are values == audio trigger sequence? (0xFF,0xFF,0xFF,0xFF,0xFF,0x75,0x00,0x00)
  }else if(audioCharacteristic.read(&buffer, 8) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xFF &&
          buffer[2] == 0xFF &&
          buffer[3] == 0xFF &&
          buffer[4] == 0xFF &&
          buffer[5] == 0x75 && 
          buffer[6] == 0x00 &&
          buffer[7] == 0x00 ){
        
        dataTransferProcess = true;
        loopCount = 0;
        RecordAudio();
  // Are values == camera packet count? (0xFF,0xEF,0xDF,0xCF,0xBF,<CountHighByte>,<CountLowByte>,0x00,0x00)
  }else if(cameraCharacteristic.read(&buffer, 9) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xEF &&
          buffer[2] == 0xDF &&
          buffer[3] == 0xCF &&
          buffer[4] == 0xBF &&
          buffer[7] == 0x00 &&
          buffer[8] == 0x00){
        int requestedPacketNum = (buffer[5] << 8) | buffer[6];
        Serial.print("Write packet #");
        Serial.println(requestedPacketNum, DEC);
        char writeBuffer[244];
        int  y = 0;
        for(int i = requestedPacketNum * 244; i < (requestedPacketNum * 244) + 244; i++){
          //Serial.print("0x");Serial.print(Data[i], HEX);Serial.println(",");
          writeBuffer[y] = Data[i];
          y++;
        }
        cameraCharacteristic.write(writeBuffer, 244);
  // Are values == camera packet count? (0xFF,0xEF,0xDF,0xCF,0xBF,<CountHighByte>,<CountLowByte>,0x00,0x00)
  }else if(audioCharacteristic.read(&buffer, 9) && 
          buffer[0] == 0xFF && 
          buffer[1] == 0xEF &&
          buffer[2] == 0xDF &&
          buffer[3] == 0xCF &&
          buffer[4] == 0xBF &&
          buffer[7] == 0x00 &&
          buffer[8] == 0x00){
        int requestedPacketNum = (buffer[5] << 8) | buffer[6];
        Serial.print("Write packet #");
        Serial.println(requestedPacketNum, DEC);
        char writeBuffer[244];
        int  y = 0;
        for(int i = requestedPacketNum * 244; i < (requestedPacketNum * 244) + 244; i++){
          //Serial.print("0x");Serial.print(Data[i], HEX);Serial.println(",");
          writeBuffer[y] = Data[i];
          y++;
        }
        audioCharacteristic.write(writeBuffer, 244);
  
  }
  }
  
  loopCount++;

}
void RecordAudio(){
  int currentAudioIndex = 0;
  int beginTime = micros();
  Serial.println("Begin audio record.");
  while(currentAudioIndex < 200000){

    int16_t sample = analogRead(microphoneAnalogPin);
    sample = sample << 6;
    Data[currentAudioIndex] = sample >> 8;
    Data[currentAudioIndex + 1] = sample & 0xFF;
    currentAudioIndex += 2;
    delayMicroseconds(100);
  }
  int endTime = micros();
  Serial.println("Audio record done.");
  Serial.print("Length of sample in micros: ");Serial.println(beginTime - endTime);
  Serial.print("In Seconds: ");Serial.println(((float)(endTime - beginTime)) / 1000000);
  int sampleRate = 100000 / (((float)(endTime - beginTime)) / 1000000);
  Serial.print("Audio Sample Rate: ");Serial.print(sampleRate);
  // Packet count = (Data Array size = 200000) / (Characteristic size = 244 bytes) = 820
  char packetCount[2] = { 820 >> 8, 820 & 0xFF};
  audioCharacteristic.write(packetCount, 2);
  // write sample rate.
  packetCount[0] = sampleRate >> 8;
  packetCount[1] = sampleRate & 0xFF;
  audioSampleRateCharacteristic.write(packetCount, 2);
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
      Data[index] = temp;
      index++;
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      Data[index]  = temp_last;
      index++;
      Data[index] = temp;
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
void ReadEnvironmentSensors(){
    Serial.println("Time to read air monitor.");
    // Perform Air Monitor Readings.
    if(!bmp.performReading()){
      Serial.println("Failed to perform temperature and air pressure reading.");
    }
    int temperature = ceil(bmp.temperature);
    int airPressure = ceil(bmp.pressure / 100);
    Serial.print("Temp: ");
    Serial.println(bmp.temperature);
    Serial.print("Pressure: ");
    Serial.println(bmp.pressure);
    char tempReading[2] = {temperature >> 8, temperature & 0xFF};
    char pressureReading[2] = {airPressure >> 8, airPressure & 0xFF};
    temperatureCharacteristic.write(tempReading, 2);
    pressureCharacteristic.write(pressureReading, 2);

    // Read humidity reading.
    float humidity_true = DHTSensor.readHumidity();
    float humidity_temp_true = DHTSensor.readTemperature();
    int humidity = ceil(humidity_true);
    int humidity_temp = ceil(humidity_temp_true);
    char humidityReading[2] = {humidity >> 8, humidity & 0xFF};
    char tempHumidityReading[2] = {humidity_temp >> 8, humidity_temp & 0xFF};
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.print("Temp used for Humidity calc: ");
    Serial.println(humidity_temp);
    humidityCharacteristic.write(humidityReading, 2);
    humidity_TempCharacteristic.write(tempHumidityReading, 2);

  }
