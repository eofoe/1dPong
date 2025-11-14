#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <PongCommon.h>
#include <LedEffects.h>

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#ifndef COB_LED_PIN
#define COB_LED_PIN 12
#endif

#ifndef STRIP_LED_PIN
#define STRIP_LED_PIN 13
#endif

#ifndef NUM_STRIP_LEDS
#define NUM_STRIP_LEDS 20
#endif

// Lamp position detection (using GPIO pins or hardcoded)
// Set jumpers on pins to encode lamp index 0-10 in binary (4 pins needed)
#define LAMP_ID_PIN_0 16
#define LAMP_ID_PIN_1 17
#define LAMP_ID_PIN_2 18
#define LAMP_ID_PIN_3 19

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// LED strip
CRGB leds[NUM_STRIP_LEDS];
LedEffects effects(leds, NUM_STRIP_LEDS, COB_LED_PIN);

// Configuration
uint8_t lampIndex = 0;  // 0-10 (position in the line)
uint8_t deviceId = 0;
ConfigManager configManager;
VisualConfig visualConfig;
GameConfig gameConfig;

// Sequencer MAC address
uint8_t sequencerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool sequencerKnown = false;

// Current state
enum LampState {
    LAMP_IDLE,
    LAMP_BALL_ACTIVE,
    LAMP_EFFECT_PLAYING,
    LAMP_SCORE_DISPLAY
};

LampState currentState = LAMP_IDLE;
IdleEffect currentIdleEffect = IDLE_RAINBOW;

// Ball animation
bool ballAtThisLamp = false;
uint8_t ballSubPosition = 0;
Player ballDirection = PLAYER_RIGHT;
uint32_t ballColor = 0xFFFFFF;  // White by default

// Status
uint32_t lastHeartbeat = 0;
uint32_t bootTime = 0;
uint32_t lastUpdate = 0;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void detectLampIndex();
void initLEDs();
void initOTA();
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
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   1D PONG - LAMP (Display Device)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    bootTime = millis();

    // Detect lamp position
    detectLampIndex();

    // Load configuration
    if (!configManager.loadVisualConfig(visualConfig)) {
        initDefaultVisualConfig(visualConfig);
    }

    if (!configManager.loadGameConfig(gameConfig)) {
        initDefaultGameConfig(gameConfig);
    }

    deviceId = configManager.getDeviceId();
    if (deviceId == 0) {
        // Use lamp index as device ID (offset by 10 to avoid conflicts)
        deviceId = 10 + lampIndex;
        configManager.setDeviceId(deviceId);
    }

    Serial.printf("Lamp Index: %d\n", lampIndex);
    Serial.printf("Device ID: %d\n", deviceId);

    // Initialize LEDs
    initLEDs();

    // Initialize WiFi (required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        // Show error on LEDs
        effects.setAll(CRGB::Red, 255);
        effects.show();
        while (1) delay(1000);
    }

    // Register callbacks
    esp_now_register_recv_cb(onESPNowReceive);
    esp_now_register_send_cb(onESPNowSend);

    // Add broadcast peer
    PongCommon::addBroadcastPeer();

    // Initialize OTA
    initOTA();

    // Set initial brightness
    effects.setBrightness(visualConfig.globalBrightness * 255 / 100);
    effects.setUpdateInterval(visualConfig.idleSpeed);

    Serial.println("\n[OK] Lamp ready!");
    Serial.printf("Starting with idle effect: %d\n", visualConfig.idleEffect);

    // Start in idle mode
    currentState = LAMP_IDLE;
    currentIdleEffect = (IdleEffect)visualConfig.idleEffect;

    // Startup flash
    effects.setAll(CRGB::White, 255);
    effects.show();
    delay(200);
    effects.clear();
    effects.show();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();

    unsigned long now = millis();

    // Update display based on current state
    updateLampDisplay();

    // Periodic status report (every 30 seconds)
    if (now - lastHeartbeat > 30000) {
        lastHeartbeat = now;
        sendStatusResponse();
    }

    delay(10);
}

// =============================================================================
// LAMP POSITION DETECTION
// =============================================================================

