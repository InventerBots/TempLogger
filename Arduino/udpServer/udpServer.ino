#include <WiFi.h>
#include <WiFiUdp.h>

#define ANALOG_RES 12

// Replace with your WiFi credentials
const char* ssid     = "Foe-Glass wifi";
const char* password = "Wizard_4 wand";

// Static IP setup
IPAddress localIP(10, 32, 1, 25);
IPAddress gateway(10, 32, 0, 1);
IPAddress subnet(255, 255, 252, 0);
IPAddress dns(8, 8, 8, 8);

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
  Serial.begin(115200);

  analogReadResolution(12);

  // Set static IP
  WiFi.config(localIP, gateway, subnet, dns);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  tcpServer.begin();
  udp.begin(udpPort);

  Serial.println("TCP server on port 6000");
  Serial.println("UDP streaming on port 5000");
}

void loop() {
  // Handle TCP control
  if (!controlClient || !controlClient.connected()) {
    controlClient = tcpServer.available();
  }

  if (controlClient && controlClient.available()) {
    int len = controlClient.readBytesUntil('\n', tcpBuffer, sizeof(tcpBuffer) - 1);
    tcpBuffer[len] = '\0';
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
    unsigned long now = millis();
    if (now - lastSendTime >= streamInterval) {
      float analogValue = convertTemp_F(analogRead(A0));

      char jsonPacket[128];
      snprintf(jsonPacket, sizeof(jsonPacket),
        "{\"type\":\"CIP\",\"tag\":\"analogValue\",\"value\":%f,\"timestamp\":%lu}",
        analogValue, now);

      udp.beginPacket(clientIP, clientUDPPort);
      udp.write(jsonPacket);
      udp.endPacket();

      Serial.println(jsonPacket);
      lastSendTime = now;
    }
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