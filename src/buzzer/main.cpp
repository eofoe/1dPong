#include <Arduino.h>
#include <WiFi.h>
#include <PongCommon.h>
#include <BuzzerState.h>
#include <CommonUtils.h>

/*
 * ============================================================================
 * 1D PONG - BUZZER FIRMWARE (Refactored)
 * ============================================================================
 *
 * This firmware handles button input from players and sends press events
 * to the sequencer via ESP-NOW.
 *
 * Player assignment is auto-detected via GPIO pin or MAC address.
 *
 * SETUP INSTRUCTIONS:
 * 1. Flash this firmware to both buzzer devices (same code for both)
 * 2. Connect GPIO4 to GND for LEFT player, or leave floating for RIGHT player
 * 3. Power on and press button to test
 *
 * ============================================================================
 */

// =============================================================================
// GLOBAL STATE
// =============================================================================

// State manager (encapsulates all buzzer state)
BuzzerStateManager buzzerState;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

void IRAM_ATTR buttonISR();
void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len);
void onESPNowSend(const uint8_t* mac, esp_now_send_status_t status);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(BOOT_DELAY_MS);

    Serial.println("\n\n========================================");
    Serial.println("   1D PONG - BUZZER (Refactored)");
    Serial.println("========================================");
    Serial.printf("MAC Address: %s\n", PongCommon::getMacAddress().c_str());

    // Configure button pin
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    // Configure LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize buzzer state
    if (!buzzerState.init()) {
        Serial.println("[FATAL] Buzzer state initialization failed!");
        LEDHelper::blinkError(LED_BUILTIN);
        while (1) delay(1000);
    }

    // Initialize WiFi (Station mode for ESP-NOW)
    WiFiManager::initStation(ESPNOW_CHANNEL);

    // Initialize ESP-NOW
    if (!PongCommon::initESPNow()) {
        Serial.println("[FATAL] ESP-NOW init failed!");
        LEDHelper::blinkError(LED_BUILTIN);
        while (1) delay(1000);
    }

    // Register callbacks
    esp_now_register_recv_cb(onESPNowReceive);
    esp_now_register_send_cb(onESPNowSend);

    // Add broadcast peer (for initial discovery)
    PongCommon::addBroadcastPeer();

    // Initialize OTA
    String hostname = "1dPong-Buzzer-";
    hostname += (buzzerState.playerAssignment == PLAYER_LEFT) ? "Left" : "Right";
    OTAManager::initOTA(hostname, DEFAULT_OTA_PASSWORD);

    Serial.println("\n[OK] Buzzer ready!");
    Serial.println("Press button to send input to Sequencer");

    // Startup indication
    LEDHelper::blinkConfirm(LED_BUILTIN);
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    ArduinoOTA.handle();

    unsigned long now = millis();

    // Check for button press event from ISR
    if (buzzerState.checkButtonPressed()) {
        if (!buzzerState.buttonWasPressed) {
            // Button just pressed
            buzzerState.buttonWasPressed = true;
            buzzerState.buttonPressStart = now;

            Serial.println("[BUTTON] Pressed");
            digitalWrite(LED_BUILTIN, HIGH);

            // Send button press message
            if (buzzerState.sendButtonPress()) {
                Serial.printf("[TX] Button press sent (player: %d)\n",
                             buzzerState.playerAssignment);
                // Quick LED blink to confirm send
                delay(LED_SEND_CONFIRM_MS);
                digitalWrite(LED_BUILTIN, LOW);
            } else {
                Serial.println("[ERROR] Failed to send button press");
                LEDHelper::blink(LED_BUILTIN, 2, LED_ERROR_BLINK_MS);
            }
        }
    }

    // Check if button was released
    if (buzzerState.buttonWasPressed && digitalRead(BUTTON_PIN) == HIGH) {
        unsigned long pressDuration = now - buzzerState.buttonPressStart;

        Serial.printf("[BUTTON] Released (duration: %lu ms)\n", pressDuration);
        digitalWrite(LED_BUILTIN, LOW);

        // Check for long press (factory reset)
        if (buzzerState.isLongPress()) {
            Serial.println("[BUTTON] Long press detected - factory reset");
            LEDHelper::blink(LED_BUILTIN, ERROR_BLINK_RESET_COUNT, LED_ERROR_BLINK_MS * 2);
            buzzerState.configManager.clearAll();
            delay(WIFI_CONNECT_DELAY_MS);
            ESP.restart();
        }

        buzzerState.buttonWasPressed = false;
    }

    // Periodic heartbeat (optional)
    if (buzzerState.needsHeartbeat()) {
        // buzzerState.sendStatusResponse();
    }

    delay(DISPLAY_UPDATE_INTERVAL_MS);
}

// =============================================================================
// BUTTON INTERRUPT
// =============================================================================

void IRAM_ATTR buttonISR() {
    unsigned long now = millis();
    if (now - buzzerState.lastButtonTime > BUTTON_DEBOUNCE_MS) {
        buzzerState.buttonPressed = true;
        buzzerState.lastButtonTime = now;
    }
}

// =============================================================================
// ESP-NOW CALLBACKS
// =============================================================================

void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < sizeof(MessageHeader)) return;

    MessageHeader* header = (MessageHeader*)data;

    // Learn sequencer MAC address
    buzzerState.learnSequencerMAC(mac, header->senderId);

    switch (header->type) {
        case MSG_STATUS_REQUEST:
            buzzerState.sendStatusResponse();
            Serial.println("[TX] Status response sent");
            break;

        case MSG_PING:
            Serial.println("[RX] Ping received");
            break;

        case MSG_RESET:
            Serial.println("[RX] Reset command received");
            delay(OTA_DELAY_BEFORE_RESTART_MS);
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
