#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <PongCommon.h>

/*
 * ============================================================================
 * 1D PONG - LAMP FIRMWARE (Sequencer-Oriented Architecture)
 * ============================================================================
 *
 * This is a "dumb" lamp that only displays what the sequencer sends.
 * All effects and animations are calculated centrally by the sequencer.
 *
 * Benefits:
 * - Perfect synchronization across all lamps
 * - Easier debugging (all logic in one place)
 * - Dynamic effects without reflashing lamps
 *
 * Drawbacks:
 * - Higher network bandwidth
 * - More CPU load on sequencer
 * - Dependency on network connection
 *
 * SETUP: Same as autonomous version (see MAC mapping below)
 *
 * ============================================================================
 */

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

// =============================================================================
// LAMP POSITION MAPPING (Same as autonomous version)
// =============================================================================

struct MacToLamp {
    uint8_t mac[3];
    uint8_t lampIndex;
};

MacToLamp macMapping[] = {
    // Add your MAC mappings here
};

const uint8_t MAC_MAPPING_COUNT = sizeof(macMapping) / sizeof(MacToLamp);

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

CRGB leds[NUM_STRIP_LEDS];
ConfigManager configManager;

uint8_t lampIndex = 0;
uint8_t deviceId = 0;

uint8_t sequencerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool sequencerKnown = false;

uint32_t bootTime = 0;
uint32_t lastFrameTime = 0;
uint32_t frameCount = 0;

// Statistics
uint32_t framesReceived = 0;
uint32_t framesMissed = 0;
uint32_t lastSecondFrames = 0;
uint32_t fpsCounter = 0;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void detectLampIndex();
void initLEDs();
void initOTA();
void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len);
void handleLampFrame(const LampFrameMessage& msg);
void setCobBrightness(uint8_t brightness);
void sendStatusResponse();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   1D PONG - LAMP (Sequencer-Oriented)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    bootTime = millis();

    // Detect lamp position
    detectLampIndex();

    deviceId = configManager.getDeviceId();
    if (deviceId == 0) {
        deviceId = 11 + lampIndex;
        configManager.setDeviceId(deviceId);
    }

    Serial.printf("Lamp Index: %d\n", lampIndex);
    Serial.printf("Device ID: %d\n", deviceId);
    Serial.println("Mode: SEQUENCER-ORIENTED (dumb display)");

    // Initialize LEDs
    initLEDs();

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        // Show error
        fill_solid(leds, NUM_STRIP_LEDS, CRGB::Red);
        FastLED.show();
        while (1) delay(1000);
    }

    esp_now_register_recv_cb(onESPNowReceive);
    PongCommon::addBroadcastPeer();

    // Initialize OTA
    initOTA();

    Serial.println("\n[OK] Lamp ready - waiting for frames from sequencer!");

    // Startup flash
    fill_solid(leds, NUM_STRIP_LEDS, CRGB::White);
    FastLED.show();
    setCobBrightness(255);
    delay(200);
    fill_solid(leds, NUM_STRIP_LEDS, CRGB::Black);
    FastLED.show();
    setCobBrightness(0);
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();

    unsigned long now = millis();

    // FPS counter
    if (now - lastSecondFrames > 1000) {
        fpsCounter = framesReceived - frameCount;
        frameCount = framesReceived;
        lastSecondFrames = now;

        // Print stats every 10 seconds
        static uint32_t lastStatsPrint = 0;
        if (now - lastStatsPrint > 10000) {
            Serial.printf("[STATS] FPS: %lu, Total: %lu, Missed: %lu\n",
                          fpsCounter, framesReceived, framesMissed);
            lastStatsPrint = now;
        }
    }

    // Watchdog: If no frames for 5 seconds, show warning
    if (now - lastFrameTime > 5000 && framesReceived > 0) {
        static uint32_t lastWarning = 0;
        if (now - lastWarning > 5000) {
            Serial.println("[WARN] No frames from sequencer!");
            lastWarning = now;
        }
    }

    delay(10);
}

// =============================================================================
// LAMP POSITION DETECTION
// =============================================================================

