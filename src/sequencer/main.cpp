#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PongCommon.h>
#include <CommonUtils.h>
#include <Constants.h>

// =============================================================================
// GAME STATE MACHINE
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
// DEVICE TRACKING STRUCTURE (must be before webserver.h include)
// =============================================================================

struct DeviceInfo {
    uint8_t macAddr[6];
    int8_t rssi;
    uint32_t lastSeen;
    bool active;
};

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Configuration
GameConfig gameConfig;
VisualConfig visualConfig;
Statistics stats;
ConfigManager configManager;
MacMappingTable macMapping;

// Web server
AsyncWebServer server(80);

// Game state
GameState currentState = STATE_IDLE;
Player currentPlayer = PLAYER_RIGHT;  // Start with right player
uint8_t leftScore = 0;
uint8_t rightScore = 0;
uint16_t currentRound = 0;
uint16_t currentSpeed = DEFAULT_START_SPEED;
uint8_t currentLampPosition = 0;
uint32_t targetReachedTime = 0;  // When ball reached target lamp
bool gameActive = false;

// Device tracking
DeviceInfo buzzerLeft = {0};
DeviceInfo buzzerRight = {0};
DeviceInfo lamps[NUM_LAMPS] = {0};
uint8_t numActiveLamps = 0;

// Timing
unsigned long lastUpdate = 0;
unsigned long stateStartTime = 0;

// Include webserver after DeviceInfo is defined
#include "webserver.h"

// Device ID
uint8_t deviceId = 0;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void initWiFi();
void initWebServer();
void initOTA();
void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len);
void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status);
void handleButtonPress(const ButtonPressMessage& msg, const uint8_t* mac);
void updateGameLogic();
void startNewGame();
void startNewRound();
void moveBallToLamp(uint8_t lampIndex, Player direction);
void processReactionTime(uint32_t reactionTime, Player player);
void awardPoint(Player player);
void sendIdleCommand();
void sendScoreUpdate();
void broadcastConfig();
String getIdleEffectName(uint8_t effect);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(BOOT_DELAY_MS);

    Serial.println("\n\n========================================");
    Serial.println("   1D PONG - SEQUENCER (Refactored)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    // Load MAC mapping table
    if (!configManager.loadMacMapping(macMapping)) {
        initMacMappingTable(macMapping);
    }

    // Load configuration
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

    // Initialize WiFi
    initWiFi();

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        while (1) delay(1000);
    }

    // Register callbacks
    esp_now_register_recv_cb(onESPNowReceive);
    esp_now_register_send_cb(onESPNowSend);

    // Add broadcast peer
    PongCommon::addBroadcastPeer();

    // Initialize web server
    initWebServer();

    // Initialize OTA
    OTAManager::initOTA("1dPong-Sequencer", DEFAULT_OTA_PASSWORD);

    // Send initial idle command
    delay(LAMP_BOOT_WAIT_MS);  // Wait for lamps to boot
    sendIdleCommand();

    Serial.println("\n[OK] Sequencer ready!");
    Serial.printf("Web UI: http://%s\n", WiFi.localIP().toString().c_str());

    currentState = STATE_IDLE;
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();
    updateGameLogic();

    // Periodic tasks
    unsigned long now = millis();
    if (now - lastUpdate > PERIODIC_UPDATE_INTERVAL_MS) {
        lastUpdate = now;

        // Update device timeout status
        for (int i = 0; i < NUM_LAMPS; i++) {
            if (lamps[i].active && (now - lamps[i].lastSeen > DEVICE_TIMEOUT_MS)) {
                lamps[i].active = false;
                numActiveLamps--;
                Serial.printf("[WARN] Lamp %d timeout\n", i);
            }
        }
    }

    delay(DISPLAY_UPDATE_INTERVAL_MS);
}

// =============================================================================
// GAME LOGIC
// =============================================================================

