#include <Arduino.h>
#include <WiFi.h>
#include <FastLED.h>
#include <PongCommon.h>
#include <LedEffects.h>
#include <LampState.h>
#include <CommonUtils.h>

/*
 * ============================================================================
 * 1D PONG - LAMP FIRMWARE (Refactored)
 * ============================================================================
 *
 * This firmware uses MAC-based lamp positioning stored in NVS.
 * Configuration is managed via the sequencer's web interface.
 *
 * SETUP INSTRUCTIONS:
 * 1. Flash this firmware to all 11 lamps (same code for all)
 * 2. Power on each lamp and note its MAC address from serial output
 * 3. Use the sequencer's web interface to map MAC addresses to lamp positions
 * 4. Mappings are automatically synced to all lamps via ESP-NOW
 *
 * ============================================================================
 */

// =============================================================================
// GLOBAL STATE
// =============================================================================

// LED strip and effects
CRGB leds[NUM_STRIP_LEDS];
LedEffects effects(leds, NUM_STRIP_LEDS, COB_LED_PIN);

// State manager (encapsulates all lamp state)
LampStateManager lampState;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void initLEDs();
void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len);
void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status);
void sendStatusResponse();
void handleLampPosition(const LampPositionMessage& msg);
void handleLampBrightness(const LampBrightnessMessage& msg);
void handleLampEffect(const LampEffectMessage& msg);
void handleLampIdle(const LampIdleMessage& msg);
void handleLampScore(const LampScoreMessage& msg);
void handleLampConfig(const LampConfigMessage& msg);
void updateLampDisplay();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(BOOT_DELAY_MS);

    Serial.println("\n\n========================================");
    Serial.println("   1D PONG - LAMP (Refactored)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    // Initialize lamp state
    if (!lampState.init()) {
        Serial.println("[FATAL] Lamp state initialization failed!");
        LEDHelper::blinkError(LED_BUILTIN);
        while (1) delay(1000);
    }

    // Initialize LEDs
    initLEDs();

    // Initialize WiFi (Station mode for ESP-NOW)
    WiFiManager::initStation(ESPNOW_CHANNEL);

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        effects.setAll(CRGB::Red, MAX_BRIGHTNESS);
        effects.show();
        LEDHelper::blinkError(LED_BUILTIN);
        while (1) delay(1000);
    }

    // Register callbacks
    esp_now_register_recv_cb(onESPNowReceive);
    esp_now_register_send_cb(onESPNowSend);

    // Add broadcast peer
    PongCommon::addBroadcastPeer();

    // Initialize OTA with LED feedback
    OTAManager::initOTA(
        "1dPong-Lamp-" + String(lampState.lampIndex),
        []() {
            Serial.println("[OTA] Update starting...");
            effects.setAll(CRGB::Blue, 100);
            effects.show();
        },
        []() {
            Serial.println("\n[OTA] Update complete!");
            effects.setAll(CRGB::Green, 100);
            effects.show();
        },
        [](unsigned int progress, unsigned int total) {
            static uint8_t lastPercent = 0;
            uint8_t percent = (progress / (total / 100));
            if (percent != lastPercent && percent % OTA_PROGRESS_INTERVAL_PERCENT == 0) {
                Serial.printf("[OTA] Progress: %u%%\n", percent);
                lastPercent = percent;

                // Show progress on strip
                uint16_t numLit = (NUM_STRIP_LEDS * percent) / 100;
                effects.clear();
                for (uint16_t i = 0; i < numLit; i++) {
                    effects.setPixel(i, CRGB::Blue);
                }
                effects.show();
            }
        },
        [](ota_error_t error) {
            Serial.printf("[OTA] Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");

            effects.setAll(CRGB::Red, MAX_BRIGHTNESS);
            effects.show();
        }
    );

    // Set initial brightness and animation speed
    effects.setBrightness(lampState.visualConfig.globalBrightness * MAX_BRIGHTNESS / 100);
    effects.setUpdateInterval(lampState.visualConfig.idleSpeed);

    Serial.println("\n[OK] Lamp ready!");
    Serial.printf("Starting with idle effect: %d\n", lampState.visualConfig.idleEffect);

    // Startup flash
    effects.setAll(CRGB::White, MAX_BRIGHTNESS);
    effects.show();
    delay(LED_FLASH_DURATION_MS);
    effects.clear();
    effects.show();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();

    // Update display based on current state
    updateLampDisplay();

    // Periodic status report
    if (lampState.needsHeartbeat()) {
        sendStatusResponse();
    }

    delay(DISPLAY_UPDATE_INTERVAL_MS);
}

