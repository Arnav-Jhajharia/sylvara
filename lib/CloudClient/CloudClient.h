#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "../SensorManager/SensorManager.h"

class CloudClient {
public:
    CloudClient();
    
    // Initialize cloud client (loads backend config from preferences)
    bool begin();
    
    // Post sensor data to backend
    bool postSensorData(const SensorData& sensorData, const char* deviceId);
    
    // Configuration management
    void setBackendURL(const char* url);
    void setAPIKey(const char* key);
    const char* getBackendURL() const;
    const char* getAPIKey() const;

private:
    // Configuration storage
    Preferences preferences;
    String backendURL;
    String apiKey;
    
    // Default values
    static const char* DEFAULT_BACKEND_URL;
    static const char* DEFAULT_API_KEY;
};
