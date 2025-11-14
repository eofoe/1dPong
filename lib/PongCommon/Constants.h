#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

// =============================================================================
// TIMING CONSTANTS
// =============================================================================

// Game timing
#define GAME_START_DELAY_MS 1000          // Delay before ball starts moving
#define GAME_EFFECT_DURATION_MS 2000      // Duration of effect playback
#define GAME_ROUND_END_DELAY_MS 1000      // Pause between rounds
#define GAME_OVER_DISPLAY_MS 5000         // How long to show game over screen
#define AUTO_MISS_TIMEOUT_MULTIPLIER 2    // Timeout = perfectWindow * this

// Button debouncing
#define BUTTON_DEBOUNCE_MS 50             // Button debounce time
#define BUTTON_LONG_PRESS_MS 3000         // Long press duration for special functions

// Status and heartbeat
#define HEARTBEAT_INTERVAL_MS 30000       // Lamp status report interval
#define BUZZER_HEARTBEAT_MS 10000         // Buzzer status interval
#define DEVICE_TIMEOUT_MS 10000           // Consider device offline after this

// Display update
#define DISPLAY_UPDATE_INTERVAL_MS 10     // Main loop delay
#define PERIODIC_UPDATE_INTERVAL_MS 1000  // Periodic tasks interval

// Boot and initialization
#define BOOT_DELAY_MS 1000                // Initial boot delay
#define LAMP_BOOT_WAIT_MS 500             // Wait for lamps to boot before sending commands
#define WIFI_CONNECT_TIMEOUT_ATTEMPTS 20  // WiFi connection attempts
#define WIFI_CONNECT_DELAY_MS 500         // Delay between WiFi attempts

// LED feedback
#define LED_FLASH_DURATION_MS 200         // Startup flash duration
#define LED_FLASH_CLEAR_MS 200            // Time between flashes
#define LED_SEND_CONFIRM_MS 50            // Quick blink on send
#define LED_ERROR_BLINK_MS 50             // Fast blink for errors
#define LED_STARTUP_BLINK_MS 200          // Startup indication blink

// OTA update
#define OTA_PROGRESS_INTERVAL_PERCENT 10  // Report OTA progress every N%
#define OTA_DELAY_BEFORE_RESTART_MS 100   // Delay before restart after command

// =============================================================================
// HARDWARE CONSTANTS
// =============================================================================

// Pin definitions (can be overridden by build flags)
#ifndef COB_LED_PIN
#define COB_LED_PIN 12                    // COB LED PWM pin
#endif

#ifndef STRIP_LED_PIN
#define STRIP_LED_PIN 13                  // WS2812B strip data pin
#endif

#ifndef NUM_STRIP_LEDS
#define NUM_STRIP_LEDS 20                 // Number of LEDs per strip
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN 0                      // Button input pin (Boot button)
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 2                     // Built-in LED
#endif

#ifndef PLAYER_DETECT_PIN
#define PLAYER_DETECT_PIN 4               // Player assignment detection pin
#endif

// =============================================================================
// NETWORK CONSTANTS
// =============================================================================

// WiFi and ESP-NOW
#define WIFI_CONNECT_TIMEOUT_SEC 10       // Total WiFi connection timeout
#define ESPNOW_RETRY_COUNT 3              // Number of retries for failed sends
#define ESPNOW_RETRY_DELAY_MS 10          // Delay between retries

// Web server
#define WEB_SERVER_PORT 80                // HTTP server port
#define WEB_REQUEST_TIMEOUT_MS 5000       // Web request timeout

// Access Point defaults
#define DEFAULT_AP_SSID "1dPong-Config"   // Default AP SSID
#define DEFAULT_AP_PASSWORD "pong1234"    // Default AP password (SHOULD BE CHANGED!)
#define DEFAULT_OTA_PASSWORD "1dPongOTA"  // Default OTA password (SHOULD BE CHANGED!)

// =============================================================================
// DISPLAY CONSTANTS
// =============================================================================

// Brightness and colors
#define MIN_BRIGHTNESS 0                  // Minimum LED brightness
#define MAX_BRIGHTNESS 255                // Maximum LED brightness
#define DIM_BACKGROUND_BRIGHTNESS 20      // Dim background when ball not present
#define DIM_BACKGROUND_RGB 10             // RGB value for dim background

// Animation
#define DEFAULT_ANIMATION_STEPS 100       // Animation smoothness

// =============================================================================
// DEVICE ID RANGES
// =============================================================================

// Device ID allocation
#define DEVICEID_SEQUENCER_DEFAULT 1      // Default sequencer ID
#define DEVICEID_LAMP_OFFSET 10           // Lamp IDs start at 11 (10 + index)
#define DEVICEID_LAMP_MIN 11              // Minimum lamp device ID
#define DEVICEID_LAMP_MAX 21              // Maximum lamp device ID (11 lamps: 11-21)

// =============================================================================
// ERROR CODES
// =============================================================================

#define ERROR_BLINK_FAST_COUNT 10         // Number of fast blinks for errors
#define ERROR_BLINK_SLOW_COUNT 3          // Number of slow blinks for warnings
#define ERROR_BLINK_CONFIRM_COUNT 3       // Number of blinks for confirmation
#define ERROR_BLINK_RESET_COUNT 5         // Number of blinks for factory reset

// =============================================================================
// MAC MAPPING CONSTANTS
// =============================================================================

#define MAX_MAC_MAPPINGS 15               // Maximum number of MAC address mappings
#define MAC_ADDRESS_BYTES 6               // Size of MAC address
#define MAC_PARTIAL_BYTES 3               // Last 3 bytes used for mapping

// =============================================================================
// VALIDATION CONSTANTS
// =============================================================================

#define LAMP_INDEX_MAX 10                 // Maximum lamp index (0-10 = 11 lamps)
#define PLAYER_COUNT 2                    // Number of players
#define MAX_SCORE 10                      // Maximum score in config

#endif // CONSTANTS_H
