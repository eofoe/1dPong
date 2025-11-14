#ifndef BUZZER_STATE_H
#define BUZZER_STATE_H

#include <Arduino.h>
#include "Config.h"
#include "Protocol.h"
#include "Constants.h"
#include "PongCommon.h"

// =============================================================================
// BUZZER STATE MANAGER CLASS
// =============================================================================

class BuzzerStateManager {
public:
    // Configuration
    uint8_t deviceId;
    Player playerAssignment;
    ConfigManager configManager;

    // Sequencer tracking
    uint8_t sequencerMac[6];
    bool sequencerKnown;

    // Button state
    volatile bool buttonPressed;
    volatile unsigned long lastButtonTime;
    unsigned long buttonPressStart;
    bool buttonWasPressed;

    // Timing
    uint32_t lastHeartbeat;
    uint32_t bootTime;

    // Constructor
    BuzzerStateManager() {
        deviceId = 0;
        playerAssignment = PLAYER_RIGHT;  // Default
        memset(sequencerMac, 0xFF, 6);
        sequencerKnown = false;
        buttonPressed = false;
        lastButtonTime = 0;
        buttonPressStart = 0;
        buttonWasPressed = false;
        lastHeartbeat = 0;
        bootTime = 0;
    }

    // Initialize
    bool init() {
        bootTime = millis();

        // Detect player assignment
        detectPlayerAssignment();

        // Load or generate device ID
        deviceId = configManager.getDeviceId();
        if (deviceId == 0) {
            // Generate unique ID from MAC address
            uint8_t mac[6];
            WiFi.macAddress(mac);
            deviceId = mac[5];
            configManager.setDeviceId(deviceId);
        }

        Serial.printf("[INIT] Device ID: %d, Player: %s\n",
                      deviceId,
                      playerAssignment == PLAYER_LEFT ? "LEFT" : "RIGHT");
        return true;
    }

    // Detect player assignment based on GPIO pin
    void detectPlayerAssignment() {
        #ifdef PLAYER_DETECT_PIN
        pinMode(PLAYER_DETECT_PIN, INPUT_PULLUP);
        delay(10);

        // If pin is LOW, assign as Player 1 (LEFT)
        // If pin is HIGH/floating, assign as Player 2 (RIGHT)
        if (digitalRead(PLAYER_DETECT_PIN) == LOW) {
            playerAssignment = PLAYER_LEFT;
        } else {
            playerAssignment = PLAYER_RIGHT;
        }
        #else
        // Fallback: Use device ID
        // Odd IDs = Player 1, Even IDs = Player 2
        playerAssignment = (deviceId % 2 == 0) ? PLAYER_RIGHT : PLAYER_LEFT;
        #endif

        Serial.printf("[CONFIG] Auto-detected player: %s\n",
                      playerAssignment == PLAYER_LEFT ? "LEFT (P1)" : "RIGHT (P2)");
    }

    // Learn sequencer MAC address
    void learnSequencerMAC(const uint8_t* mac, uint8_t senderId) {
        if (!sequencerKnown && senderId == DEVICEID_SEQUENCER_DEFAULT) {
            memcpy(sequencerMac, mac, 6);
            PongCommon::addPeer(sequencerMac);
            sequencerKnown = true;
            Serial.printf("[ESPNOW] Sequencer discovered: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }

    // Send button press message
    bool sendButtonPress() {
        ButtonPressMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_BUTTON_PRESS, deviceId);
        msg.player = playerAssignment;
        msg.pressTime = micros();

        return PongCommon::sendMessage(sequencerMac, &msg, sizeof(msg));
    }

    // Send status response
    bool sendStatusResponse() {
        StatusResponseMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_STATUS_RESPONSE, deviceId);
        msg.deviceType = DEVICE_BUZZER;
        msg.deviceId = deviceId;
        msg.rssi = WiFi.RSSI();
        msg.batteryLevel = 0;  // Not applicable for powered device
        msg.uptime = getUptime();
        msg.freeHeap = ESP.getFreeHeap() / 1024;

        return PongCommon::sendMessage(sequencerMac, &msg, sizeof(msg));
    }

    // Check if heartbeat needed
    bool needsHeartbeat() {
        unsigned long now = millis();
        if (now - lastHeartbeat > BUZZER_HEARTBEAT_MS) {
            lastHeartbeat = now;
            return true;
        }
        return false;
    }

    // Get uptime in seconds
    uint32_t getUptime() {
        return (millis() - bootTime) / 1000;
    }

    // Handle button ISR flag
    bool checkButtonPressed() {
        unsigned long now = millis();
        if (buttonPressed && (now - lastButtonTime > BUTTON_DEBOUNCE_MS)) {
            buttonPressed = false;
            return true;
        }
        return false;
    }

    // Check for long press
    bool isLongPress() {
        unsigned long pressDuration = millis() - buttonPressStart;
        return pressDuration > BUTTON_LONG_PRESS_MS;
    }
};

#endif // BUZZER_STATE_H
