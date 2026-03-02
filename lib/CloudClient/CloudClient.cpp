#include "CloudClient.h"

// Set your defaults here
const char* CloudClient::DEFAULT_BACKEND_URL = "https://57556399f1a0.ngrok-free.app/api/device/ingest";
const char* CloudClient::DEFAULT_API_KEY = "sylvara-secret-key-123";

CloudClient::CloudClient() {
    backendURL = DEFAULT_BACKEND_URL;
    apiKey = DEFAULT_API_KEY;
}

bool CloudClient::begin() {
    Serial.println("CloudClient: Initializing...");
    
    // Load backend config from NVS if stored
    preferences.begin("cloud-config", true);  // Read-only
    
    String storedURL = preferences.getString("backend_url", "");
    String storedKey = preferences.getString("api_key", "");
    
    preferences.end();
    
    if (storedURL.length() > 0) {
        backendURL = storedURL;
        Serial.print("CloudClient: Loaded backend URL from NVS: ");
        Serial.println(backendURL.c_str());
    } else {
        Serial.print("CloudClient: Using default backend URL: ");
        Serial.println(backendURL.c_str());
    }
    
    if (storedKey.length() > 0) {
        apiKey = storedKey;
        Serial.println("CloudClient: Loaded API key from NVS");
    } else {
        Serial.println("CloudClient: Using default API key");
    }
    
    return true;
}

void CloudClient::setBackendURL(const char* url) {
    backendURL = url;
    Serial.print("CloudClient: Backend URL set to: ");
    Serial.println(backendURL.c_str());
    
    // Store in NVS for persistence
    preferences.begin("cloud-config", false);  // Read-write
    preferences.putString("backend_url", backendURL);
    preferences.end();
}

void CloudClient::setAPIKey(const char* key) {
    apiKey = key;
    Serial.println("CloudClient: API key updated");
    
    // Store in NVS for persistence
    preferences.begin("cloud-config", false);  // Read-write
    preferences.putString("api_key", apiKey);
    preferences.end();
}

const char* CloudClient::getBackendURL() const {
    return backendURL.c_str();
}

const char* CloudClient::getAPIKey() const {
    return apiKey.c_str();
}

bool CloudClient::postSensorData(const SensorData& sensorData, const char* deviceId) {
    if (!WiFi.isConnected()) {
        Serial.println("CloudClient: WiFi not connected, skipping post");
        return false;
    }

    if (!sensorData.isValid) {
        Serial.println("CloudClient: Sensor data invalid, skipping post");
        return false;
    }

    HTTPClient http;
    
    // Build JSON payload
    String payload = "{";
    payload += "\"device_id\":\"" + String(deviceId) + "\",";
    payload += "\"timestamp\":" + String(time(nullptr)) + ",";
    payload += "\"soil_moisture\":" + String(sensorData.humidity, 2) + ",";  // humidity as moisture proxy
    payload += "\"temperature\":" + String(sensorData.temperature, 2) + ",";
    payload += "\"light\":" + String(sensorData.light, 2) + ",";
    payload += "\"battery\":" + String(sensorData.battery, 2);
    payload += "}";

    Serial.print("CloudClient: Posting to ");
    Serial.println(backendURL.c_str());
    Serial.print("CloudClient: Payload: ");
    Serial.println(payload);

    http.begin(backendURL.c_str());
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-sylvara-key", apiKey.c_str());

    int httpCode = http.POST(payload);
    Serial.print("CloudClient: HTTP Response Code: ");
    Serial.println(httpCode);

    bool success = false;
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        Serial.println("CloudClient: Sensor data posted successfully");
        success = true;
    } else {
        Serial.print("CloudClient: Failed to post sensor data - HTTP ");
        Serial.println(httpCode);
        
        // Log response for debugging
        String response = http.getString();
        Serial.print("CloudClient: Response: ");
        Serial.println(response);
    }

    http.end();
    return success;
}
