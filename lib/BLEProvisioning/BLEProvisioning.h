#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <string>
#include "../SensorManager/SensorManager.h"

class BLEProvisioning {
public:
    BLEProvisioning(const char* deviceName);

    void begin();
    void loop();

    bool isWiFiConnected();

    static bool wifiConnectedHandled;

    // Called by BLE callbacks
    static void setSSID(const std::string& value);
    static void setPasswordAndConnect(const std::string& value);
    static void scanAndUpdateWiFiList();

    // Provisioning control (public API)
    static void requestProvisioning();
    static void stopProvisioning();
    static void clearStoredCredentials();

    // Deep sleep
    static void goDeepSleep(uint64_t sleepDurationMicros);

private:
    static void connectToWiFi();
    static void startBLE();
    static void stopBLE();

    static std::string ssid;
    static std::string password;
    static Preferences preferences;
    static std::string deviceName;
    static BLECharacteristic* pWifiListCharacteristic;

    static bool bleActive;
    static bool bleInitialized;
    static BLEServer* server;
    static BLEAdvertising* advertising;
    static unsigned long wifiStartTime;


};
