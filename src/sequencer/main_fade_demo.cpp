#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PongCommon.h>
#include "sequencer_effects.h"

/*
 * ============================================================================
 * SEQUENCER FADE DEMO (Sequencer-Oriented Architecture)
 * ============================================================================
 *
 * This demo showcases the sequencer-oriented architecture with a smooth
 * fade effect traveling across all 11 lamps.
 *
 * Press 'r' in serial monitor to toggle Rainbow mode
 * Press 'f' to toggle Fade wave mode
 * Press 'b' to test Ball animation
 *
 * Demonstrates:
 * - Perfect synchronization across all lamps
 * - Centralized effect calculation
 * - High frame rate updates (configurable)
 *
 * ============================================================================
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#define TARGET_FPS 30  // Target frame rate
#define FRAME_TIME (1000 / TARGET_FPS)

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

SequencerEffects effects;

enum EffectMode {
    MODE_FADE,
    MODE_RAINBOW,
    MODE_BALL
};

EffectMode currentMode = MODE_FADE;
float ballPosition = 0.0f;
float ballVelocity = 0.05f;

unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
unsigned long fpsCounter = 0;
unsigned long lastFpsTime = 0;

uint8_t deviceId = 1;  // Sequencer ID

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void initWiFi();
void initOTA();
void updateEffects();
void handleSerialInput();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   1D PONG - FADE DEMO");
    Serial.println("   Sequencer-Oriented Architecture");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());
    Serial.printf("Target FPS: %d\n", TARGET_FPS);

    // Initialize WiFi
    initWiFi();

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        while (1) delay(1000);
    }

    // Add broadcast peer
    PongCommon::addBroadcastPeer();

    // Initialize OTA
    initOTA();

    // Start effect
    effects.startEffect();

    Serial.println("\n[OK] Sequencer ready!");
    Serial.println("\nControls:");
    Serial.println("  'f' - Fade wave effect");
    Serial.println("  'r' - Rainbow effect");
    Serial.println("  'b' - Ball animation");
    Serial.println("\nStarting fade effect...\n");

    lastFrameTime = millis();
    lastFpsTime = millis();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();
    handleSerialInput();

    unsigned long now = millis();

    // Frame rate limiting
    if (now - lastFrameTime >= FRAME_TIME) {
        lastFrameTime = now;
        frameCount++;

        // Update effect
        updateEffects();

        // Send frames to all lamps
        effects.sendFramesToLamps();
    }

    // FPS counter
    if (now - lastFpsTime >= 1000) {
        fpsCounter = frameCount;
        frameCount = 0;
        lastFpsTime = now;

        Serial.printf("[FPS] %lu fps (Frame #%lu)\n", fpsCounter, effects.getFrameNumber());
    }

    delay(1);
}

// =============================================================================
// EFFECT UPDATE
// =============================================================================

void updateEffects() {
    unsigned long now = millis();

    switch (currentMode) {
        case MODE_FADE:
            effects.updateFadeEffect(now);
            break;

        case MODE_RAINBOW:
            effects.updateRainbowEffect(now);
            break;

        case MODE_BALL:
            // Animate ball position
            ballPosition += ballVelocity;
            if (ballPosition >= 10.0f) {
                ballPosition = 10.0f;
                ballVelocity = -ballVelocity;
            }
            if (ballPosition <= 0.0f) {
                ballPosition = 0.0f;
                ballVelocity = -ballVelocity;
            }

            effects.updateBallEffect(ballPosition, ballVelocity > 0 ? PLAYER_RIGHT : PLAYER_LEFT);
            break;
    }
}

// =============================================================================
// SERIAL INPUT HANDLER
// =============================================================================

void handleSerialInput() {
    if (Serial.available() > 0) {
        char c = Serial.read();

        switch (c) {
            case 'f':
            case 'F':
                currentMode = MODE_FADE;
                effects.startEffect();
                Serial.println("\n[MODE] Fade wave effect activated");
                break;

            case 'r':
            case 'R':
                currentMode = MODE_RAINBOW;
                effects.startEffect();
                Serial.println("\n[MODE] Rainbow effect activated");
                break;

            case 'b':
            case 'B':
                currentMode = MODE_BALL;
                effects.startEffect();
                ballPosition = 0.0f;
                ballVelocity = 0.05f;
                Serial.println("\n[MODE] Ball animation activated");
                break;

            case '+':
                ballVelocity = min(1.0f, ballVelocity * 1.5f);
                Serial.printf("[SPEED] Ball velocity: %.3f\n", ballVelocity);
                break;

            case '-':
                ballVelocity = max(0.01f, ballVelocity / 1.5f);
                Serial.printf("[SPEED] Ball velocity: %.3f\n", ballVelocity);
                break;
        }
    }
}

// =============================================================================
// WIFI INITIALIZATION
// =============================================================================

void initWiFi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname("1dPong-Sequencer-FadeDemo");

    // Start Access Point
    WiFi.softAP("1dPong-FadeDemo", "pong1234");
    Serial.printf("[WiFi] AP started: 1dPong-FadeDemo\n");
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Connect to station if credentials provided
    #ifdef WIFI_SSID
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Connection failed, AP mode only");
    }
    #endif

    // Set WiFi channel for ESP-NOW
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

// =============================================================================
// OTA INITIALIZATION
// =============================================================================

void initOTA() {
    ArduinoOTA.setHostname("1dPong-Sequencer-FadeDemo");
    ArduinoOTA.setPassword("1dPongOTA");

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Update starting...");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}
