#ifndef PONG_COMMON_H
#define PONG_COMMON_H

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Protocol.h"
#include "Config.h"

// =============================================================================
// GLOBAL UTILITIES
// =============================================================================

class PongCommon {
public:
    // Initialize ESP-NOW
    static bool initESPNow(uint8_t channel = ESPNOW_CHANNEL) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();

        // Set channel
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

        if (esp_now_init() != ESP_OK) {
            Serial.println("[ERROR] ESP-NOW init failed");
            return false;
        }

        Serial.println("[OK] ESP-NOW initialized");
        return true;
    }

    // Add peer device
    static bool addPeer(const uint8_t* macAddr) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, macAddr, 6);
        peerInfo.channel = ESPNOW_CHANNEL;
        peerInfo.encrypt = false;

        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("[ERROR] Failed to add peer");
            return false;
        }

        return true;
    }

    // Add broadcast peer
    static bool addBroadcastPeer() {
        uint8_t broadcastAddr[] = BROADCAST_MAC;
        return addPeer(broadcastAddr);
    }

    // Send ESP-NOW message
    static bool sendMessage(const uint8_t* macAddr, const void* data, size_t len) {
        esp_err_t result = esp_now_send(macAddr, (uint8_t*)data, len);
        return (result == ESP_OK);
    }

    // Broadcast message
    static bool broadcastMessage(const void* data, size_t len) {
        uint8_t broadcastAddr[] = BROADCAST_MAC;
        return sendMessage(broadcastAddr, data, len);
    }

    // Get MAC address as string
    static String getMacAddress() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18];
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }

    // Initialize message header
    static void initMessageHeader(MessageHeader& header, MessageType type, uint8_t senderId) {
        static uint16_t sequenceCounter = 0;
        header.type = type;
        header.senderId = senderId;
        header.timestamp = micros();
        header.sequenceNum = sequenceCounter++;
    }

    // LED color utilities
    static uint32_t rgbToColor(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    static uint32_t rgbwToColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | w;
    }

    static void colorToRGB(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
        r = (color >> 16) & 0xFF;
        g = (color >> 8) & 0xFF;
        b = color & 0xFF;
    }

    static void colorToRGBW(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) {
        r = (color >> 24) & 0xFF;
        g = (color >> 16) & 0xFF;
        b = (color >> 8) & 0xFF;
        w = color & 0xFF;
    }

    // Scale brightness (0-255 input, 0-100 scale percentage)
    static uint8_t scaleBrightness(uint8_t value, uint8_t scale) {
        return (uint16_t)value * scale / 100;
    }
};

// =============================================================================
// PERSISTENT STORAGE MANAGER
// =============================================================================

class ConfigManager {
private:
    Preferences prefs;

public:
    // Load game configuration
    bool loadGameConfig(GameConfig& config) {
        if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
            Serial.println("[ERROR] Failed to open NVS");
            return false;
        }

        size_t len = prefs.getBytes(NVS_KEY_GAME_CONFIG, &config, sizeof(GameConfig));
        prefs.end();

        if (len != sizeof(GameConfig) || !isConfigValid(config)) {
            Serial.println("[WARN] Invalid game config, using defaults");
            initDefaultGameConfig(config);
            return false;
        }