// =============================================================================
// LED INITIALIZATION
// =============================================================================

void initLEDs() {
    FastLED.addLeds<WS2812B, STRIP_LED_PIN, GRB>(leds, NUM_STRIP_LEDS);
    FastLED.setBrightness(MAX_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    Serial.println("[LED] FastLED initialized");
}

// =============================================================================
// DISPLAY UPDATE
// =============================================================================

void updateLampDisplay() {
    switch (lampState.currentState) {
        case LAMP_IDLE:
            effects.updateIdleEffect(lampState.currentIdleEffect);
            break;

        case LAMP_BALL_ACTIVE:
            if (lampState.ballAtThisLamp) {
                effects.showBallPosition(lampState.lampIndex, lampState.ballSubPosition,
                                       lampState.ballDirection, lampState.ballColor);
            } else {
                // Dim background when ball not present
                effects.setAll(CRGB(DIM_BACKGROUND_RGB, DIM_BACKGROUND_RGB, DIM_BACKGROUND_RGB),
                             DIM_BACKGROUND_BRIGHTNESS);
                effects.show();
            }
            break;

        case LAMP_EFFECT_PLAYING:
            // Effect is playing (handled in message handler)
            break;

        case LAMP_SCORE_DISPLAY:
            // Score display is static (set in message handler)
            break;
    }
}

// =============================================================================
// ESP-NOW MESSAGE HANDLERS
// =============================================================================

void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < sizeof(MessageHeader)) return;

    MessageHeader* header = (MessageHeader*)data;

    // Learn sequencer MAC
    lampState.learnSequencerMAC(mac, header->senderId);

    switch (header->type) {
        case MSG_LAMP_SET_POSITION:
            if (len == sizeof(LampPositionMessage)) {
                handleLampPosition(*(LampPositionMessage*)data);
            }
            break;

        case MSG_LAMP_SET_BRIGHTNESS:
            if (len == sizeof(LampBrightnessMessage)) {
                handleLampBrightness(*(LampBrightnessMessage*)data);
            }
            break;

        case MSG_LAMP_EFFECT:
            if (len == sizeof(LampEffectMessage)) {
                handleLampEffect(*(LampEffectMessage*)data);
            }
            break;

        case MSG_LAMP_IDLE:
            if (len == sizeof(LampIdleMessage)) {
                handleLampIdle(*(LampIdleMessage*)data);
            }
            break;

        case MSG_LAMP_SCORE_DISPLAY:
            if (len == sizeof(LampScoreMessage)) {
                handleLampScore(*(LampScoreMessage*)data);
            }
            break;

        case MSG_LAMP_CONFIG:
            if (len == sizeof(LampConfigMessage)) {
                handleLampConfig(*(LampConfigMessage*)data);
            }
            break;

        case MSG_STATUS_REQUEST:
            sendStatusResponse();
            break;

        case MSG_RESET:
            Serial.println("[CMD] Reset received");
            delay(OTA_DELAY_BEFORE_RESTART_MS);
            ESP.restart();
            break;

        default:
            break;
    }
}

void handleLampPosition(const LampPositionMessage& msg) {
    // Determine ball color based on direction
    uint32_t color = (msg.direction == PLAYER_LEFT) ?
                     lampState.visualConfig.player1Color :
                     lampState.visualConfig.player2Color;

    lampState.updateBallPosition(msg.lampIndex, msg.position, (Player)msg.direction, color);

    if (lampState.ballAtThisLamp) {
        Serial.printf("[BALL] Ball at this lamp (pos: %d, dir: %d)\n",
                      lampState.ballSubPosition, lampState.ballDirection);
    }
}

