#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Arduino.h>
#include "Config.h"
#include "Protocol.h"
#include "Constants.h"
#include "PongCommon.h"

// =============================================================================
// GAME STATE ENUM
// =============================================================================

enum GameState {
    STATE_IDLE,
    STATE_WAITING_START,
    STATE_BALL_MOVING,
    STATE_WAITING_PRESS,
    STATE_EFFECT_PLAYING,
    STATE_ROUND_END,
    STATE_GAME_OVER
};

// =============================================================================
// DEVICE INFO STRUCTURE
// =============================================================================

struct DeviceInfo {
    uint8_t macAddr[6];
    int8_t rssi;
    uint32_t lastSeen;
    bool active;
};

// =============================================================================
// GAME STATE MANAGER CLASS
// =============================================================================

class GameStateManager {
public:
    // Configuration
    GameConfig gameConfig;
    VisualConfig visualConfig;
    Statistics stats;
    ConfigManager configManager;
    MacMappingTable macMapping;
    uint8_t deviceId;

    // Game state
    GameState currentState;
    Player currentPlayer;
    uint8_t leftScore;
    uint8_t rightScore;
    uint16_t currentRound;
    uint16_t currentSpeed;
    uint8_t currentLampPosition;
    uint32_t targetReachedTime;
    bool gameActive;

    // Device tracking
    DeviceInfo buzzerLeft;
    DeviceInfo buzzerRight;
    DeviceInfo lamps[NUM_LAMPS];
    uint8_t numActiveLamps;

    // Timing
    unsigned long lastUpdate;
    unsigned long stateStartTime;

    // Constructor
    GameStateManager() {
        deviceId = 0;
        currentState = STATE_IDLE;
        currentPlayer = PLAYER_RIGHT;
        leftScore = 0;
        rightScore = 0;
        currentRound = 0;
        currentSpeed = DEFAULT_START_SPEED;
        currentLampPosition = 0;
        targetReachedTime = 0;
        gameActive = false;
        numActiveLamps = 0;
        lastUpdate = 0;
        stateStartTime = 0;

        memset(&buzzerLeft, 0, sizeof(DeviceInfo));
        memset(&buzzerRight, 0, sizeof(DeviceInfo));
        memset(lamps, 0, sizeof(lamps));
    }

    // Initialize
    bool init() {
        // Load MAC mapping table
        if (!configManager.loadMacMapping(macMapping)) {
            initMacMappingTable(macMapping);
        }

        // Load configurations
        if (!configManager.loadGameConfig(gameConfig)) {
            initDefaultGameConfig(gameConfig);
            configManager.saveGameConfig(gameConfig);
        }

        if (!configManager.loadVisualConfig(visualConfig)) {
            initDefaultVisualConfig(visualConfig);
            configManager.saveVisualConfig(visualConfig);
        }

        if (!configManager.loadStatistics(stats)) {
            initStatistics(stats);
            configManager.saveStatistics(stats);
        }

        deviceId = configManager.getDeviceId();
        if (deviceId == 0) {
            deviceId = DEVICEID_SEQUENCER_DEFAULT;
            configManager.setDeviceId(deviceId);
        }

        DebugHelper::printGameConfig(gameConfig);
        DebugHelper::printVisualConfig(visualConfig);
        DebugHelper::printStatistics(stats);

        return true;
    }

    // Start new game
    void startNewGame() {
        Serial.println("\n[GAME] Starting new game!");

        leftScore = 0;
        rightScore = 0;
        currentRound = 0;
        gameActive = true;
        stats.sessionGames++;

        currentPlayer = PLAYER_RIGHT;
        currentSpeed = gameConfig.startSpeed;

        sendScoreUpdate();

        currentState = STATE_WAITING_START;
        stateStartTime = millis();
    }

    // Start new round
    void startNewRound() {
        currentRound++;
        currentSpeed = calculateSpeed(gameConfig, currentRound);

        Serial.printf("[GAME] Round %d - Speed: %d ms/lamp\n", currentRound, currentSpeed);

        // Switch player
        currentPlayer = (currentPlayer == PLAYER_LEFT) ? PLAYER_RIGHT : PLAYER_LEFT;

        currentState = STATE_WAITING_START;
        stateStartTime = millis();
    }

    // Award point to player
    void awardPoint(Player player) {
        if (player == PLAYER_LEFT) {
            leftScore++;
            Serial.printf("[GAME] Left player scores! Score: %d - %d\n", leftScore, rightScore);
        } else {
            rightScore++;
            Serial.printf("[GAME] Right player scores! Score: %d - %d\n", leftScore, rightScore);
        }

        // Update statistics
        if (leftScore >= gameConfig.winningScore) {
            stats.totalGames++;
            stats.player1Wins++;
            Serial.println("[GAME] ★★★ LEFT PLAYER WINS! ★★★");
        } else if (rightScore >= gameConfig.winningScore) {
            stats.totalGames++;
            stats.player2Wins++;
            Serial.println("[GAME] ★★★ RIGHT PLAYER WINS! ★★★");
        }

        configManager.saveStatistics(stats);
        sendScoreUpdate();
    }