        Serial.println("[OK] Game config loaded from NVS");
        return true;
    }

    // Save game configuration
    bool saveGameConfig(const GameConfig& config) {
        if (!prefs.begin(NVS_NAMESPACE, false)) {  // Read-write
            Serial.println("[ERROR] Failed to open NVS for writing");
            return false;
        }

        size_t written = prefs.putBytes(NVS_KEY_GAME_CONFIG, &config, sizeof(GameConfig));
        prefs.end();

        if (written != sizeof(GameConfig)) {
            Serial.println("[ERROR] Failed to write game config");
            return false;
        }

        Serial.println("[OK] Game config saved to NVS");
        return true;
    }

    // Load visual configuration
    bool loadVisualConfig(VisualConfig& config) {
        if (!prefs.begin(NVS_NAMESPACE, true)) {
            return false;
        }

        size_t len = prefs.getBytes(NVS_KEY_VISUAL_CONFIG, &config, sizeof(VisualConfig));
        prefs.end();

        if (len != sizeof(VisualConfig) || !isConfigValid(config)) {
            Serial.println("[WARN] Invalid visual config, using defaults");
            initDefaultVisualConfig(config);
            return false;
        }

        Serial.println("[OK] Visual config loaded from NVS");
        return true;
    }

    // Save visual configuration
    bool saveVisualConfig(const VisualConfig& config) {
        if (!prefs.begin(NVS_NAMESPACE, false)) {
            return false;
        }

        size_t written = prefs.putBytes(NVS_KEY_VISUAL_CONFIG, &config, sizeof(VisualConfig));
        prefs.end();

        if (written != sizeof(VisualConfig)) {
            Serial.println("[ERROR] Failed to write visual config");
            return false;
        }

        Serial.println("[OK] Visual config saved to NVS");
        return true;
    }

    // Load statistics
    bool loadStatistics(Statistics& stats) {
        if (!prefs.begin(NVS_NAMESPACE, true)) {
            return false;
        }

        size_t len = prefs.getBytes(NVS_KEY_STATISTICS, &stats, sizeof(Statistics));
        prefs.end();

        if (len != sizeof(Statistics) || !isStatsValid(stats)) {
            Serial.println("[WARN] Invalid statistics, initializing");
            initStatistics(stats);
            return false;
        }

        Serial.println("[OK] Statistics loaded from NVS");
        return true;
    }

    // Save statistics
    bool saveStatistics(const Statistics& stats) {
        if (!prefs.begin(NVS_NAMESPACE, false)) {
            return false;
        }

        size_t written = prefs.putBytes(NVS_KEY_STATISTICS, &stats, sizeof(Statistics));
        prefs.end();

        if (written != sizeof(Statistics)) {
            Serial.println("[ERROR] Failed to write statistics");
            return false;
        }

        return true;  // Don't log every stats save (happens often)
    }

    // Clear all stored data (factory reset)
    bool clearAll() {
        if (!prefs.begin(NVS_NAMESPACE, false)) {
            return false;
        }

        bool success = prefs.clear();
        prefs.end();

        Serial.println(success ? "[OK] NVS cleared" : "[ERROR] Failed to clear NVS");
        return success;
    }

    // Get/Set device ID (0-255)
    uint8_t getDeviceId() {
        if (!prefs.begin(NVS_NAMESPACE, true)) {
            return 0;
        }

        uint8_t id = prefs.getUChar(NVS_KEY_DEVICE_ID, 0);
        prefs.end();
        return id;
    }

    bool setDeviceId(uint8_t id) {
        if (!prefs.begin(NVS_NAMESPACE, false)) {
            return false;
        }

        size_t written = prefs.putUChar(NVS_KEY_DEVICE_ID, id);
        prefs.end();
        return (written > 0);
    }

    // Load MAC mapping table
    bool loadMacMapping(MacMappingTable& table) {
        if (!prefs.begin(NVS_NAMESPACE, true)) {
            Serial.println("[ERROR] Failed to open NVS for MAC mapping");
            return false;
        }

        size_t len = prefs.getBytes(NVS_KEY_MAC_MAPPING, &table, sizeof(MacMappingTable));
        prefs.end();

        if (len != sizeof(MacMappingTable) || !isMacMappingValid(table)) {
            Serial.println("[WARN] Invalid MAC mapping table, initializing empty");
            initMacMappingTable(table);
            return false;
        }

        Serial.printf("[OK] MAC mapping table loaded (%d entries)\n", table.count);
        return true;
    }

    // Save MAC mapping table
    bool saveMacMapping(const MacMappingTable& table) {
        if (!isMacMappingValid(table)) {
            Serial.println("[ERROR] Invalid MAC mapping table");
            return false;
        }

        if (!prefs.begin(NVS_NAMESPACE, false)) {
            Serial.println("[ERROR] Failed to open NVS for writing MAC mapping");
            return false;
        }

        size_t written = prefs.putBytes(NVS_KEY_MAC_MAPPING, &table, sizeof(MacMappingTable));
        prefs.end();

        if (written != sizeof(MacMappingTable)) {
            Serial.println("[ERROR] Failed to write MAC mapping table");
            return false;
        }

        Serial.printf("[OK] MAC mapping table saved (%d entries)\n", table.count);
        return true;
    }

    // Add or update a MAC mapping
    bool addMacMapping(MacMappingTable& table, const uint8_t* mac, uint8_t lampIndex) {
        if (lampIndex > LAMP_INDEX_MAX) {
            Serial.printf("[ERROR] Invalid lamp index: %d\n", lampIndex);
            return false;
        }

        // Check if MAC already exists and update it
        for (uint8_t i = 0; i < table.count; i++) {
            if (memcmp(table.entries[i].mac, mac, MAC_PARTIAL_BYTES) == 0) {
                table.entries[i].lampIndex = lampIndex;
                Serial.printf("[INFO] Updated MAC mapping: %02X:%02X:%02X -> Lamp %d\n",
                              mac[0], mac[1], mac[2], lampIndex);
                return saveMacMapping(table);
            }
        }

        // Add new entry if space available
        if (table.count < MAX_MAC_MAPPINGS) {
            memcpy(table.entries[table.count].mac, mac, MAC_PARTIAL_BYTES);
            table.entries[table.count].lampIndex = lampIndex;
            table.count++;
            Serial.printf("[INFO] Added MAC mapping: %02X:%02X:%02X -> Lamp %d\n",
                          mac[0], mac[1], mac[2], lampIndex);
            return saveMacMapping(table);
        }

        Serial.println("[ERROR] MAC mapping table full");
        return false;
    }

    // Remove a MAC mapping
    bool removeMacMapping(MacMappingTable& table, const uint8_t* mac) {
        for (uint8_t i = 0; i < table.count; i++) {
            if (memcmp(table.entries[i].mac, mac, MAC_PARTIAL_BYTES) == 0) {
                // Shift remaining entries down
                for (uint8_t j = i; j < table.count - 1; j++) {
                    table.entries[j] = table.entries[j + 1];
                }
                table.count--;
                Serial.printf("[INFO] Removed MAC mapping: %02X:%02X:%02X\n",
                              mac[0], mac[1], mac[2]);
                return saveMacMapping(table);
            }
        }

        Serial.println("[WARN] MAC mapping not found");
        return false;
    }

    // Find lamp index by MAC address
    int8_t findLampIndex(const MacMappingTable& table, const uint8_t* mac) {
        for (uint8_t i = 0; i < table.count; i++) {
            if (memcmp(table.entries[i].mac, mac, MAC_PARTIAL_BYTES) == 0) {
                return table.entries[i].lampIndex;
            }
        }
        return -1;  // Not found
    }
};

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

