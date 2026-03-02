#include <Arduino.h>
#include "BLEProvisioning.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

// UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_SSID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_PASSWORD  "88573d8f-b986-42d8-9657-3aa5a16d8689"
#define CHAR_UUID_WIFI_LIST "04f99703-9d8e-4e4c-9f87-6e945c71d601"

// Config
#define WIFI_TIMEOUT_MS 15000

std::string BLEProvisioning::ssid = "";
std::string BLEProvisioning::password = "";
Preferences BLEProvisioning::preferences;
bool BLEProvisioning::bleActive = false;
bool BLEProvisioning::wifiConnectedHandled = false;

std::string BLEProvisioning::deviceName = "";
BLECharacteristic* BLEProvisioning::pWifiListCharacteristic = NULL;
bool BLEProvisioning::bleInitialized = false;
BLEServer* BLEProvisioning::server = NULL;
BLEAdvertising* BLEProvisioning::advertising = NULL;

unsigned long BLEProvisioning::wifiStartTime = 0;

// -------- BLE CALLBACKS --------

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        Serial.println("BLE client connected. Scanning for networks...");
        BLEProvisioning::scanAndUpdateWiFiList();
    }

    void onDisconnect(BLEServer* server) override {
        Serial.println("BLE client disconnected");
        server->getAdvertising()->start();
    }
};

class SSIDCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        BLEProvisioning::setSSID(c->getValue());
    }
};

class PasswordCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        BLEProvisioning::setPasswordAndConnect(c->getValue());
    }
};

// -------- CLASS IMPLEMENTATION --------

BLEProvisioning::BLEProvisioning(const char* deviceName) {
    BLEProvisioning::deviceName = deviceName;
}

void BLEProvisioning::begin() {
    // Load stored credentials
    preferences.begin("wifi-config", true);
    String savedSSID = preferences.getString("ssid", "");
    String savedPass = preferences.getString("password", "");
    preferences.end();

    if (savedSSID.length()) {
        Serial.println("Found stored WiFi credentials");
        ssid = savedSSID.c_str();
        password = savedPass.c_str();
        connectToWiFi();
    } else {
        Serial.println("No credentials found — BLE will stay off until requested");
        // Do not auto-start BLE; wait for user to request provisioning
    }
}

void BLEProvisioning::loop() {

    // Handle WiFi timeout
    if (wifiStartTime && WiFi.status() != WL_CONNECTED) {
        if (millis() - wifiStartTime > WIFI_TIMEOUT_MS) {
            Serial.println("WiFi timeout, enabling BLE");
            wifiStartTime = 0;
            startBLE();
        }
    }

    // ONE-SHOT BLE shutdown after WiFi success
    if (WiFi.status() == WL_CONNECTED && bleActive && !wifiConnectedHandled) {
        Serial.println("WiFi connected, disabling BLE");
        wifiConnectedHandled = true;
        stopBLE();
    }
}


bool BLEProvisioning::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// -------- STATIC HELPERS --------

void BLEProvisioning::setSSID(const std::string& value) {
    ssid = value;
    Serial.print("SSID received: ");
    Serial.println(ssid.c_str());
}

void BLEProvisioning::setPasswordAndConnect(const std::string& value) {
    password = value;
    Serial.println("Password received");
    connectToWiFi();
    // We received credentials from BLE — stop advertising/provisioning now
    if (bleActive) {
        Serial.println("Received credentials over BLE — stopping provisioning");
        stopBLE();
    }
}

void BLEProvisioning::connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    wifiStartTime = millis();

    // Save credentials
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", ssid.c_str());
    preferences.putString("password", password.c_str());
    preferences.end();
}

