#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "Protocol.h"
#include "Constants.h"

// =============================================================================
// GAME CONFIGURATION STRUCTURE
// =============================================================================

struct GameConfig {
    // Timing settings
    uint16_t godShotWindow;         // Milliseconds (default: 100)
    uint16_t perfectWindow;         // Milliseconds (default: 600)

    // Speed settings
    uint16_t startSpeed;            // ms per lamp (default: 200)
    uint16_t speedIncrement;        // Linear increment per round (default: 20)
    float speedMultiplier;          // Exponential multiplier (default: 1.15)
    uint16_t maxSpeed;              // Speed cap ms per lamp (default: 50)
    uint8_t speedProfile;           // 0=Linear, 1=Exponential, 2=Custom

    // Game rules
    uint8_t winningScore;           // Points to win (default: 3)

    // Timing mode
    uint8_t timingMode;             // TimingMode enum (default: TIMING_FULL_BRIGHT)

    // Animation mode
    uint8_t animationMode;          // AnimationMode enum (default: ANIM_SMOOTH_STRIP)

    // Validation marker
    uint32_t magic;                 // Magic number for validation (0x504F4E47 = "PONG")
} __attribute__((packed));

struct VisualConfig {
    // Display settings
    uint8_t scoreDisplayMode;       // 0=Edges, 1=Minimal (default: 0)
    uint8_t idleEffect;             // IdleEffect enum (default: IDLE_RAINBOW)
    uint8_t globalBrightness;       // 0-100 percentage (default: 80)

    // Effect settings
    uint16_t idleSpeed;             // Animation speed ms (default: 50)
    uint8_t cobStripBalance;        // COB vs Strip brightness ratio 0-100 (default: 50)

    // Colors
    uint32_t player1Color;          // RGB color for player 1 (default: 0x00FF00 green)
    uint32_t player2Color;          // RGB color for player 2 (default: 0xFF0000 red)
    uint32_t neutralColor;          // RGB color for neutral (default: 0x0000FF blue)

    // Validation marker
    uint32_t magic;                 // Magic number for validation (0x504F4E47 = "PONG")
} __attribute__((packed));

// =============================================================================
// DEFAULT VALUES
// =============================================================================

#define CONFIG_MAGIC 0x504F4E47  // "PONG" in hex

// Game defaults
#define DEFAULT_GODSHOT_WINDOW 100
#define DEFAULT_PERFECT_WINDOW 600
#define DEFAULT_START_SPEED 200
#define DEFAULT_SPEED_INCREMENT 20
#define DEFAULT_SPEED_MULTIPLIER 1.15f
#define DEFAULT_MAX_SPEED 50
#define DEFAULT_SPEED_PROFILE 0  // Linear
#define DEFAULT_WINNING_SCORE 3
#define DEFAULT_TIMING_MODE TIMING_FULL_BRIGHT
#define DEFAULT_ANIMATION_MODE ANIM_SMOOTH_STRIP

// Visual defaults
#define DEFAULT_SCORE_DISPLAY_MODE 0  // Edges
#define DEFAULT_IDLE_EFFECT IDLE_RAINBOW
#define DEFAULT_GLOBAL_BRIGHTNESS 80
#define DEFAULT_IDLE_SPEED 50
#define DEFAULT_COB_STRIP_BALANCE 50
#define DEFAULT_PLAYER1_COLOR 0x00FF00  // Green
#define DEFAULT_PLAYER2_COLOR 0xFF0000  // Red
#define DEFAULT_NEUTRAL_COLOR 0x0000FF  // Blue

// =============================================================================
// STATISTICS STRUCTURE
// =============================================================================

struct Statistics {
    // Game statistics
    uint32_t totalGames;
    uint32_t player1Wins;
    uint32_t player2Wins;

    // Timing statistics
    uint32_t totalPresses;
    uint32_t godShotCount;
    uint32_t perfectCount;
    uint32_t missCount;

    // Reaction times (milliseconds)
    uint32_t totalReactionTime;     // Sum for averaging
    uint16_t bestReactionTime;      // Fastest ever
    uint16_t worstReactionTime;     // Slowest perfect (not miss)

    // Per-player stats
    uint32_t player1GodShots;
    uint32_t player2GodShots;
    uint32_t player1TotalReaction;
    uint32_t player2TotalReaction;
    uint16_t player1Presses;
    uint16_t player2Presses;

    // Session stats (not persisted)
    uint32_t sessionGames;
    uint32_t sessionStartTime;

    // Validation marker
    uint32_t magic;
} __attribute__((packed));

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

