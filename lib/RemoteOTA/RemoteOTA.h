#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#define FIRMWARE_VERSION "1.0.0"

class RemoteOTA {
public:
    static void check();

private:
    static constexpr const char* GITHUB_OWNER = "Arnav-Jhajharia";
    static constexpr const char* GITHUB_REPO  = "sylvara";
};