void BLEProvisioning::startBLE() {
    wifiConnectedHandled = false;

    Serial.print("startBLE() called. bleActive="); Serial.print(bleActive);
    Serial.print(" bleInitialized="); Serial.println(bleInitialized);

    if (bleActive) {
        Serial.println("startBLE(): already active, returning");
        return;
    }

    // If not yet initialized, create server, service and characteristics once
    if (!bleInitialized) {
        Serial.println("startBLE(): initializing BLE stack...");
        BLEDevice::init(BLEProvisioning::deviceName.c_str());
        Serial.println("startBLE(): BLEDevice::init done");

        server = BLEDevice::createServer();
        if (server) {
            Serial.println("startBLE(): server created");
            server->setCallbacks(new ServerCallbacks());
        } else {
            Serial.println("startBLE(): failed to create server");
        }

        BLEService* service = nullptr;
        if (server) service = server->createService(SERVICE_UUID);
        if (service) {
            Serial.println("startBLE(): service created");
        } else {
            Serial.println("startBLE(): failed to create service");
        }

        if (service) {
            BLECharacteristic* ssidChar = service->createCharacteristic(
                CHAR_UUID_SSID,
                BLECharacteristic::PROPERTY_WRITE
            );
            ssidChar->setCallbacks(new SSIDCallback());

            BLECharacteristic* passChar = service->createCharacteristic(
                CHAR_UUID_PASSWORD,
                BLECharacteristic::PROPERTY_WRITE
            );
            passChar->setCallbacks(new PasswordCallback());

            pWifiListCharacteristic = service->createCharacteristic(
                CHAR_UUID_WIFI_LIST,
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
            );

            service->start();
            Serial.println("startBLE(): service started");
        }

        advertising = BLEDevice::getAdvertising();
        if (advertising) {
            Serial.println("startBLE(): got advertising instance");
            advertising->addServiceUUID(SERVICE_UUID);
            advertising->setScanResponse(true);
            advertising->setMinPreferred(0x06);
            advertising->setMinPreferred(0x12);
        } else {
            Serial.println("startBLE(): advertising instance is NULL");
        }

        bleInitialized = true;
        Serial.println("startBLE(): initialization complete");
    }

    // Start advertising (re-usable)
    if (advertising) {
        Serial.println("startBLE(): starting advertising via advertising->start()");
        advertising->start();
        bleActive = true;
        Serial.println("BLE provisioning active (advertising)");
    } else {
        Serial.println("startBLE(): advertising is NULL, attempting BLEDevice::startAdvertising()");
        BLEDevice::startAdvertising();
        bleActive = true;
        Serial.println("BLE provisioning active (advertising) [fallback]");
    }
}

void BLEProvisioning::stopBLE() {
    // Stop advertising but keep BLE initialized so we can restart without full reinit
    if (advertising) {
        advertising->stop();
    }
    bleActive = false;
    Serial.println("BLE advertising stopped (BLE remains initialized)");
}

void BLEProvisioning::scanAndUpdateWiFiList() {
    Serial.println("Scanning for WiFi networks...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    Serial.println("Scan done");

    std::string wifiList = "";
    if (n == 0) {
        wifiList = "No networks found";
    } else {
        // Top 5 strong networks to fit in MTU (approx)
        int limit = (n > 5) ? 5 : n;
        for (int i = 0; i < limit; ++i) {
            if (i > 0) wifiList += "|";
            wifiList += WiFi.SSID(i).c_str();
        }
    }

    if (pWifiListCharacteristic != NULL) {
        pWifiListCharacteristic->setValue(wifiList);
        Serial.print("WiFi list updated: ");
        Serial.println(wifiList.c_str());
    }
}

void BLEProvisioning::requestProvisioning() {
    // Public wrapper to start BLE advertising/provisioning
    startBLE();
}

void BLEProvisioning::clearStoredCredentials() {
    preferences.begin("wifi-config", false);
    preferences.clear();
    preferences.end();
    Serial.println("Stored WiFi credentials cleared");
    // Also disconnect from WiFi if connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
    }
}

void BLEProvisioning::stopProvisioning() {
    stopBLE();
}

void BLEProvisioning::goDeepSleep(uint64_t sleepDurationMicros) {
    Serial.println("\n========== DEEP SLEEP CONFIG ==========");
    Serial.print("Sleep duration: ");
    Serial.print(sleepDurationMicros / 1000000);
    Serial.println(" seconds");
    Serial.println("Wakeup sources enabled:");
    Serial.println("  - RTC Timer (primary)");
    Serial.println("  - GPIO 8 button (secondary)");
    Serial.println("========================================\n");
    Serial.flush();

    // Disable WiFi and BLE before sleep to save power
    WiFi.disconnect(true);  // turn off radio
    BLEDevice::deinit(true);

    // Configure RTC timer to wake from deep sleep
    esp_sleep_enable_timer_wakeup(sleepDurationMicros);
    Serial.println("[SLEEP] RTC timer enabled");

    // Configure GPIO 8 (button) as wakeup source (active LOW)
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(GPIO_NUM_8, GPIO_INTR_LOW_LEVEL);
    Serial.println("[SLEEP] GPIO 8 button wakeup enabled (LOW level)");
    
    Serial.println("[SLEEP] Entering deep sleep now...");
    Serial.println("[SLEEP] Device will wake on: timer OR button press");
    Serial.flush();
    delay(100);

    // Enter deep sleep
    esp_deep_sleep_start();
    // Code after this line won't execute until device wakes up
}