inline void initDefaultGameConfig(GameConfig& config) {
    config.godShotWindow = DEFAULT_GODSHOT_WINDOW;
    config.perfectWindow = DEFAULT_PERFECT_WINDOW;
    config.startSpeed = DEFAULT_START_SPEED;
    config.speedIncrement = DEFAULT_SPEED_INCREMENT;
    config.speedMultiplier = DEFAULT_SPEED_MULTIPLIER;
    config.maxSpeed = DEFAULT_MAX_SPEED;
    config.speedProfile = DEFAULT_SPEED_PROFILE;
    config.winningScore = DEFAULT_WINNING_SCORE;
    config.timingMode = DEFAULT_TIMING_MODE;
    config.animationMode = DEFAULT_ANIMATION_MODE;
    config.magic = CONFIG_MAGIC;
}

inline void initDefaultVisualConfig(VisualConfig& config) {
    config.scoreDisplayMode = DEFAULT_SCORE_DISPLAY_MODE;
    config.idleEffect = DEFAULT_IDLE_EFFECT;
    config.globalBrightness = DEFAULT_GLOBAL_BRIGHTNESS;
    config.idleSpeed = DEFAULT_IDLE_SPEED;
    config.cobStripBalance = DEFAULT_COB_STRIP_BALANCE;
    config.player1Color = DEFAULT_PLAYER1_COLOR;
    config.player2Color = DEFAULT_PLAYER2_COLOR;
    config.neutralColor = DEFAULT_NEUTRAL_COLOR;
    config.magic = CONFIG_MAGIC;
}

inline void initStatistics(Statistics& stats) {
    memset(&stats, 0, sizeof(Statistics));
    stats.bestReactionTime = 0xFFFF;  // Max value
    stats.worstReactionTime = 0;
    stats.magic = CONFIG_MAGIC;
}

inline bool isConfigValid(const GameConfig& config) {
    return config.magic == CONFIG_MAGIC;
}

inline bool isConfigValid(const VisualConfig& config) {
    return config.magic == CONFIG_MAGIC;
}

inline bool isStatsValid(const Statistics& stats) {
    return stats.magic == CONFIG_MAGIC;
}

// Calculate speed based on profile
inline uint16_t calculateSpeed(const GameConfig& config, uint16_t round) {
    uint16_t speed;

    switch (config.speedProfile) {
        case 0:  // Linear
            speed = config.startSpeed - (round * config.speedIncrement);
            break;

        case 1:  // Exponential
            speed = config.startSpeed / pow(config.speedMultiplier, round);
            break;

        case 2:  // Custom (can be extended)
            speed = config.startSpeed - (round * config.speedIncrement);
            break;

        default:
            speed = config.startSpeed;
    }

    // Apply speed cap
    return max((uint16_t)config.maxSpeed, (uint16_t)speed);
}

// Calculate average reaction time
inline float getAverageReactionTime(const Statistics& stats) {
    if (stats.totalPresses == 0) return 0.0f;
    return (float)stats.totalReactionTime / (float)stats.totalPresses;
}

inline float getPlayerAverageReaction(const Statistics& stats, Player player) {
    if (player == PLAYER_LEFT) {
        return stats.player1Presses > 0 ?
            (float)stats.player1TotalReaction / (float)stats.player1Presses : 0.0f;
    } else {
        return stats.player2Presses > 0 ?
            (float)stats.player2TotalReaction / (float)stats.player2Presses : 0.0f;
    }
}

// =============================================================================
// MAC MAPPING STRUCTURE
// =============================================================================

struct MacMapping {
    uint8_t mac[MAC_PARTIAL_BYTES];  // Last 3 bytes of MAC address
    uint8_t lampIndex;               // Lamp position (0-10)
} __attribute__((packed));

struct MacMappingTable {
    uint8_t count;                   // Number of valid entries
    MacMapping entries[MAX_MAC_MAPPINGS];
    uint32_t magic;                  // Validation marker
} __attribute__((packed));

// Initialize empty MAC mapping table
inline void initMacMappingTable(MacMappingTable& table) {
    memset(&table, 0, sizeof(MacMappingTable));
    table.magic = CONFIG_MAGIC;
}

inline bool isMacMappingValid(const MacMappingTable& table) {
    return table.magic == CONFIG_MAGIC && table.count <= MAX_MAC_MAPPINGS;
}

// =============================================================================
// NVS/PREFERENCES KEYS
// =============================================================================

#define NVS_NAMESPACE "pong"
#define NVS_KEY_GAME_CONFIG "game_cfg"
#define NVS_KEY_VISUAL_CONFIG "vis_cfg"
#define NVS_KEY_STATISTICS "stats"
#define NVS_KEY_DEVICE_ID "dev_id"
#define NVS_KEY_MAC_MAPPING "mac_map"

#endif // CONFIG_H