void detectLampIndex() {
    #ifdef LAMP_ID_PIN_0
    // Read lamp index from GPIO pins (binary encoding)
    pinMode(LAMP_ID_PIN_0, INPUT_PULLUP);
    pinMode(LAMP_ID_PIN_1, INPUT_PULLUP);
    pinMode(LAMP_ID_PIN_2, INPUT_PULLUP);
    pinMode(LAMP_ID_PIN_3, INPUT_PULLUP);
    delay(10);

    // Read binary value (inverted - LOW = 1, HIGH = 0)
    uint8_t bit0 = (digitalRead(LAMP_ID_PIN_0) == LOW) ? 1 : 0;
    uint8_t bit1 = (digitalRead(LAMP_ID_PIN_1) == LOW) ? 1 : 0;
    uint8_t bit2 = (digitalRead(LAMP_ID_PIN_2) == LOW) ? 1 : 0;
    uint8_t bit3 = (digitalRead(LAMP_ID_PIN_3) == LOW) ? 1 : 0;

    lampIndex = (bit3 << 3) | (bit2 << 2) | (bit1 << 1) | bit0;

    // Validate (must be 0-10)
    if (lampIndex >= NUM_LAMPS) {
        Serial.printf("[WARN] Invalid lamp index %d, defaulting to 0\n", lampIndex);
        lampIndex = 0;
    }
    #else
    // Fallback: Use last digit of MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    lampIndex = mac[5] % NUM_LAMPS;
    #endif

    Serial.printf("[CONFIG] Lamp index: %d\n", lampIndex);
}

// =============================================================================
// LED INITIALIZATION
// =============================================================================

void initLEDs() {
    // Initialize FastLED for RGBW strip
    // Note: FastLED doesn't natively support RGBW, so we'll use RGB mode
    // and control white channel separately if needed
    FastLED.addLeds<WS2812B, STRIP_LED_PIN, GRB>(leds, NUM_STRIP_LEDS);
    FastLED.setBrightness(255);
    FastLED.clear();
    FastLED.show();

    Serial.println("[LED] FastLED initialized");
}

// =============================================================================
// DISPLAY UPDATE
// =============================================================================

