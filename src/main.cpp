#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <BLEProvisioning.h>
#include <SensorManager.h>
#include <CloudClient.h>

BLEProvisioning ble("SYL-7482");
SensorManager sensorManager;
CloudClient cloudClient;
AsyncWebServer otaServer(80);
bool otaServerStarted = false;

// Button configuration
// Change this to the actual GPIO connected to the button on your PCB.
// The hardware has the button idle LOW and it becomes HIGH when pressed.
const uint8_t BUTTON_PIN = 19; // <-- update as needed

// Deep sleep config
const uint64_t SLEEP_DURATION_SECONDS = 300; // 5 minutes (for testing; change to 3600 for production 1 hour)
const uint64_t SLEEP_DURATION_MICROS = SLEEP_DURATION_SECONDS * 1000000ULL;

// Wakeup reason tracking
bool isButtonWakeup = false;
bool isTimerWakeup = false;

// Debounce / press detection
bool buttonState = LOW;
bool lastButtonReading = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long buttonDownTime = 0;
const unsigned long longPressMillis = 2000;

// Multi-press detection
int pressCount = 0;
unsigned long lastReleaseTime = 0;
const unsigned long multiPressTimeout = 600; // ms to wait for subsequent presses
bool awaitingMultiPress = false;

// WiFi activity timer (non-blocking)
unsigned long lastWiFiActivityTime = 0;
const unsigned long wifiActivityInterval = 5000; // 5 seconds

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Booting Sylvara firmware");

    // Check wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.print("[WAKEUP] Reason code: ");
    Serial.println((int)wakeup_reason);
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[WAKEUP] Source: GPIO/EXT0 (button press)");
            isButtonWakeup = true;
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println("[WAKEUP] Source: GPIO (button press via gpio_wakeup)");
            isButtonWakeup = true;
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[WAKEUP] Source: RTC Timer (scheduled 5-minute interval)");
            isTimerWakeup = true;
            break;
        default:
            Serial.println("[WAKEUP] Source: Cold boot / power reset / unknown");
            break;
    }

    // DEBUG
    Serial.println("Battery voltage: " + String(analogRead(1) * 9.5035 * pow(10, -4), 2));

    // Configure button pin (assumes external pull-down so use INPUT)
    pinMode(BUTTON_PIN, INPUT);

    // Initialize sensors
    if (!sensorManager.begin()) {
        Serial.println("Sensor initialization failed!");
    }

    // Initialize cloud client
    cloudClient.begin();

    ble.begin();

    // Initialize ElegantOTA (server starts when WiFi connects)
    ElegantOTA.begin(&otaServer);
}

void loop() {
    ble.loop();
    ElegantOTA.loop();

    // Start OTA server once WiFi connects
    if (WiFi.status() == WL_CONNECTED && !otaServerStarted) {
        otaServer.begin();
        otaServerStarted = true;
        Serial.println("[OTA] ElegantOTA available at http://" + WiFi.localIP().toString() + "/update");
    }

    // If woken by timer, post sensor data and go back to sleep
    if (isTimerWakeup) {
        Serial.println("\n========== TIMER WAKEUP CYCLE ==========");
        Serial.println("Reading sensors...");
        
        // Read all sensor data
        SensorData sensorData = sensorManager.readAllSensors();
        
        if (sensorData.isValid) {
            Serial.println("[SENSORS] Data read successfully:");
            Serial.print("  Light: ");
            Serial.print(sensorData.light);
            Serial.println(" lux");
            Serial.print("  Temperature: ");
            Serial.print(sensorData.temperature);
            Serial.println(" °C");
            Serial.print("  Humidity: ");
            Serial.print(sensorData.humidity);
            Serial.println(" %");
            Serial.print("  Battery: ");
            Serial.print(sensorData.battery);
            Serial.println(" V");
        } else {
            Serial.println("[SENSORS] ERROR: Sensor data invalid!");
        }

        // Connect to WiFi if we have stored credentials
        if (ble.isWiFiConnected() == false) {
            // Try to connect using stored credentials
            // (BLEProvisioning::begin already attempted this)
            Serial.println("[WIFI] Attempting connection (waiting up to 10 seconds)...");
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            Serial.println();
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[WIFI] Connected! Posting sensor data...");
            cloudClient.postSensorData(sensorData, "SYL-7482");
            Serial.println("[CLOUD] Data post complete");
        } else {
            Serial.println("[WIFI] ERROR: Not connected, skipping data post");
        }

        // Post done, return to deep sleep
        Serial.println("Cycle complete, preparing deep sleep...");
        delay(1000);
        isTimerWakeup = false;
        Serial.println("========================================\n");
        BLEProvisioning::goDeepSleep(SLEEP_DURATION_MICROS);
    }

    // If woken by button, allow user interaction (BLE provisioning, etc)
    if (isButtonWakeup) {
        Serial.println("\n========== BUTTON WAKEUP ==========");
        Serial.println("Device active for user interaction");
        Serial.println("Button actions:");
        Serial.println("  - SINGLE PRESS: Enable BLE provisioning");
        Serial.println("  - DOUBLE PRESS: Clear stored WiFi credentials");
        Serial.println("  - LONG PRESS: Debug output");
        Serial.println("===================================\n");
        isButtonWakeup = false;
    }

    // Button handling (debounced, multi-press and long press)
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
        lastButtonReading = reading;
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == HIGH) {
                // button pressed down
                buttonDownTime = millis();
            } else {
                // button released -> check duration
                unsigned long pressDuration = millis() - buttonDownTime;
                if (pressDuration >= longPressMillis) {
                    // Long press detected
                    Serial.println("Button: LONG PRESS");
                    pressCount = 0;
                    awaitingMultiPress = false;
                } else {
                    // Short press: accumulate for multi-press detection
                    pressCount++;
                    lastReleaseTime = millis();
                    awaitingMultiPress = true;
                }
            }
        }
    }

    // If waiting for additional presses, check timeout to finalize action
    if (awaitingMultiPress && (millis() - lastReleaseTime) > multiPressTimeout) {
        if (pressCount == 1) {
            Serial.println("Button: SINGLE PRESS -> starting BLE provisioning");
            BLEProvisioning::requestProvisioning();
        } else if (pressCount == 2) {
            Serial.println("Button: DOUBLE PRESS -> clearing stored credentials");
            BLEProvisioning::clearStoredCredentials();
        } else {
            Serial.print("Button: MULTI PRESS (count=");
            Serial.print(pressCount);
            Serial.println(")");
        }
        // Reset
        pressCount = 0;
        awaitingMultiPress = false;
    }

    // Non-blocking WiFi activity (instead of blocking delay)
    if (ble.isWiFiConnected()) {
        if (millis() - lastWiFiActivityTime > wifiActivityInterval) {
            // Placeholder for sensor read / cloud upload
            // TODO: Add sensor logic here
            lastWiFiActivityTime = millis();
        }
    }

    delay(100);
}
