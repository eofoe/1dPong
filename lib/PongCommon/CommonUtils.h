#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Constants.h"

// =============================================================================
// COMMON OTA UTILITIES
// =============================================================================

class OTAManager {
public:
    // Initialize OTA with custom callbacks
    static void initOTA(const String& hostname,
                       std::function<void()> onStart = nullptr,
                       std::function<void()> onEnd = nullptr,
                       std::function<void(unsigned int, unsigned int)> onProgress = nullptr,
                       std::function<void(ota_error_t)> onError = nullptr,
                       const char* password = DEFAULT_OTA_PASSWORD) {

        ArduinoOTA.setHostname(hostname.c_str());
        ArduinoOTA.setPassword(password);

        // Default onStart handler
        if (onStart) {
            ArduinoOTA.onStart(onStart);
        } else {
            ArduinoOTA.onStart([]() {
                Serial.println("[OTA] Update starting...");
            });
        }

        // Default onEnd handler
        if (onEnd) {
            ArduinoOTA.onEnd(onEnd);
        } else {
            ArduinoOTA.onEnd([]() {
                Serial.println("\n[OTA] Update complete!");
            });
        }

        // Default onProgress handler
        if (onProgress) {
            ArduinoOTA.onProgress(onProgress);
        } else {
            ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                static uint8_t lastPercent = 0;
                uint8_t percent = (progress / (total / 100));
                if (percent != lastPercent && percent % OTA_PROGRESS_INTERVAL_PERCENT == 0) {
                    Serial.printf("[OTA] Progress: %u%%\n", percent);
                    lastPercent = percent;
                }
            });
        }

        // Default onError handler
        if (onError) {
            ArduinoOTA.onError(onError);
        } else {
            ArduinoOTA.onError([](ota_error_t error) {
                Serial.printf("[OTA] Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });
        }

        ArduinoOTA.begin();
        Serial.println("[OTA] Ready");
    }

    // Simplified init for devices without custom callbacks
    static void initOTA(const String& hostname, const char* password = DEFAULT_OTA_PASSWORD) {
        initOTA(hostname, nullptr, nullptr, nullptr, nullptr, password);
    }
};

// =============================================================================
// COMMON WIFI UTILITIES
// =============================================================================

class WiFiManager {
public:
    // Initialize WiFi in Station mode only
    static bool initStation(uint8_t channel = ESPNOW_CHANNEL) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

        Serial.printf("[WiFi] Station mode, Channel: %d\n", channel);
        return true;
    }

    // Initialize WiFi in AP+STA mode
    static bool initAPandStation(const char* ssid, const char* password,
                                 const char* stationSSID = nullptr,
                                 const char* stationPassword = nullptr,
                                 uint8_t channel = ESPNOW_CHANNEL) {
        WiFi.mode(WIFI_AP_STA);

        // Start Access Point
        if (ssid && password) {
            WiFi.softAP(ssid, password);
            Serial.printf("[WiFi] AP started: %s\n", ssid);
            Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        }

        // Connect to station if credentials provided
        if (stationSSID && stationPassword) {
            WiFi.begin(stationSSID, stationPassword);
            Serial.printf("[WiFi] Connecting to %s...\n", stationSSID);

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_TIMEOUT_ATTEMPTS) {
                delay(WIFI_CONNECT_DELAY_MS);
                Serial.print(".");
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("\n[WiFi] Connection failed, AP mode only");
            }
        }

        // Ensure WiFi channel matches ESP-NOW
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        return true;
    }

    // Initialize WiFi with default AP settings
    static bool initDefaultAP(uint8_t channel = ESPNOW_CHANNEL) {
        return initAPandStation(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD, nullptr, nullptr, channel);
    }

    // Get WiFi info as string
    static String getWiFiInfo() {
        String info = "WiFi Mode: ";
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);

        switch (mode) {
            case WIFI_MODE_STA: info += "Station"; break;
            case WIFI_MODE_AP: info += "Access Point"; break;
            case WIFI_MODE_APSTA: info += "AP+Station"; break;
            default: info += "Unknown";
        }

        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            if (WiFi.status() == WL_CONNECTED) {
                info += "\nStation IP: " + WiFi.localIP().toString();
                info += "\nSSID: " + WiFi.SSID();
                info += "\nRSSI: " + String(WiFi.RSSI()) + " dBm";
            } else {
                info += "\nStation: Not connected";
            }
        }

        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            info += "\nAP IP: " + WiFi.softAPIP().toString();
            info += "\nAP Clients: " + String(WiFi.softAPgetStationNum());
        }

        return info;
    }
};

// =============================================================================
// LED UTILITY CLASS
// =============================================================================

class LEDHelper {
public:
    // Blink LED n times
    static void blink(uint8_t pin, int count, int delayMs = LED_STARTUP_BLINK_MS) {
        for (int i = 0; i < count; i++) {
            digitalWrite(pin, HIGH);
            delay(delayMs);
            digitalWrite(pin, LOW);
            delay(delayMs);
        }
    }

    // Fast blink for errors
    static void blinkError(uint8_t pin) {
        blink(pin, ERROR_BLINK_FAST_COUNT, LED_ERROR_BLINK_MS);
    }

    // Slow blink for warnings
    static void blinkWarning(uint8_t pin) {
        blink(pin, ERROR_BLINK_SLOW_COUNT, LED_STARTUP_BLINK_MS);
    }

    // Quick confirm blink
    static void blinkConfirm(uint8_t pin) {
        blink(pin, ERROR_BLINK_CONFIRM_COUNT, LED_FLASH_DURATION_MS);
    }
};

// =============================================================================
// MAC ADDRESS UTILITIES
// =============================================================================

class MACHelper {
public:
    // Get last 3 bytes of MAC address
    static void getPartialMAC(uint8_t* output) {
        uint8_t fullMAC[6];
        WiFi.macAddress(fullMAC);
        memcpy(output, &fullMAC[3], MAC_PARTIAL_BYTES);
    }

    // Get full MAC address
    static void getFullMAC(uint8_t* output) {
        WiFi.macAddress(output);
    }

    // Format MAC as string (last 3 bytes)
    static String formatPartialMAC(const uint8_t* mac) {
        char buffer[9];
        sprintf(buffer, "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
        return String(buffer);
    }

    // Format full MAC as string
    static String formatFullMAC(const uint8_t* mac) {
        char buffer[18];
        sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buffer);
    }

    // Parse MAC string to bytes (supports both formats)
    static bool parseMAC(const char* str, uint8_t* output, uint8_t expectedBytes) {
        char temp[3] = {0};
        const char* p = str;

        for (uint8_t i = 0; i < expectedBytes; i++) {
            // Skip separators
            if (*p == ':' || *p == '-') p++;

            // Read two hex digits
            if (!isxdigit(p[0]) || !isxdigit(p[1])) {
                return false;
            }

            temp[0] = p[0];
            temp[1] = p[1];
            output[i] = (uint8_t)strtol(temp, nullptr, 16);
            p += 2;
        }

        return true;
    }
};

#endif // COMMON_UTILS_H