void updateLampDisplay() {
    unsigned long now = millis();

    switch (currentState) {
        case LAMP_IDLE:
            // Run idle animation
            effects.updateIdleEffect(currentIdleEffect);
            break;

        case LAMP_BALL_ACTIVE:
            // Show ball position (if ball is at this lamp)
            if (ballAtThisLamp) {
                effects.showBallPosition(lampIndex, ballSubPosition, ballDirection, ballColor);
            } else {
                // Ball not here - dim background
                effects.setAll(CRGB(10, 10, 10), 20);
                effects.show();
            }
            break;

        case LAMP_EFFECT_PLAYING:
            // Effect is being played (handled in message handler)
            // This state prevents idle animation from overwriting effect
            break;

        case LAMP_SCORE_DISPLAY:
            // Score display is static (set in message handler)
            // Just maintain the display
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
    if (!sequencerKnown && header->senderId == 1) {
        memcpy(sequencerMac, mac, 6);
        PongCommon::addPeer(sequencerMac);
        sequencerKnown = true;
        Serial.printf("[ESPNOW] Sequencer learned: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

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
            delay(100);
            ESP.restart();
            break;

        default:
            break;
    }
}

void handleLampPosition(const LampPositionMessage& msg) {
    // Check if ball is at this lamp
    if (msg.lampIndex == lampIndex) {
        ballAtThisLamp = true;
        ballSubPosition = msg.position;
        ballDirection = (Player)msg.direction;

        // Determine ball color based on direction
        ballColor = (ballDirection == PLAYER_LEFT) ?
                    visualConfig.player1Color : visualConfig.player2Color;

        currentState = LAMP_BALL_ACTIVE;

        Serial.printf("[BALL] Ball at this lamp (pos: %d, dir: %d)\n",
                      ballSubPosition, ballDirection);
    } else {
        ballAtThisLamp = false;

        // If ball just left, dim down
        if (currentState == LAMP_BALL_ACTIVE) {
            effects.setAll(CRGB(5, 5, 5), 10);
            effects.show();
        }
    }
}

void handleLampBrightness(const LampBrightnessMessage& msg) {
    // Check if message is for this lamp or all lamps
    if (msg.lampIndex == lampIndex || msg.lampIndex == 0xFF) {
        uint8_t r, g, b, w;
        PongCommon::colorToRGBW(msg.color, r, g, b, w);

        CRGB color = CRGB(r, g, b);
        effects.setAll(color, msg.cobBrightness);
        effects.show();

        Serial.printf("[LED] Brightness set: COB=%d, Strip=%d, Color=0x%08X\n",
                      msg.cobBrightness, msg.stripBrightness, msg.color);
    }
}

void handleLampEffect(const LampEffectMessage& msg) {
    // Check if effect is for this lamp or all lamps
    if (msg.lampIndex != lampIndex && msg.lampIndex != 0xFF) {
        return;
    }

    currentState = LAMP_EFFECT_PLAYING;

    Serial.printf("[EFFECT] Playing effect %d\n", msg.effectType);

    Player player = (Player)msg.param1;
    uint32_t playerColor = (player == PLAYER_LEFT) ?
                           visualConfig.player1Color : visualConfig.player2Color;

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

    // Return to previous state after effect
    // (Sequencer will send new command)
}

void handleLampIdle(const LampIdleMessage& msg) {
    currentIdleEffect = (IdleEffect)msg.idleEffect;
    effects.setBrightness(msg.brightness);
    effects.setUpdateInterval(msg.speed);

    visualConfig.idleEffect = msg.idleEffect;
    visualConfig.globalBrightness = map(msg.brightness, 0, 255, 0, 100);
    visualConfig.idleSpeed = msg.speed;

    configManager.saveVisualConfig(visualConfig);

    currentState = LAMP_IDLE;

    Serial.printf("[IDLE] Mode set to %d (brightness: %d, speed: %d)\n",
                  msg.idleEffect, msg.brightness, msg.speed);
}

void handleLampScore(const LampScoreMessage& msg) {
    currentState = LAMP_SCORE_DISPLAY;

    effects.showScore(lampIndex, msg.leftScore, msg.rightScore,
                      visualConfig.player1Color, visualConfig.player2Color);

    Serial.printf("[SCORE] Display: %d - %d\n", msg.leftScore, msg.rightScore);
}

void handleLampConfig(const LampConfigMessage& msg) {
    gameConfig.animationMode = msg.animationMode;
    gameConfig.timingMode = msg.timingMode;
    visualConfig.globalBrightness = msg.globalBrightness;

    configManager.saveGameConfig(gameConfig);
    configManager.saveVisualConfig(visualConfig);

    effects.setBrightness(msg.globalBrightness * 255 / 100);

    Serial.println("[CONFIG] Configuration updated");
}

void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status) {
    // Optional: Track send status
}

// =============================================================================
// STATUS REPORTING
// =============================================================================

void sendStatusResponse() {
    StatusResponseMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_STATUS_RESPONSE, deviceId);
    msg.deviceType = DEVICE_LAMP;
    msg.deviceId = deviceId;
    msg.rssi = WiFi.RSSI();
    msg.batteryLevel = 0;
    msg.uptime = (millis() - bootTime) / 1000;
    msg.freeHeap = ESP.getFreeHeap() / 1024;

    PongCommon::sendMessage(sequencerMac, &msg, sizeof(msg));
    Serial.printf("[STATUS] Sent (RSSI: %d dBm, Heap: %d KB)\n",
                  msg.rssi, msg.freeHeap);
}

// =============================================================================
// OTA UPDATES
// =============================================================================

void initOTA() {
    String hostname = "1dPong-Lamp-" + String(lampIndex);
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPassword("1dPongOTA");

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Update starting...");
        effects.setAll(CRGB::Blue, 100);
        effects.show();
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
        effects.setAll(CRGB::Green, 100);
        effects.show();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPercent = 0;
        uint8_t percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
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
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");

        effects.setAll(CRGB::Red, 255);
        effects.show();
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}
