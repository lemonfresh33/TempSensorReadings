#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
  void init();
private:
  String preferredNetworks[3];
  String preferredPasswords[3];
  const char* apSSID;
  const char* apPassword;
  bool isAPMode;
  unsigned long lastScanTime;
  const unsigned long SCAN_INTERVAL = 30000; // Rescan every 30 seconds

public:
  WiFiManager();
  
  void begin();
  void handle();
  bool isConnected();
  bool isInAPMode();
  void printStatus();
  
private:
  void scanNetworks();
  void connectToStrongest();
  void startAccessPoint();
  void checkConnection();
};

#endif // WIFI_MANAGER_H
