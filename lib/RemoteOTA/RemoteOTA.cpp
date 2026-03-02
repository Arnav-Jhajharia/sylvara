#include "RemoteOTA.h"

void RemoteOTA::check() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] WiFi not connected, cannot check for updates");
        return;
    }

    Serial.println("\n========== REMOTE OTA CHECK ==========");
    Serial.print("[OTA] Current firmware: v");
    Serial.println(FIRMWARE_VERSION);
    Serial.println("[OTA] Checking GitHub for updates...");

    WiFiClientSecure client;
    client.setInsecure();

    // Check latest release version via GitHub API
    HTTPClient http;
    String apiUrl = "https://api.github.com/repos/" + String(GITHUB_OWNER)
                  + "/" + String(GITHUB_REPO) + "/releases/latest";

    http.begin(client, apiUrl);
    http.addHeader("User-Agent", "Sylvara-ESP32");

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[OTA] GitHub API error: HTTP %d\n", httpCode);
        http.end();
        Serial.println("=======================================\n");
        return;
    }

    String payload = http.getString();
    http.end();

    // Parse tag_name from JSON response
    String latestVersion = "";
    int idx = payload.indexOf("\"tag_name\"");
    if (idx >= 0) {
        int start = payload.indexOf("\"", idx + 10) + 1;
        int end = payload.indexOf("\"", start);
        latestVersion = payload.substring(start, end);
        if (latestVersion.startsWith("v")) latestVersion = latestVersion.substring(1);
    }

    if (latestVersion.length() == 0) {
        Serial.println("[OTA] Could not parse version from GitHub");
        Serial.println("=======================================\n");
        return;
    }

    Serial.print("[OTA] Latest version: v");
    Serial.println(latestVersion);

    if (latestVersion == FIRMWARE_VERSION) {
        Serial.println("[OTA] Firmware is up to date!");
        Serial.println("=======================================\n");
        return;
    }

    // Download and flash new firmware
    Serial.println("[OTA] New version found! Downloading...");

    String firmwareUrl = "https://github.com/" + String(GITHUB_OWNER)
                       + "/" + String(GITHUB_REPO)
                       + "/releases/latest/download/firmware.bin";
    Serial.print("[OTA] URL: ");
    Serial.println(firmwareUrl);

    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] FAILED: (%d) %s\n",
                httpUpdate.getLastError(),
                httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No update needed");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Success! Rebooting...");
            break;
    }
    Serial.println("=======================================\n");
}
