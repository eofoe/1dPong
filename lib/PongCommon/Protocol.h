#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// =============================================================================
// ESP-NOW PROTOCOL DEFINITIONS
// =============================================================================

// Message types
enum MessageType : uint8_t {
    // Buzzer -> Sequencer
    MSG_BUTTON_PRESS = 0x01,

    // Sequencer -> Lamps
    MSG_LAMP_SET_POSITION = 0x10,      // Set ball position
    MSG_LAMP_SET_BRIGHTNESS = 0x11,    // Individual lamp control
    MSG_LAMP_EFFECT = 0x12,            // Trigger effect
    MSG_LAMP_IDLE = 0x13,              // Set idle mode
    MSG_LAMP_SCORE_DISPLAY = 0x14,     // Update score display
    MSG_LAMP_CONFIG = 0x15,            // Configuration update
    MSG_LAMP_FRAME = 0x16,             // LED frame data (sequencer-oriented)
    MSG_LAMP_BROADCAST = 0x1F,         // Broadcast to all lamps

    // Bidirectional
    MSG_PING = 0xF0,
    MSG_PONG = 0xF1,
    MSG_STATUS_REQUEST = 0xF2,
    MSG_STATUS_RESPONSE = 0xF3,
    MSG_RESET = 0xFF
};

// Player identification
enum Player : uint8_t {
    PLAYER_LEFT = 0,
    PLAYER_RIGHT = 1,
    PLAYER_NONE = 0xFF
};

// Effect types
enum EffectType : uint8_t {
    EFFECT_NONE = 0,
    EFFECT_GODSHOT = 1,        // Super speed boost visual
    EFFECT_PERFECT = 2,         // Good timing visual
    EFFECT_MISS = 3,            // Failed timing visual
    EFFECT_POINT_WIN = 4,       // Player wins point
    EFFECT_POINT_LOSE = 5,      // Player loses point
    EFFECT_GAME_WIN = 6,        // Player wins game
    EFFECT_GAME_LOSE = 7,       // Player loses game
    EFFECT_IDLE = 8             // Idle animation
};

// Idle effect types
enum IdleEffect : uint8_t {
    IDLE_RAINBOW = 0,
    IDLE_BREATHING = 1,
    IDLE_KNIGHT_RIDER = 2,
    IDLE_TWINKLE = 3,
    IDLE_FIRE = 4,
    IDLE_PULSE = 5,
    IDLE_COUNT = 6  // Total number of effects
};

// Animation modes for ball movement
enum AnimationMode : uint8_t {
    ANIM_DISCRETE = 0,     // Jump between lamps
    ANIM_SMOOTH_FADE = 1,  // Fade transition
    ANIM_SMOOTH_STRIP = 2  // Show position on strip
};

// Timing window start modes
enum TimingMode : uint8_t {
    TIMING_START_FADE = 0,      // Start when fade begins
    TIMING_FULL_BRIGHT = 1,     // Start when fully bright
    TIMING_COMPLETE = 2         // Start when animation complete
};

// =============================================================================
// MESSAGE STRUCTURES
// =============================================================================

// Base message header (all messages start with this)
struct MessageHeader {
    uint8_t type;           // MessageType
    uint8_t senderId;       // Device ID of sender
    uint32_t timestamp;     // Microsecond timestamp
    uint16_t sequenceNum;   // Packet sequence number
} __attribute__((packed));

// Button press from buzzer
struct ButtonPressMessage {
    MessageHeader header;
    uint8_t player;         // PLAYER_LEFT or PLAYER_RIGHT
    uint32_t pressTime;     // Microsecond timestamp of press
} __attribute__((packed));

// Ball position update
struct LampPositionMessage {
    MessageHeader header;
    uint8_t lampIndex;      // 0-10 (11 lamps total)
    uint8_t position;       // 0-255 (sub-lamp position for smooth animation)
    uint16_t speed;         // Movement speed (ms per lamp)
    uint8_t direction;      // 0=left, 1=right
} __attribute__((packed));

// Individual lamp brightness control
struct LampBrightnessMessage {
    MessageHeader header;
    uint8_t lampIndex;      // 0-10 or 0xFF for all
    uint8_t cobBrightness;  // 0-255 for COB LED
    uint8_t stripBrightness; // 0-255 for strip
    uint32_t color;         // RGBW color (8 bits per channel)
} __attribute__((packed));

// Effect trigger
struct LampEffectMessage {
    MessageHeader header;
    uint8_t lampIndex;      // 0-10 or 0xFF for all
    uint8_t effectType;     // EffectType
    uint8_t param1;         // Effect-specific parameter
    uint8_t param2;         // Effect-specific parameter
} __attribute__((packed));

// Idle mode setting
struct LampIdleMessage {
    MessageHeader header;
    uint8_t idleEffect;     // IdleEffect
    uint8_t brightness;     // Global brightness 0-255
    uint16_t speed;         // Animation speed
} __attribute__((packed));

// Score display update
struct LampScoreMessage {
    MessageHeader header;
    uint8_t leftScore;      // 0-3
    uint8_t rightScore;     // 0-3
    uint8_t displayMode;    // Display mode (for future expansion)
} __attribute__((packed));

// Configuration update
struct LampConfigMessage {
    MessageHeader header;
    uint8_t animationMode;  // AnimationMode
    uint8_t timingMode;     // TimingMode
    uint8_t globalBrightness; // 0-100 (percentage)
    uint8_t reserved;       // For future use
} __attribute__((packed));

// Status response
struct StatusResponseMessage {
    MessageHeader header;
    uint8_t deviceType;     // 1=Sequencer, 2=Buzzer, 3=Lamp
    uint8_t deviceId;       // Unique device ID
    int8_t rssi;            // WiFi RSSI
    uint8_t batteryLevel;   // 0-100 (if applicable)
    uint32_t uptime;        // Seconds since boot
    uint16_t freeHeap;      // Free heap in KB
} __attribute__((packed));

// LED Frame data (sequencer-oriented architecture)
struct LampFrameMessage {
    MessageHeader header;
    uint8_t lampIndex;      // Target lamp (0-10)
    uint8_t cobBrightness;  // COB LED brightness 0-255
    uint8_t numLeds;        // Number of LEDs in frame (typically 20)
    uint8_t rgbData[60];    // RGB data: [R0,G0,B0, R1,G1,B1, ... R19,G19,B19]
                            // Max 60 bytes for 20 LEDs
} __attribute__((packed));

// =============================================================================
// CONSTANTS
// =============================================================================

#define ESPNOW_CHANNEL 1
#define MAX_MESSAGE_SIZE 250
#define NUM_LAMPS 11
#define BROADCAST_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// Device type identifiers
#define DEVICE_SEQUENCER 1
#define DEVICE_BUZZER 2
#define DEVICE_LAMP 3

#endif // PROTOCOL_H