    // Process reaction time
    void processReactionTime(uint32_t reactionTime, Player player) {
        Serial.printf("[GAME] Reaction time: %lu ms\n", reactionTime);

        // Update statistics
        stats.totalPresses++;
        stats.totalReactionTime += reactionTime;

        if (player == PLAYER_LEFT) {
            stats.player1Presses++;
            stats.player1TotalReaction += reactionTime;
        } else {
            stats.player2Presses++;
            stats.player2TotalReaction += reactionTime;
        }

        if (reactionTime < stats.bestReactionTime) {
            stats.bestReactionTime = reactionTime;
        }

        EffectType effect;
        bool pointScored = false;
        Player winner = PLAYER_NONE;

        // Determine outcome
        if (reactionTime <= gameConfig.godShotWindow) {
            // GODSHOT!
            Serial.println("[GAME] ★ GODSHOT! ★");
            effect = EFFECT_GODSHOT;
            stats.godShotCount++;

            if (player == PLAYER_LEFT) {
                stats.player1GodShots++;
            } else {
                stats.player2GodShots++;
            }

            // Super speed boost
            currentSpeed = max(gameConfig.maxSpeed, (uint16_t)(currentSpeed / 2));

        } else if (reactionTime <= gameConfig.perfectWindow) {
            // Perfect timing
            Serial.println("[GAME] Perfect!");
            effect = EFFECT_PERFECT;
            stats.perfectCount++;

            if (reactionTime > stats.worstReactionTime && reactionTime <= gameConfig.perfectWindow) {
                stats.worstReactionTime = reactionTime;
            }

        } else {
            // Miss - opponent gets point
            Serial.println("[GAME] Miss!");
            effect = EFFECT_MISS;
            stats.missCount++;
            pointScored = true;
            winner = (player == PLAYER_LEFT) ? PLAYER_RIGHT : PLAYER_LEFT;
        }

        // Save statistics
        configManager.saveStatistics(stats);

        // Send effect to all lamps
        sendEffect(effect, player);

        currentState = STATE_EFFECT_PLAYING;
        stateStartTime = millis();

        if (pointScored) {
            awardPoint(winner);
        }
    }

    // Send score update
    void sendScoreUpdate() {
        LampScoreMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_LAMP_SCORE_DISPLAY, deviceId);
        msg.leftScore = leftScore;
        msg.rightScore = rightScore;
        msg.displayMode = visualConfig.scoreDisplayMode;

        PongCommon::broadcastMessage(&msg, sizeof(msg));
        Serial.printf("[TX] Score update sent: %d - %d\n", leftScore, rightScore);
    }

    // Send idle command
    void sendIdleCommand() {
        LampIdleMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_LAMP_IDLE, deviceId);
        msg.idleEffect = visualConfig.idleEffect;
        msg.brightness = visualConfig.globalBrightness;
        msg.speed = visualConfig.idleSpeed;

        PongCommon::broadcastMessage(&msg, sizeof(msg));
        Serial.printf("[TX] Idle command sent (effect: %d)\n", visualConfig.idleEffect);
    }

    // Send effect
    void sendEffect(EffectType effect, Player player) {
        LampEffectMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_LAMP_EFFECT, deviceId);
        msg.lampIndex = 0xFF;  // All lamps
        msg.effectType = effect;
        msg.param1 = player;
        msg.param2 = 0;

        PongCommon::broadcastMessage(&msg, sizeof(msg));
    }

    // Send ball position
    void sendBallPosition(uint8_t lampIndex, Player direction) {
        LampPositionMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_LAMP_SET_POSITION, deviceId);
        msg.lampIndex = lampIndex;
        msg.position = 0;
        msg.speed = currentSpeed;
        msg.direction = direction;

        PongCommon::broadcastMessage(&msg, sizeof(msg));
    }

    // Broadcast configuration
    void broadcastConfig() {
        LampConfigMessage msg;
        PongCommon::initMessageHeader(msg.header, MSG_LAMP_CONFIG, deviceId);
        msg.animationMode = gameConfig.animationMode;
        msg.timingMode = gameConfig.timingMode;
        msg.globalBrightness = visualConfig.globalBrightness;
        msg.reserved = 0;

        PongCommon::broadcastMessage(&msg, sizeof(msg));
        Serial.println("[TX] Configuration broadcast sent");
    }

    // Update device tracking
    void updateDeviceTracking(const uint8_t* mac, Player player) {
        unsigned long now = millis();

        if (player == PLAYER_LEFT) {
            memcpy(buzzerLeft.macAddr, mac, 6);
            buzzerLeft.lastSeen = now;
            buzzerLeft.active = true;
        } else {
            memcpy(buzzerRight.macAddr, mac, 6);
            buzzerRight.lastSeen = now;
            buzzerRight.active = true;
        }
    }

    // Check device timeouts
    void checkDeviceTimeouts() {
        unsigned long now = millis();

        for (int i = 0; i < NUM_LAMPS; i++) {
            if (lamps[i].active && (now - lamps[i].lastSeen > DEVICE_TIMEOUT_MS)) {
                lamps[i].active = false;
                numActiveLamps--;
                Serial.printf("[WARN] Lamp %d timeout\n", i);
            }
        }
    }
};

#endif // GAME_STATE_H
