#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PongCommon.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// Button configuration
#ifndef BUTTON_PIN
#define BUTTON_PIN 0  // GPIO0 (Boot button on most ESP32 boards)
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // Built-in LED
#endif

// Debounce settings
#define DEBOUNCE_TIME 50  // milliseconds
#define LONG_PRESS_TIME 3000  // 3 seconds for special functions

// Player assignment (configure via jumper or hardcode)
// Set PLAYER_ID to 0 for left player, 1 for right player
// You can use a jumper on GPIO pin to auto-detect
#define PLAYER_DETECT_PIN 4  // Optional: GPIO4 - pull LOW for Player 1, HIGH/float for Player 2

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

uint8_t deviceId = 0;
Player playerAssignment = PLAYER_RIGHT;  // Default
ConfigManager configManager;

// Sequencer MAC address (will be learned from first message)
uint8_t sequencerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast initially
bool sequencerKnown = false;

// Button state
volatile bool buttonPressed = false;
volatile unsigned long lastButtonTime = 0;
unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;

// Status
uint32_t lastHeartbeat = 0;
uint32_t bootTime = 0;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void IRAM_ATTR buttonISR();
void detectPlayerAssignment();
void sendButtonPress();
void sendStatusResponse();
void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len);
void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status);
void initOTA();
void blinkLED(int count, int delayMs = 100);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   1D PONG - BUZZER (Input Device)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    bootTime = millis();

    // Configure button pin
    pinMode(BUTTON_PIN, INPUT_PULLUP);  // Active LOW
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    // Configure LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Detect player assignment
    detectPlayerAssignment();

    // Load device ID
    deviceId = configManager.getDeviceId();
    if (deviceId == 0) {
        // Generate unique ID from MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        deviceId = mac[5];  // Use last byte of MAC
        configManager.setDeviceId(deviceId);
    }

    Serial.printf("Device ID: %d\n", deviceId);
    Serial.printf("Player Assignment: %s\n",
                  playerAssignment == PLAYER_LEFT ? "LEFT" : "RIGHT");

    // Initialize WiFi (required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        blinkLED(10, 50);  // Fast blink = error
        while (1) delay(1000);
    }

    // Register callbacks
    esp_now_register_recv_cb(onESPNowReceive);
    esp_now_register_send_cb(onESPNowSend);

    // Add broadcast peer (for initial discovery)
    PongCommon::addBroadcastPeer();

    // Initialize OTA
    initOTA();

    Serial.println("\n[OK] Buzzer ready!");
    Serial.println("Press button to send input to Sequencer");

    // Startup indication
    blinkLED(3, 200);
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();

    // Handle button press
    unsigned long now = millis();

    // Check for button press event from ISR
    if (buttonPressed && (now - lastButtonTime > DEBOUNCE_TIME)) {
        buttonPressed = false;

        if (!buttonWasPressed) {
            // Button just pressed
            buttonWasPressed = true;
            buttonPressStart = now;

            Serial.println("[BUTTON] Pressed");
            digitalWrite(LED_BUILTIN, HIGH);

            sendButtonPress();
        }
    }

    // Check if button was released
    if (buttonWasPressed && digitalRead(BUTTON_PIN) == HIGH) {
        unsigned long pressDuration = now - buttonPressStart;

        Serial.printf("[BUTTON] Released (duration: %lu ms)\n", pressDuration);
        digitalWrite(LED_BUILTIN, LOW);

        // Check for long press (special function - e.g., reset or reconfigure)
        if (pressDuration > LONG_PRESS_TIME) {
            Serial.println("[BUTTON] Long press detected - factory reset");
            blinkLED(5, 100);
            configManager.clearAll();
            delay(500);
            ESP.restart();
        }

        buttonWasPressed = false;
    }

    // Periodic heartbeat (every 10 seconds)
    if (now - lastHeartbeat > 10000) {
        lastHeartbeat = now;
        // Could send periodic status update here if needed
        // sendStatusResponse();
    }

    delay(10);
}

// =============================================================================
// BUTTON INTERRUPT
// =============================================================================

void IRAM_ATTR buttonISR() {
    unsigned long now = millis();
    if (now - lastButtonTime > DEBOUNCE_TIME) {
        buttonPressed = true;
        lastButtonTime = now;
    }
}

// =============================================================================
// PLAYER ASSIGNMENT
// =============================================================================

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

// =============================================================================
// ESP-NOW COMMUNICATION
// =============================================================================

void sendButtonPress() {
    ButtonPressMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_BUTTON_PRESS, deviceId);
    msg.player = playerAssignment;
    msg.pressTime = micros();

    bool success = PongCommon::sendMessage(sequencerMac, &msg, sizeof(msg));

    if (success) {
        Serial.printf("[TX] Button press sent (player: %d)\n", playerAssignment);
        // Quick LED blink to confirm send
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
    } else {
        Serial.println("[ERROR] Failed to send button press");
        blinkLED(2, 50);  // Double blink = send error
    }
}

void sendStatusResponse() {
    StatusResponseMessage msg;
    PongCommon::initMessageHeader(msg.header, MSG_STATUS_RESPONSE, deviceId);
    msg.deviceType = DEVICE_BUZZER;
    msg.deviceId = deviceId;
    msg.rssi = WiFi.RSSI();
    msg.batteryLevel = 0;  // Not applicable for powered device
    msg.uptime = (millis() - bootTime) / 1000;
    msg.freeHeap = ESP.getFreeHeap() / 1024;

    PongCommon::sendMessage(sequencerMac, &msg, sizeof(msg));
    Serial.println("[TX] Status response sent");
}

void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < sizeof(MessageHeader)) return;

    MessageHeader* header = (MessageHeader*)data;

    // Learn sequencer MAC address from first message
    if (!sequencerKnown && header->senderId == 1) {  // Assume sequencer has ID 1
        memcpy(sequencerMac, mac, 6);
        PongCommon::addPeer(sequencerMac);
        sequencerKnown = true;

        Serial.printf("[ESPNOW] Sequencer discovered: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    switch (header->type) {
        case MSG_STATUS_REQUEST:
            sendStatusResponse();
            break;

        case MSG_PING:
            Serial.println("[RX] Ping received");
            // Could respond with PONG message
            break;

        case MSG_RESET:
            Serial.println("[RX] Reset command received");
            delay(100);
            ESP.restart();
            break;

        default:
            // Ignore other messages (intended for lamps)
            break;
    }
}

void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[WARN] ESP-NOW send failed");
    }
}

// =============================================================================
// OTA UPDATES
// =============================================================================

void initOTA() {
    // Set hostname based on player assignment
    String hostname = "1dPong-Buzzer-";
    hostname += (playerAssignment == PLAYER_LEFT) ? "Left" : "Right";
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPassword("1dPongOTA");

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Update starting...");
        blinkLED(3, 100);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPercent = 0;
        uint8_t percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
            Serial.printf("[OTA] Progress: %u%%\n", percent);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");

        blinkLED(10, 50);
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void blinkLED(int count, int delayMs) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
}