void updateGameLogic() {
    unsigned long now = millis();

    switch (currentState) {
        case STATE_IDLE:
            // Waiting for game start (triggered by button press)
            break;

        case STATE_WAITING_START:
            // Brief delay before ball starts moving
            if (now - stateStartTime > GAME_START_DELAY_MS) {
                currentState = STATE_BALL_MOVING;
                currentLampPosition = (currentPlayer == PLAYER_RIGHT) ? 0 : NUM_LAMPS - 1;
                moveBallToLamp(currentLampPosition, currentPlayer);
            }
            break;

        case STATE_BALL_MOVING:
            // Ball is moving, waiting to reach target
            // Movement is handled by lamps based on speed parameter
            break;

        case STATE_WAITING_PRESS:
            // Ball reached target, waiting for button press
            if (now - targetReachedTime > gameConfig.perfectWindow * AUTO_MISS_TIMEOUT_MULTIPLIER) {
                // Timeout - auto-miss
                Serial.println("[GAME] Timeout - Auto Miss");
                Player winner = (currentPlayer == PLAYER_LEFT) ? PLAYER_RIGHT : PLAYER_LEFT;
                awardPoint(winner);
                currentState = STATE_ROUND_END;
                stateStartTime = now;
            }
            break;

        case STATE_EFFECT_PLAYING:
            // Effect is playing, wait for completion
            if (now - stateStartTime > GAME_EFFECT_DURATION_MS) {
                if (leftScore >= gameConfig.winningScore || rightScore >= gameConfig.winningScore) {
                    currentState = STATE_GAME_OVER;
                    stateStartTime = now;
                } else {
                    currentState = STATE_ROUND_END;
                    stateStartTime = now;
                }
            }
            break;

        case STATE_ROUND_END:
            // Brief pause between rounds
            if (now - stateStartTime > GAME_ROUND_END_DELAY_MS) {
                startNewRound();
            }
            break;

        case STATE_GAME_OVER:
            // Game finished, show winner
            if (now - stateStartTime > GAME_OVER_DISPLAY_MS) {
                // Return to idle
                leftScore = 0;
                rightScore = 0;
                currentRound = 0;
                gameActive = false;
                currentState = STATE_IDLE;
                sendIdleCommand();
                Serial.println("[GAME] Game Over - Returning to Idle");
            }
            break;
    }
}

void startNewGame() {
    Serial.println("\n[GAME] Starting new game!");

    leftScore = 0;
    rightScore = 0;
    currentRound = 0;
    gameActive = true;
    stats.sessionGames++;

    currentPlayer = PLAYER_RIGHT;  // Right player starts
    currentSpeed = gameConfig.startSpeed;

    sendScoreUpdate();

    currentState = STATE_WAITING_START;
    stateStartTime = millis();
}

void startNewRound() {
    currentRound++;
    currentSpeed = calculateSpeed(gameConfig, currentRound);

    Serial.printf("[GAME] Round %d - Speed: %d ms/lamp\n", currentRound, currentSpeed);

    // Switch player (ball goes to opposite side)
    currentPlayer = (currentPlayer == PLAYER_LEFT) ? PLAYER_RIGHT : PLAYER_LEFT;

    currentState = STATE_WAITING_START;
    stateStartTime = millis();
}

void moveBallToLamp(uint8_t lampIndex, Player direction) {
    LampPositionMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_LAMP_SET_POSITION, deviceId);
    msg.lampIndex = lampIndex;
    msg.position = 0;
    msg.speed = currentSpeed;
    msg.direction = direction;

    PongCommon::broadcastMessage(&msg, sizeof(msg));

    // Calculate when ball will reach target
    uint8_t targetLamp = (direction == PLAYER_RIGHT) ? NUM_LAMPS - 1 : 0;
    uint8_t distance = abs((int)targetLamp - (int)lampIndex);
    uint32_t travelTime = distance * currentSpeed;

    Serial.printf("[GAME] Ball moving from lamp %d to %d (distance: %d, time: %lu ms)\n",
                  lampIndex, targetLamp, distance, travelTime);

    // Schedule transition to waiting state
    // This would ideally be handled by a timer, but for now we'll track in the state machine
    currentState = STATE_BALL_MOVING;
    targetReachedTime = millis() + travelTime;

    // We'll transition to WAITING_PRESS when we detect the ball has reached the target
    // For now, use a simple timer-based approach
}