class DebugHelper {
public:
    static void printGameConfig(const GameConfig& cfg) {
        Serial.println("\n=== GAME CONFIG ===");
        Serial.printf("GodShot Window: %d ms\n", cfg.godShotWindow);
        Serial.printf("Perfect Window: %d ms\n", cfg.perfectWindow);
        Serial.printf("Start Speed: %d ms/lamp\n", cfg.startSpeed);
        Serial.printf("Speed Profile: %d\n", cfg.speedProfile);
        Serial.printf("Winning Score: %d\n", cfg.winningScore);
        Serial.printf("Timing Mode: %d\n", cfg.timingMode);
        Serial.printf("Animation Mode: %d\n", cfg.animationMode);
    }

    static void printVisualConfig(const VisualConfig& cfg) {
        Serial.println("\n=== VISUAL CONFIG ===");
        Serial.printf("Idle Effect: %d\n", cfg.idleEffect);
        Serial.printf("Global Brightness: %d%%\n", cfg.globalBrightness);
        Serial.printf("Score Display Mode: %d\n", cfg.scoreDisplayMode);
        Serial.printf("Player 1 Color: 0x%06X\n", cfg.player1Color);
        Serial.printf("Player 2 Color: 0x%06X\n", cfg.player2Color);
    }

    static void printStatistics(const Statistics& stats) {
        Serial.println("\n=== STATISTICS ===");
        Serial.printf("Total Games: %lu\n", stats.totalGames);
        Serial.printf("P1 Wins: %lu | P2 Wins: %lu\n", stats.player1Wins, stats.player2Wins);
        Serial.printf("GodShots: %lu | Perfect: %lu | Miss: %lu\n",
                      stats.godShotCount, stats.perfectCount, stats.missCount);
        Serial.printf("Avg Reaction: %.1f ms\n", getAverageReactionTime(stats));
        Serial.printf("Best: %d ms | Worst: %d ms\n",
                      stats.bestReactionTime, stats.worstReactionTime);
    }
};

#endif // PONG_COMMON_H
