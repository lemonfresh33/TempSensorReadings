#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

class WiFiManager; // Forward declaration

class OTAUpdate {
private:
  WiFiManager* wifiManager;
  bool initialized;
  bool ota_started;

public:
  OTAUpdate();
  
  void begin(WiFiManager* wm);
  void handle();
  bool isConnected();
  
private:
  void setupWiFi();
  void setupOTA();
};

#endif // OTA_UPDATE_H