void handleButtonPress(const ButtonPressMessage& msg, const uint8_t* mac) {
    unsigned long now = millis();
    Player pressingPlayer = (Player)msg.player;

    Serial.printf("[INPUT] Button press from Player %d (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                  msg.player, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Update device tracking
    if (pressingPlayer == PLAYER_LEFT) {
        memcpy(buzzerLeft.macAddr, mac, 6);
        buzzerLeft.lastSeen = now;
        buzzerLeft.active = true;
    } else {
        memcpy(buzzerRight.macAddr, mac, 6);
        buzzerRight.lastSeen = now;
        buzzerRight.active = true;
    }

    // Handle based on game state
    switch (currentState) {
        case STATE_IDLE:
            // First button press starts the game
            startNewGame();
            break;

        case STATE_BALL_MOVING:
            // Early press - might be anticipation or miss
            Serial.println("[GAME] Early press - ignoring");
            break;

        case STATE_WAITING_PRESS: {
            // Check if correct player pressed
            if (pressingPlayer != currentPlayer) {
                Serial.println("[GAME] Wrong player pressed!");
                return;
            }

            // Calculate reaction time
            uint32_t reactionTime = now - targetReachedTime;
            processReactionTime(reactionTime, pressingPlayer);
            break;
        }

        case STATE_GAME_OVER:
            // Start new game
            startNewGame();
            break;

        default:
            break;
    }
}

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
    LampEffectMessage effectMsg;
    PongCommon::initMessageHeader(effectMsg.header, MSG_LAMP_EFFECT, deviceId);
    effectMsg.lampIndex = 0xFF;  // All lamps
    effectMsg.effectType = effect;
    effectMsg.param1 = player;
    effectMsg.param2 = 0;
    PongCommon::broadcastMessage(&effectMsg, sizeof(effectMsg));

    currentState = STATE_EFFECT_PLAYING;
    stateStartTime = millis();

    if (pointScored) {
        awardPoint(winner);
    }
}

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

void sendScoreUpdate() {
    LampScoreMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_LAMP_SCORE_DISPLAY, deviceId);
    msg.leftScore = leftScore;
    msg.rightScore = rightScore;
    msg.displayMode = visualConfig.scoreDisplayMode;

    PongCommon::broadcastMessage(&msg, sizeof(msg));
    Serial.printf("[TX] Score update sent: %d - %d\n", leftScore, rightScore);
}

void sendIdleCommand() {
    LampIdleMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_LAMP_IDLE, deviceId);
    msg.idleEffect = visualConfig.idleEffect;
    msg.brightness = visualConfig.globalBrightness;
    msg.speed = visualConfig.idleSpeed;

    PongCommon::broadcastMessage(&msg, sizeof(msg));
    Serial.printf("[TX] Idle command sent (effect: %s)\n",
                  getIdleEffectName(visualConfig.idleEffect).c_str());
}

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

// =============================================================================
// ESP-NOW CALLBACKS
// =============================================================================

void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < sizeof(MessageHeader)) {
        Serial.println("[WARN] Received message too short");
        return;
    }

    MessageHeader* header = (MessageHeader*)data;

    switch (header->type) {
        case MSG_BUTTON_PRESS:
            if (len == sizeof(ButtonPressMessage)) {
                handleButtonPress(*(ButtonPressMessage*)data, mac);
            }
            break;

        case MSG_STATUS_RESPONSE:
            // Handle status updates from devices
            if (len == sizeof(StatusResponseMessage)) {
                StatusResponseMessage* status = (StatusResponseMessage*)data;
                Serial.printf("[STATUS] Device %d (Type %d): RSSI=%d, Heap=%d KB, Uptime=%lu s\n",
                              status->deviceId, status->deviceType, status->rssi,
                              status->freeHeap, status->uptime);

                // Update tracking based on device type
                if (status->deviceType == DEVICE_LAMP && status->deviceId < NUM_LAMPS) {
                    memcpy(lamps[status->deviceId].macAddr, mac, 6);
                    lamps[status->deviceId].rssi = status->rssi;
                    lamps[status->deviceId].lastSeen = millis();
                    if (!lamps[status->deviceId].active) {
                        lamps[status->deviceId].active = true;
                        numActiveLamps++;
                    }
                }
            }
            break;

        default:
            Serial.printf("[WARN] Unknown message type: 0x%02X\n", header->type);
            break;
    }
}

void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status) {
    // Optional: Track send failures
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[WARN] ESP-NOW send failed");
    }
}

// =============================================================================
// WIFI & WEB SERVER
// =============================================================================

void initWiFi() {
    // Set hostname
    WiFi.setHostname("1dPong-Sequencer");

    // Initialize WiFi (AP+STA mode)
    #ifdef WIFI_SSID
    WiFiManager::initAPandStation(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD,
                                   WIFI_SSID, WIFI_PASSWORD, ESPNOW_CHANNEL);
    #else
    WiFiManager::initDefaultAP(ESPNOW_CHANNEL);
    #endif
}

// initWebServer() is now defined in webserver.h

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

String getIdleEffectName(uint8_t effect) {
    switch (effect) {
        case IDLE_RAINBOW: return "Rainbow";
        case IDLE_BREATHING: return "Breathing";
        case IDLE_KNIGHT_RIDER: return "Knight Rider";
        case IDLE_TWINKLE: return "Twinkle";
        case IDLE_FIRE: return "Fire";
        case IDLE_PULSE: return "Pulse";
        default: return "Unknown";
    }
}
