#ifndef LAMP_STATE_H
#define LAMP_STATE_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "Protocol.h"
#include "Constants.h"
#include "PongCommon.h"

// =============================================================================
// LAMP STATE ENUM
// =============================================================================

enum LampState {
    LAMP_IDLE,
    LAMP_BALL_ACTIVE,
    LAMP_EFFECT_PLAYING,
    LAMP_SCORE_DISPLAY
};

// =============================================================================
// LAMP STATE MANAGER CLASS
// =============================================================================

class LampStateManager {
public:
    // Configuration
    uint8_t lampIndex;
    uint8_t deviceId;
    VisualConfig visualConfig;
    GameConfig gameConfig;
    ConfigManager configManager;
    MacMappingTable macMapping;

    // Sequencer tracking
    uint8_t sequencerMac[6];
    bool sequencerKnown;

    // Current state
    LampState currentState;
    IdleEffect currentIdleEffect;

    // Ball animation state
    bool ballAtThisLamp;
    uint8_t ballSubPosition;
    Player ballDirection;
    uint32_t ballColor;

    // Timing
    uint32_t lastHeartbeat;
    uint32_t bootTime;
    uint32_t lastUpdate;

    // Constructor
    LampStateManager() {
        lampIndex = 0;
        deviceId = 0;
        memset(sequencerMac, 0xFF, 6);
        sequencerKnown = false;
        currentState = LAMP_IDLE;
        currentIdleEffect = IDLE_RAINBOW;
        ballAtThisLamp = false;
        ballSubPosition = 0;
        ballDirection = PLAYER_RIGHT;
        ballColor = 0xFFFFFF;
        lastHeartbeat = 0;
        bootTime = 0;
        lastUpdate = 0;
    }

    // Initialize configurations
    bool init() {
        bootTime = millis();

        // Load MAC mapping table
        if (!configManager.loadMacMapping(macMapping)) {
            initMacMappingTable(macMapping);
        }

        // Detect lamp index
        if (!detectLampIndex()) {
            Serial.println("[ERROR] Failed to detect lamp index");
            return false;
        }

        // Load configurations
        if (!configManager.loadVisualConfig(visualConfig)) {
            initDefaultVisualConfig(visualConfig);
        }

        if (!configManager.loadGameConfig(gameConfig)) {
            initDefaultGameConfig(gameConfig);
        }

        // Load or generate device ID
        deviceId = configManager.getDeviceId();
        if (deviceId == 0) {
            // Use lamp index as device ID (offset to avoid conflicts)
            deviceId = DEVICEID_LAMP_OFFSET + lampIndex + 1;
            configManager.setDeviceId(deviceId);
        }

        Serial.printf("[INIT] Lamp Index: %d, Device ID: %d\n", lampIndex, deviceId);
        return true;
    }

    // Detect lamp index from MAC mapping or fallback
    bool detectLampIndex() {
        uint8_t fullMAC[6];
        WiFi.macAddress(fullMAC);
        uint8_t partialMAC[MAC_PARTIAL_BYTES];
        memcpy(partialMAC, &fullMAC[3], MAC_PARTIAL_BYTES);

        // First, check if lamp index is already stored in NVS
        uint8_t storedId = configManager.getDeviceId();
        if (storedId >= DEVICEID_LAMP_MIN && storedId <= DEVICEID_LAMP_MAX) {
            lampIndex = storedId - DEVICEID_LAMP_OFFSET - 1;
            Serial.printf("[CONFIG] Lamp index from NVS: %d\n", lampIndex);
            return true;
        }

        // Check MAC mapping table
        int8_t foundIndex = configManager.findLampIndex(macMapping, partialMAC);
        if (foundIndex >= 0) {
            lampIndex = foundIndex;
            Serial.printf("[CONFIG] Lamp index from MAC mapping: %d\n", lampIndex);

            // Save to NVS for faster boot next time
            configManager.setDeviceId(DEVICEID_LAMP_OFFSET + lampIndex + 1);
            return true;
        }

        // Fallback: Use last byte of MAC modulo number of lamps
        lampIndex = fullMAC[5] % NUM_LAMPS;
        Serial.printf("[WARN] MAC not in mapping table, using fallback: %d\n", lampIndex);
        Serial.println("[INFO] Add this lamp to the MAC mapping via web interface");
        Serial.printf("       MAC: %02X:%02X:%02X -> Lamp %d\n",
                      partialMAC[0], partialMAC[1], partialMAC[2], lampIndex);

        // Save fallback to NVS
        configManager.setDeviceId(DEVICEID_LAMP_OFFSET + lampIndex + 1);
        return true;
    }

    // Learn sequencer MAC address
    void learnSequencerMAC(const uint8_t* mac, uint8_t senderId) {
        if (!sequencerKnown && senderId == DEVICEID_SEQUENCER_DEFAULT) {
            memcpy(sequencerMac, mac, 6);
            PongCommon::addPeer(sequencerMac);
            sequencerKnown = true;
            Serial.printf("[ESPNOW] Sequencer learned: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }

    // Update ball position
    void updateBallPosition(uint8_t targetLampIndex, uint8_t position, Player direction, uint32_t color) {
        if (targetLampIndex == lampIndex) {
            ballAtThisLamp = true;
            ballSubPosition = position;
            ballDirection = direction;
            ballColor = color;
            currentState = LAMP_BALL_ACTIVE;
        } else {
            ballAtThisLamp = false;
        }
    }

    // Set idle state
    void setIdle(IdleEffect effect, uint8_t brightness, uint16_t speed) {
        currentIdleEffect = effect;
        visualConfig.idleEffect = effect;
        visualConfig.globalBrightness = brightness;
        visualConfig.idleSpeed = speed;
        currentState = LAMP_IDLE;

        // Save to NVS
        configManager.saveVisualConfig(visualConfig);
    }

    // Set effect playing state
    void setEffectPlaying() {
        currentState = LAMP_EFFECT_PLAYING;
    }

    // Set score display state
    void setScoreDisplay() {
        currentState = LAMP_SCORE_DISPLAY;
    }

    // Check if heartbeat needed
    bool needsHeartbeat() {
        unsigned long now = millis();
        if (now - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
            lastHeartbeat = now;
            return true;
        }
        return false;
    }

    // Get uptime in seconds
    uint32_t getUptime() {
        return (millis() - bootTime) / 1000;
    }
};

#endif // LAMP_STATE_H
