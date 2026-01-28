#include "OTAUpdate.h"
#include "WiFiManager.h"

OTAUpdate::OTAUpdate() : wifiManager(nullptr), initialized(false), ota_started(false) {}

void OTAUpdate::begin(WiFiManager *manager)
{
    Serial.println("[OTAUpdate] begin() called");
    if (!manager)
    {
        Serial.println("[OTAUpdate] ERROR: WiFiManager pointer is null!");
        return;
    }
    wifiManager = manager;
    initialized = true;
    Serial.println("[OTAUpdate] WiFiManager pointer set and OTA initialized.");
}

void OTAUpdate::handle()
{
    if (!wifiManager)
    {
        Serial.println("[OTAUpdate] ERROR: WiFiManager pointer is null!");
        return;
    }
    if (!initialized)
    {
        Serial.println("[OTAUpdate] ERROR: OTA not initialized!");
        return;
    }
    if (wifiManager->isConnected())
    {
        // CRITICAL NEW LOGIC
        if (!ota_started)
        {
            // Only run this block ONCE when the connection is established!
            Serial.println("[OTAUpdate] WiFi connected, starting ArduinoOTA services.");

            ArduinoOTA.setHostname("WallPlotter");

            ArduinoOTA.onStart([]()
                               { Serial.println("[OTAUpdate] OTA Start"); });

            ArduinoOTA.onEnd([]()
                             { Serial.println("[OTAUpdate] OTA End"); });

            ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                                  { Serial.printf("[OTAUpdate] OTA Progress: %u / %u\n", progress, total); });

            ArduinoOTA.onError([](ota_error_t error)
                               { Serial.printf("[OTAUpdate] OTA Error: %u\n", error); });

            // This is the call that initializes network services and must be delayed!
            ArduinoOTA.begin();
            ota_started = true;
            Serial.println("[OTAUpdate] ArduinoOTA.begin() completed.");
        }

        // Once started, handle on every loop
        ArduinoOTA.handle();
    }
    else
    {
        // If WiFi disconnects, the OTA services should ideally be stopped
        // and ota_started reset, but for now, just skip the handle.
        // If you see the "Skipping" message frequently, it's safe to remove it.
        Serial.println("[OTAUpdate] Skipping OTA handle: WiFi not connected.");
    }
}

bool OTAUpdate::isConnected()
{
    return initialized && wifiManager && wifiManager->isConnected();
}