void detectLampIndex() {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    // Check NVS first
    uint8_t storedIndex = configManager.getDeviceId();
    if (storedIndex > 0 && storedIndex <= NUM_LAMPS + 10) {
        lampIndex = storedIndex - 11;
        Serial.printf("[CONFIG] Lamp index from NVS: %d\n", lampIndex);
        return;
    }

    // Check MAC mapping table
    bool found = false;
    for (uint8_t i = 0; i < MAC_MAPPING_COUNT; i++) {
        if (mac[3] == macMapping[i].mac[0] &&
            mac[4] == macMapping[i].mac[1] &&
            mac[5] == macMapping[i].mac[2]) {
            lampIndex = macMapping[i].lampIndex;
            found = true;
            Serial.printf("[CONFIG] Lamp index from MAC mapping: %d\n", lampIndex);
            configManager.setDeviceId(11 + lampIndex);
            break;
        }
    }

    if (!found) {
        lampIndex = mac[5] % NUM_LAMPS;
        Serial.printf("[WARN] MAC not in mapping table, using fallback: %d\n", lampIndex);
        Serial.println("[INFO] Add this MAC to macMapping[] in lamp/main.cpp:");
        Serial.printf("       {{0x%02X, 0x%02X, 0x%02X}, %d},  // Lamp %d\n",
                      mac[3], mac[4], mac[5], lampIndex, lampIndex);
        configManager.setDeviceId(11 + lampIndex);
    }

    Serial.printf("[CONFIG] Final lamp index: %d (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                  lampIndex, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// =============================================================================
// LED CONTROL
// =============================================================================

void initLEDs() {
    // Initialize FastLED
    FastLED.addLeds<WS2812B, STRIP_LED_PIN, GRB>(leds, NUM_STRIP_LEDS);
    FastLED.setBrightness(255);
    FastLED.clear();
    FastLED.show();

    // Configure COB LED PWM
    pinMode(COB_LED_PIN, OUTPUT);
    ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit
    ledcAttachPin(COB_LED_PIN, 0);
    ledcWrite(0, 0);

    Serial.println("[LED] FastLED initialized");
}

void setCobBrightness(uint8_t brightness) {
    ledcWrite(0, brightness);
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
        case MSG_LAMP_FRAME:
            if (len == sizeof(LampFrameMessage)) {
                handleLampFrame(*(LampFrameMessage*)data);
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
            // Ignore old-style messages in sequencer-oriented mode
            break;
    }
}

void handleLampFrame(const LampFrameMessage& msg) {
    // Only process frames for this lamp
    if (msg.lampIndex != lampIndex && msg.lampIndex != 0xFF) {
        return;
    }

    // Update statistics
    framesReceived++;
    lastFrameTime = millis();

    // Set COB brightness
    setCobBrightness(msg.cobBrightness);

    // Update LED strip
    uint8_t numLeds = min((uint8_t)NUM_STRIP_LEDS, msg.numLeds);
    for (uint8_t i = 0; i < numLeds; i++) {
        uint8_t r = msg.rgbData[i * 3];
        uint8_t g = msg.rgbData[i * 3 + 1];
        uint8_t b = msg.rgbData[i * 3 + 2];
        leds[i] = CRGB(r, g, b);
    }

    FastLED.show();
}

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
}

// =============================================================================
// OTA UPDATES
// =============================================================================

void initOTA() {
    String hostname = "1dPong-Lamp-SO-" + String(lampIndex);
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPassword("1dPongOTA");

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Update starting...");
        fill_solid(leds, NUM_STRIP_LEDS, CRGB::Blue);
        FastLED.show();
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
        fill_solid(leds, NUM_STRIP_LEDS, CRGB::Green);
        FastLED.show();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint16_t numLit = (NUM_STRIP_LEDS * progress) / total;
        fill_solid(leds, NUM_STRIP_LEDS, CRGB::Black);
        for (uint16_t i = 0; i < numLit; i++) {
            leds[i] = CRGB::Blue;
        }
        FastLED.show();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", error);
        fill_solid(leds, NUM_STRIP_LEDS, CRGB::Red);
        FastLED.show();
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}
