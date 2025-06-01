#include <SparkFun_Qwiic_OLED.h>
#include <res/qw_fnt_8x16.h>

extern "C" {
  #include "lwip/dhcp.h"
  #include "lwip/netif.h"
  #include "lwip/ip_addr.h"
}

#include <Wire.h>
#include <SPI.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SD.h>
#include <ArduinoJson.h>

#define AHT20_I2C_ADDRESS 0x38

#define ANALOG_RES 12

#define DISPLAY_RATE 1000
#define TEMP_POL_RATE 250

const char* ssid     = "Foe-Glass wifi";
const char* password = "Wizard_4 wand";

unsigned long sysTime;
unsigned long lastDispTime;
unsigned long lastPolTime;

uint8_t l1_posX = 0, l1_posY = 0;
uint8_t l2_posX = 0, l2_posY = 15;
uint8_t l3_posX = 0, l3_posY = 30;
uint8_t l4_posX = 0, l4_posY = 45;

int analogNC = -50;

// Define analog input pins
const uint8_t tempCH0_pin = A0;
const uint8_t tempCH1_pin = A1;
const uint8_t tempCH2_pin = A2;

// Define digital output pins
const uint8_t ch0stat_pin = 10;
const uint8_t ch1stat_pin = 11;
const uint8_t ch2stat_pin = 12;

// Define SD card SPI bus pins
const uint8_t SD_MISO = 4;
const uint8_t SD_MOSI = 3;
const uint8_t SD_CS = 5;
const uint8_t SD_SCK = 2;

// Define I2C bus pins
const uint8_t I2C_SCL = 17;
const uint8_t I2C_SDA = 16;

float tempCH0;
float tempCH1;
float tempCH2;

// Static IP setup
IPAddress localIP(10, 32, 1, 25);
IPAddress gateway(10, 32, 0, 1);
IPAddress subnet(255, 255, 252, 0);
IPAddress dns(8, 8, 8, 8);

const unsigned int udpPort = 5000;
const unsigned int tcpPort = 6000;
unsigned int clientUDPPort = udpPort;

bool streaming = false;
bool sdInit = false;
bool i2cInit = false;

char tcpBuffer[64];
unsigned long lastSendTime = 0;
unsigned int streamInterval = 20; // 20ms interval

Qwiic1in3OLED disp;

IPAddress ip_adr;
IPAddress clientIP;
String ip_str;

WiFiServer tcpServer(tcpPort);
WiFiClient controlClient;
WiFiUDP udp;

String configPath = "/config.json";
File configFile;
JsonDocument configFileObj;
struct Config {
  char hostname[64];
  char ssid[32];
  char ssidPass[32];
  int  tcpPort;
  int  udpPort;
  IPAddress ip;
  IPAddress subnet;
  IPAddress gateway;
  IPAddress dns;
};
Config config;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure I2C bus
  Wire.setSCL(I2C_SCL);
  Wire.setSDA(I2C_SDA);
  Wire.begin();
  if(!disp.begin()) {
    while(true);
  }

  // Verify that the display is working
  disp.erase();
  disp.text(l1_posX, l1_posY, "Display test");
  disp.display();
  delay(500);

  // Configure SPI bus for SD card
  SPI.setRX(SD_MISO);
  SPI.setTX(SD_MOSI);
  SPI.setSCK(SD_SCK);
  disp.erase();
  disp.text(l1_posX, l1_posY, "SD card init");
  disp.display();
  sdInit = SD.begin(SD_CS);

  // Check if the SD card initialized correctly
  if(sdInit) {
    disp.text(l2_posX, l2_posY, "complete");
    configFile = SD.open(configPath);
    if(configFile) {
      disp.text(l3_posX, l3_posY, "found config file");
      DeserializationError configError = deserializeJson(configFileObj, configFile);
      
      config.ssid.formString(configFileObj["ssid"].as<const char*>());
      config.ssidPass.formString(configFileObj["password"].as<const char*>());
      config.ip.fromString(configFileObj["ip"].as<const char*>());
      config.subnet.fromString(configFileObj["subnet_mask"].as<const char*>());
      config.gateway.fromString(configFileObj["gateway"].as<const char*>());
      config.dns.fromString(configFileObj["dns"][0].as<const char*>());
      configFile.close();
    }
    disp.display();
  } else {
    disp.text(l2_posX, l2_posY, "failed");
    disp.display();
  }
  delay(500);

  analogReadResolution(12);

  pinMode(ch0stat_pin, OUTPUT);
  pinMode(ch1stat_pin, OUTPUT);
  pinMode(ch2stat_pin, OUTPUT);

  // Test outputs
  disp.erase();
  disp.text(l1_posX, l1_posY, "IO test");
  disp.display();

  digitalWrite(ch0stat_pin, HIGH);
  digitalWrite(ch1stat_pin, HIGH);
  digitalWrite(ch2stat_pin, HIGH);
  
  delay(500);
 
  digitalWrite(ch0stat_pin, LOW);
  digitalWrite(ch1stat_pin, LOW);
  digitalWrite(ch2stat_pin, LOW);

  delay(500);

  initializeAHT20();

  // Connect to WiFi
  disp.erase();
  disp.text(l1_posX, l1_posY, "Connecting to Wifi");
  disp.display();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  struct netif* netif = netif_list;  // should be the only interface

  if (netif != NULL and sdInit) {
    dhcp_stop(netif);  // Stop DHCP

    // Set static IP
    ip4_addr_t ip, gw, mask;

    ip4addr_aton(config.ip.toString().c_str(), &ip);
    ip4addr_aton(config.gateway.toString().c_str(), &gw);
    ip4addr_aton(config.subnet.toString().c_str(), &mask);

    netif_set_addr(netif, &ip, &mask, &gw);
    netif_set_up(netif);  // Bring interface back up
  } else {
    Serial.println("Failed to get netif");
  }

  ip_adr = WiFi.localIP();
  ip_str = ip_adr.toString();

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(ip_str);

  tcpServer.begin();
  udp.begin(udpPort);

  Serial.println("TCP server on port 6000");
  Serial.println("UDP streaming on port 5000");

  disp.erase();
  disp.text(l1_posX, l1_posY, "Connected");
  disp.text(l2_posX, l2_posY, ip_str);
  disp.display();
  
  delay(500);
  disp.erase();
  disp.setFont(QW_FONT_8X16);
}

