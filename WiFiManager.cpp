#include "WiFiManager.h"

WiFiManager::WiFiManager()
  : apSSID("tvdetectorvan"), apPassword("ollie123"), isAPMode(false), lastScanTime(0) {
}

void WiFiManager::init() {
  preferredNetworks[0] = "Ollie7";
  preferredNetworks[1] = "Ollie8";
  preferredNetworks[2] = "Ollie10";
  preferredPasswords[0] = "Welcome1!";
  preferredPasswords[1] = "welcome1";
  preferredPasswords[2] = "welcome1";
  Serial.println("[WiFiManager] init(): preferredNetworks");
  for (int j = 0; j < 3; j++) {
    Serial.printf("  [%d] len=%d '%s'\n", j, preferredNetworks[j].length(), preferredNetworks[j].c_str());
  }
}

void WiFiManager::begin() {
  Serial.println("[WiFiManager] Starting WiFi initialization...");
  init();
  Serial.println("[WiFiManager] begin(): preferredNetworks");
  for (int j = 0; j < 3; j++) {
    Serial.printf("  [%d] len=%d '%s'\n", j, preferredNetworks[j].length(), preferredNetworks[j].c_str());
  }
  scanNetworks();
}

void WiFiManager::scanNetworks() {
  Serial.println("[WiFiManager] Scanning for available networks...");
  
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("[WiFiManager] No networks found.");
    startAccessPoint();
    return;
  }
  
  Serial.printf("[WiFiManager] Found %d networks:\n", numNetworks);
  
  // Look for strongest preferred network
  int bestNetworkIndex = -1;
  int bestRSSI = -120;
  String bestNetworkName = "";
  
  Serial.println("[WiFiManager] Preferred networks list:");
  for (int j = 0; j < 3; j++) {
    Serial.printf("  [%d] len=%d '%s'\n", j, preferredNetworks[j].length(), preferredNetworks[j].c_str());
  }
    Serial.println("[WiFiManager] scanNetworks(): preferredNetworks");
    for (int j = 0; j < 3; j++) {
      Serial.printf("  [%d] len=%d '%s'\n", j, preferredNetworks[j].length(), preferredNetworks[j].c_str());
    }
  Serial.println("[WiFiManager] Scanning results:");
  
  int bestNetworkPasswordIndex = -1;
  for (int i = 0; i < numNetworks; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    
    Serial.printf("  [%d] SSID: %s (len=%d), RSSI: %d dBm", i + 1, ssid.c_str(), ssid.length(), rssi);
    
    // Check if this network is in our preferred list
    bool isPreferred = false;
    for (int j = 0; j < 3; j++) {
        if (ssid.equalsIgnoreCase(preferredNetworks[j])) {
        isPreferred = true;
        Serial.print(" [PREFERRED]");
        
        if (rssi > bestRSSI) {
          bestNetworkIndex = i;
          bestRSSI = rssi;
          bestNetworkName = ssid;
          bestNetworkPasswordIndex = j;
          Serial.printf(" [NEW STRONGEST: %d dBm]", rssi);
        } else if (bestNetworkIndex >= 0) {
          Serial.printf(" [Not stronger than current best (%d dBm)]", bestRSSI);
        }
        break;
      }
    }
    if (!isPreferred) {
      Serial.print(" [NOT PREFERRED]");
    }
    Serial.println();
  }
  
  if (bestNetworkIndex >= 0) {
    Serial.printf("[WiFiManager] === DECISION === Connecting to: %s (RSSI: %d dBm)\n", bestNetworkName.c_str(), bestRSSI);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFi.SSID(bestNetworkIndex).c_str(), preferredPasswords[bestNetworkPasswordIndex].c_str());
    
    int attempts = 0;
    Serial.print("[WiFiManager] Connection attempt: ");
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(200);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      isAPMode = false;
      Serial.println();
      Serial.printf("[WiFiManager] ✓ Successfully connected to: %s\n", bestNetworkName.c_str());
      Serial.printf("[WiFiManager] IP Address: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("[WiFiManager] RSSI: %d dBm\n", WiFi.RSSI());
    } else {
      Serial.println();
      Serial.printf("[WiFiManager] ✗ Failed to connect to %s after 30 attempts.\n", bestNetworkName.c_str());
      Serial.println("[WiFiManager] Starting access point instead.");
      startAccessPoint();
    }
  } else {
    Serial.println("[WiFiManager] === DECISION === No preferred networks found. Starting access point.");
    startAccessPoint();
  }
  
  WiFi.scanDelete();
}

void WiFiManager::startAccessPoint() {
  Serial.printf("[WiFiManager] === DECISION === Starting Access Point mode\n");
  Serial.printf("[WiFiManager] AP SSID: %s\n", apSSID);
  Serial.printf("[WiFiManager] AP Password: %s\n", apPassword);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFiManager] ✓ Access Point started!\n");
  Serial.printf("[WiFiManager] AP IP Address: %s\n", apIP.toString().c_str());
  Serial.printf("[WiFiManager] Connect to '%s' with password '%s'\n", apSSID, apPassword);
  
  isAPMode = true;
}

void WiFiManager::checkConnection() {
  // If in AP mode, nothing to check
  if (isAPMode) {
    return;
  }
  
  // If not connected, try to reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFiManager] Connection lost. Will rescan on next interval.");
  }
}

void WiFiManager::handle() {
  //Serial.println("[WiFiManager] handle() called");
  unsigned long now = millis();
  
  // Periodically rescan and try to connect to preferred networks
  if (now - lastScanTime > SCAN_INTERVAL) {
    lastScanTime = now;
    
    // If in AP mode or disconnected, try to find preferred networks
    if (isAPMode || WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFiManager] Periodic scan triggered...");
      scanNetworks();
    }
  }
  
  checkConnection();
}

bool WiFiManager::isConnected() {
  return !isAPMode && WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isInAPMode() {
  return isAPMode;
}

void WiFiManager::printStatus() {
  if (isAPMode) {
    Serial.printf("[WiFiManager] In Access Point mode: %s\n", apSSID);
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFiManager] Connected to: %s, IP: ", WiFi.SSID().c_str());
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFiManager] Not connected, retrying...");
  }
}