void handleLampBrightness(const LampBrightnessMessage& msg) {
    // Check if message is for this lamp or all lamps
    if (msg.lampIndex == lampState.lampIndex || msg.lampIndex == 0xFF) {
        uint8_t r, g, b, w;
        PongCommon::colorToRGBW(msg.color, r, g, b, w);

        CRGB color = CRGB(r, g, b);
        effects.setAll(color, msg.cobBrightness);
        effects.show();

        Serial.printf("[LED] Brightness set: COB=%d, Color=0x%08X\n",
                      msg.cobBrightness, msg.color);
    }
}

void handleLampEffect(const LampEffectMessage& msg) {
    // Check if effect is for this lamp or all lamps
    if (msg.lampIndex != lampState.lampIndex && msg.lampIndex != 0xFF) {
        return;
    }

    lampState.setEffectPlaying();

    Serial.printf("[EFFECT] Playing effect %d\n", msg.effectType);

    Player player = (Player)msg.param1;
    uint32_t playerColor = (player == PLAYER_LEFT) ?
                           lampState.visualConfig.player1Color :
                           lampState.visualConfig.player2Color;

    switch ((EffectType)msg.effectType) {
        case EFFECT_GODSHOT:
            effects.effectGodShot(player);
            break;

        case EFFECT_PERFECT:
            effects.effectPerfect(player);
            break;

        case EFFECT_MISS:
            effects.effectMiss(player);
            break;

        case EFFECT_POINT_WIN:
            effects.effectPointWin(player, playerColor);
            break;

        case EFFECT_POINT_LOSE:
            effects.effectPointLose(player);
            break;

        case EFFECT_GAME_WIN:
            effects.effectGameWin(playerColor);
            break;

        case EFFECT_GAME_LOSE:
            effects.effectGameLose(playerColor);
            break;

        default:
            break;
    }
}

void handleLampIdle(const LampIdleMessage& msg) {
    lampState.setIdle((IdleEffect)msg.idleEffect,
                      map(msg.brightness, 0, MAX_BRIGHTNESS, 0, 100),
                      msg.speed);

    effects.setBrightness(msg.brightness);
    effects.setUpdateInterval(msg.speed);

    Serial.printf("[IDLE] Mode set to %d (brightness: %d, speed: %d)\n",
                  msg.idleEffect, msg.brightness, msg.speed);
}

void handleLampScore(const LampScoreMessage& msg) {
    lampState.setScoreDisplay();

    effects.showScore(lampState.lampIndex, msg.leftScore, msg.rightScore,
                      lampState.visualConfig.player1Color,
                      lampState.visualConfig.player2Color);

    Serial.printf("[SCORE] Display: %d - %d\n", msg.leftScore, msg.rightScore);
}

void handleLampConfig(const LampConfigMessage& msg) {
    lampState.gameConfig.animationMode = msg.animationMode;
    lampState.gameConfig.timingMode = msg.timingMode;
    lampState.visualConfig.globalBrightness = msg.globalBrightness;

    lampState.configManager.saveGameConfig(lampState.gameConfig);
    lampState.configManager.saveVisualConfig(lampState.visualConfig);

    effects.setBrightness(msg.globalBrightness * MAX_BRIGHTNESS / 100);

    Serial.println("[CONFIG] Configuration updated");
}

void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status) {
    // Optional: Track send status
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[WARN] ESP-NOW send failed");
    }
}

// =============================================================================
// STATUS REPORTING
// =============================================================================

void sendStatusResponse() {
    StatusResponseMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_STATUS_RESPONSE, lampState.deviceId);
    msg.deviceType = DEVICE_LAMP;
    msg.deviceId = lampState.deviceId;
    msg.rssi = WiFi.RSSI();
    msg.batteryLevel = 0;
    msg.uptime = lampState.getUptime();
    msg.freeHeap = ESP.getFreeHeap() / 1024;

    PongCommon::sendMessage(lampState.sequencerMac, &msg, sizeof(msg));
    Serial.printf("[STATUS] Sent (RSSI: %d dBm, Heap: %d KB)\n",
                  msg.rssi, msg.freeHeap);
}