void loop() {
  float temperature, humidity;

  String l1_text = "Temp A: ";
  String l2_text = "AH: ";
  String l3_text = "Temp 1: ";
  String l4_text = "Temp 2: ";

  tempCH0 = convertTemp_F(analogRead(tempCH0_pin));
  tempCH1 = convertTemp_F(analogRead(tempCH1_pin));
  tempCH2 = convertTemp_F(analogRead(tempCH2_pin));

  unsigned long sysTime = millis();

  char jsonPacket[128];
  int packetLen;

  // Set channel staus LEDs based on if a sensor is pressent
  checkInput(tempCH0, analogNC, ch0stat_pin);
  checkInput(tempCH1, analogNC, ch1stat_pin);
  checkInput(tempCH2, analogNC, ch2stat_pin);

  // Handle TCP control
  if (!controlClient || !controlClient.connected()) {
    controlClient = tcpServer.accept();
  }

  if (controlClient && controlClient.available()) {
    packetLen = controlClient.readBytesUntil('\n', tcpBuffer, sizeof(tcpBuffer) - 1);
    tcpBuffer[packetLen] = '\0';
    Serial.print("[TCP] Command: ");
    Serial.println(tcpBuffer);

    if (strncmp(tcpBuffer, "START_STREAM", 12) == 0) {
      int rpi = atoi(tcpBuffer + 13);
      streamInterval = (rpi > 0) ? rpi : 20;
      streaming = true;
      clientIP = controlClient.remoteIP();
      clientUDPPort = 5000;
      Serial.print("Streaming started at: ");
      Serial.print(streamInterval);
      Serial.print(" ms\n");
    } else if (strcmp(tcpBuffer, "STOP_STREAM") == 0) {
      streaming = false;
      Serial.println("Streaming stopped");
    }
  }

  // Stream data over UDP
  if (streaming) {
    if (sysTime - lastSendTime >= streamInterval) {
      snprintf(jsonPacket, sizeof(jsonPacket),
        "{\"tempCH0\":%f, \"tempCH1\":%f, \"tempCH2\":%f, \"timestamp\":%lu}",
        tempCH0,tempCH1, tempCH2, sysTime);

      udp.beginPacket(clientIP, clientUDPPort);
      udp.write(jsonPacket);
      udp.endPacket();

      Serial.println(jsonPacket);
      lastSendTime = sysTime;
    }
  }

  // Local display logic
  if (tempCH0 > analogNC) {
    l3_text+=String(tempCH0);
  } else {
    l3_text+="--.--";
  }
  if(tempCH1 > analogNC) {
    l4_text+=String(tempCH1);
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
  }      
}

void checkInput(float input, float cutoff, uint8_t statusPin) {
  if (input > cutoff) {
    digitalWrite(statusPin, HIGH);
  } else {
    digitalWrite(statusPin, LOW);
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