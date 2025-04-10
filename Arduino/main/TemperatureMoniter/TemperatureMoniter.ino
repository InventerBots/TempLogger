#include <SparkFun_Qwiic_OLED.h>
#include <res/qw_fnt_8x16.h>

#include <Wire.h>
#include <String.h>
#include <WiFi.h>

#define AHT20_I2C_ADDRESS 0x38
#define ANALOG_RES 12

#define DISPLAY_RATE 1000
#define TEMP_POL_RATE 250

#define ENABLE_SERIAL false

float analogCh1_ofset = 1.01;
float analogCh2_ofset = 1.01;
float analogCh3_ofset = 1.01;

int analogNC = -50;

char ssid[] = "Foe-Glass wifi";
char password[] = "Wizard_4 wand";

String systemStat;
unsigned long sysTime;
unsigned long lastDispTime;
unsigned long lastPolTime;

uint8_t l1_posX = 0, l1_posY = 0;
uint8_t l2_posX = 0, l2_posY = 15;
uint8_t l3_posX = 0, l3_posY = 30;
uint8_t l4_posX = 0, l4_posY = 45;

int srvrStat = WL_IDLE_STATUS;
uint16_t srvrPort = 8000;
IPAddress ip_adr;
String ip_str;

Qwiic1in3OLED disp;
WiFiServer server(srvrPort);

void setup() {
  // Configure I2C bus
  Wire.setSCL(17);
  Wire.setSDA(16);
  Wire.begin();

  analogReadResolution(12);

  // Attempt to open serial port
  #if ENABLE_SERIAL
  Serial.begin(115200);
  if(Serial.available()) {
    systemStat = "Serial connected";
    Serial.println("Serial connected");
  }
  #endif

  // Initalize OLED
  if(!disp.begin()) {
    while(true);
  }


  disp.text(l1_posX, l1_posY, "Connecting to Wifi");
  disp.display();

  // Configure Wifi Server
  srvrStat = WiFi.begin(ssid, password);
  if (srvrStat == WL_CONNECTED) {

    ip_adr = WiFi.localIP();
    ip_str = ip_adr.toString();

    disp.erase();
    disp.text(l1_posX, l1_posY, "Connected");
    disp.text(l2_posX, l2_posY, ip_str);
    disp.display();
    delay(500);
  }

  // Serial.println(systemStat);
  disp.text(l1_posX, l1_posY, systemStat);
  disp.display();
  delay(500);
  disp.erase();

  if(initializeAHT20()) {
    systemStat = "AHT20 initalized";
  } else {
    systemStat = "AHT20 not found";
  }
  // Serial.println(systemStat);
  disp.text(l1_posX, l1_posY, systemStat);
  disp.display();
  delay(500);
  disp.erase();
  disp.setFont(QW_FONT_8X16);

}

void loop() {
  float temperature, humidity;

  String l1_text = "Temp A: ";
  String l2_text = "RH: ";
  String l3_text = "Temp 1: ";
  String l4_text = "Temp 2: ";

  sysTime = millis();

  float ntcCh1 = convertTemp_F(analogRead(A0)*analogCh2_ofset);
  float ntcCh2 = convertTemp_F(analogRead(A1)*analogCh1_ofset);
  if (ntcCh1 > analogNC) {
    l3_text+=String(ntcCh1);
  } else {
    l3_text+="--.--";
  }
  if(ntcCh2 > analogNC) {
    l4_text+=String(ntcCh2);
  } else {
    l4_text+="--.--";
  }
  

  if(readAHT20(&temperature, &humidity)) {
    l1_text+=String(temperature*1.8+32)+"F";
    l2_text+=String(humidity)+"%";

  } else {
    l1_text+="Error";
    l2_text+="Error";
  }

  if(sysTime > lastDispTime+DISPLAY_RATE) {
    lastDispTime = sysTime; 
    disp.erase();

    disp.text(l1_posX, l1_posY, l1_text);
    disp.text(l2_posX, l2_posY, l2_text);
    disp.text(l3_posX, l3_posY, l3_text);
    disp.text(l4_posX, l4_posY, l4_text);
    disp.display();

    #if ENABLE_SERIAL
    Serial.print("voltage: ");
    Serial.println(((float)analog/4096)*3.3);
    Serial.print("R2: ");
    Serial.println(convertTemp_F(analog));
    Serial.print("raw: ");
    Serial.println(analog);
    #endif
  }      
}

float convertTemp_F(uint16_t AInput) {
  const float c1 = 1.009249522e-03;
  const float c2 = 2.378405444e-04;
  const float c3 = 2.019202697e-07;
  const float R1 = 10000;
  float R2 = R1 * (float)(pow(2, ANALOG_RES)/AInput-1);
  float tempK = 1 / (c1 + c2 * log(R2) + c3 * pow(log(R2), 3));
  return (tempK - 273.15) * 9/5 + 32;
  // return tempK;
}

bool initializeAHT20() {
  bool status = false;
  Wire.beginTransmission(AHT20_I2C_ADDRESS);
  Wire.write(0xBE); // Initialization command
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() == 0) {
    status = true;
  } else {
    status = false;
  }
  delay(10);
  return status;
}

bool readAHT20(float *temperature, float *humidity) {
  // Send the measurement command
  Wire.beginTransmission(AHT20_I2C_ADDRESS);
  Wire.write(0xAC); // Trigger measurement command
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false; // Error in I2C communication
  }

  delay(80); // Wait for the measurement to complete

  // Request 6 bytes of data from the sensor
  Wire.requestFrom(AHT20_I2C_ADDRESS, 6);
  if (Wire.available() != 6) {
    return false; // Not enough data received
  }

  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  // Check if the sensor is busy (status bit 7 == 1)
  if (data[0] & 0x80) {
    return false;
  }

  // Convert the raw data to temperature and humidity
  uint32_t rawHumidity = ((uint32_t)(data[1] & 0xF0) << 12) | ((uint32_t)data[2] << 8) | data[3];
  uint32_t rawTemperature = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

  *humidity = ((float)rawHumidity / 1048576.0) * 100.0;
  *temperature = ((float)rawTemperature / 1048576.0) * 200.0 - 50.0;

  return true;
}
