#include <SparkFun_Qwiic_OLED.h>
#include <res/qw_fnt_8x16.h>

#include <Wire.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiUdp.h>

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

// Static IP setup
IPAddress localIP(10, 32, 1, 25);
IPAddress gateway(10, 32, 0, 1);
IPAddress subnet(255, 255, 252, 0);
IPAddress dns(8, 8, 8, 8);

IPAddress ip_adr;
String ip_str;

Qwiic1in3OLED disp;

WiFiServer tcpServer(6000);      // TCP control
WiFiClient controlClient;
WiFiUDP udp;
const unsigned int udpPort = 5000;

bool streaming = false;
IPAddress clientIP;
unsigned int clientUDPPort = udpPort;

char tcpBuffer[64];
unsigned long lastSendTime = 0;
const unsigned long streamInterval = 20;  // 5ms interval

void setup() {
  analogReadResolution(12);
  // Configure I2C bus
  Wire.setSCL(17);
  Wire.setSDA(16);
  Wire.begin();

  Serial.begin(115200);

  if(!disp.begin()) {
    while(true);
  }

  disp.text(l1_posX, l1_posY, "Connecting to Wifi");
  disp.display();

  // Set static IP
  WiFi.config(localIP, gateway, subnet, dns);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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
  String l2_text = "RH: ";
  String l3_text = "Temp 1: ";
  String l4_text = "Temp 2: ";

  float tempCH0 = convertTemp_F(analogRead(A0));
  float tempCH1 = convertTemp_F(analogRead(A1));
  float tempCH2 = convertTemp_F(analogRead(A2));

  unsigned long sysTime = millis();

  char jsonPacket[128];
  int packetLen;

  // Handle TCP control
  if (!controlClient || !controlClient.connected()) {
    controlClient = tcpServer.accept();
  }

  if (controlClient && controlClient.available()) {
    packetLen = controlClient.readBytesUntil('\n', tcpBuffer, sizeof(tcpBuffer) - 1);
    tcpBuffer[packetLen] = '\0';
    Serial.print("[TCP] Command: ");
    Serial.println(tcpBuffer);

    if (strcmp(tcpBuffer, "START_STREAM") == 0) {
      streaming = true;
      clientIP = controlClient.remoteIP();
      clientUDPPort = 5000;
      Serial.println("Streaming started");
    } else if (strcmp(tcpBuffer, "STOP_STREAM") == 0) {
      streaming = false;
      Serial.println("Streaming stopped");
    }
  }

  // Stream data over UDP
  if (streaming) {
    if (sysTime - lastSendTime >= streamInterval) {
      snprintf(jsonPacket, sizeof(jsonPacket),
        "{\"type\":\"CIP\",\"tag\":\"analogValue\",\"value\":%f,\"timestamp\":%lu}",
        tempCH0, sysTime);

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