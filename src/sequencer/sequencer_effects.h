#ifndef SEQUENCER_EFFECTS_H
#define SEQUENCER_EFFECTS_H

#include <Arduino.h>
#include <PongCommon.h>

/*
 * ============================================================================
 * SEQUENCER-SIDE EFFECTS (Sequencer-Oriented Architecture)
 * ============================================================================
 *
 * All visual effects are calculated here and sent as frames to lamps.
 * This enables perfect synchronization across all 11 lamps.
 *
 * ============================================================================
 */

class SequencerEffects {
private:
    // Frame buffers for each lamp
    struct LampFrame {
        uint8_t cobBrightness;
        uint8_t rgb[20][3];  // [LED][RGB]
    };

    LampFrame frames[NUM_LAMPS];
    unsigned long effectStartTime;
    uint32_t frameNumber;

    // Helper: Set single LED color for a lamp
    void setLED(uint8_t lampIndex, uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b) {
        if (lampIndex >= NUM_LAMPS || ledIndex >= 20) return;
        frames[lampIndex].rgb[ledIndex][0] = r;
        frames[lampIndex].rgb[ledIndex][1] = g;
        frames[lampIndex].rgb[ledIndex][2] = b;
    }

    // Helper: Set all LEDs for a lamp
    void setAllLEDs(uint8_t lampIndex, uint8_t r, uint8_t g, uint8_t b) {
        if (lampIndex >= NUM_LAMPS) return;
        for (int i = 0; i < 20; i++) {
            frames[lampIndex].rgb[i][0] = r;
            frames[lampIndex].rgb[i][1] = g;
            frames[lampIndex].rgb[i][2] = b;
        }
    }

    // Helper: Clear all lamps
    void clearAll() {
        for (int lamp = 0; lamp < NUM_LAMPS; lamp++) {
            frames[lamp].cobBrightness = 0;
            setAllLEDs(lamp, 0, 0, 0);
        }
    }

    // Helper: Lerp (linear interpolation)
    uint8_t lerp(uint8_t a, uint8_t b, float t) {
        return a + (b - a) * t;
    }

public:
    SequencerEffects() : effectStartTime(0), frameNumber(0) {
        clearAll();
    }

    // =============================================================================
    // FADE EFFECT - Demo for smooth transitions
    // =============================================================================

    void updateFadeEffect(unsigned long now) {
        frameNumber++;
        uint32_t elapsed = now - effectStartTime;

        // Fade wave parameters
        float speed = 0.002f;  // Wave speed
        float waveLength = 3.0f;  // Wavelength in lamps

        // Calculate fade for each lamp
        for (uint8_t lamp = 0; lamp < NUM_LAMPS; lamp++) {
            // Sine wave traveling across lamps
            float phase = (lamp / waveLength) - (elapsed * speed);
            float intensity = (sin(phase * 2 * PI) + 1.0f) / 2.0f;  // 0.0 to 1.0

            // Color shifts with position
            float hue = (lamp * 30.0f + elapsed * 0.05f);
            hue = fmod(hue, 360.0f);

            // Convert HSV to RGB (simplified)
            uint8_t r, g, b;
            hsvToRgb(hue, 1.0f, intensity, r, g, b);

            // Set all LEDs in this lamp to same color
            setAllLEDs(lamp, r, g, b);

            // COB brightness matches LED intensity
            frames[lamp].cobBrightness = intensity * 255;
        }
    }

    // =============================================================================
    // BALL ANIMATION - Shows ball traveling across lamps
    // =============================================================================

    void updateBallEffect(float position, uint8_t player) {
        clearAll();

        // Position: 0.0 = left, 10.0 = right
        uint8_t lampIndex = (uint8_t)position;
        float subPosition = position - lampIndex;

        // Color based on player
        uint8_t r = (player == PLAYER_LEFT) ? 0 : 255;
        uint8_t g = (player == PLAYER_LEFT) ? 255 : 0;
        uint8_t b = 0;

        // Main lamp with ball
        if (lampIndex < NUM_LAMPS) {
            frames[lampIndex].cobBrightness = 255;

            // Show position on strip with gradient
            for (int i = 0; i < 20; i++) {
                float ledPos = i / 19.0f;  // 0.0 to 1.0 across strip
                float distance = fabs(ledPos - subPosition);
                uint8_t brightness = max(0, (int)(255 * (1.0f - distance * 3.0f)));

                setLED(lampIndex, i,
                       (r * brightness) >> 8,
                       (g * brightness) >> 8,
                       (b * brightness) >> 8);
            }
        }

        // Adjacent lamps dim
        if (lampIndex > 0) {
            frames[lampIndex - 1].cobBrightness = 64;
            setAllLEDs(lampIndex - 1, r >> 2, g >> 2, b >> 2);
        }
        if (lampIndex < NUM_LAMPS - 1) {
            frames[lampIndex + 1].cobBrightness = 64;
            setAllLEDs(lampIndex + 1, r >> 2, g >> 2, b >> 2);
        }
    }

    // =============================================================================
    // RAINBOW EFFECT - All lamps show rainbow
    // =============================================================================

    void updateRainbowEffect(unsigned long now) {
        frameNumber++;
        float hueShift = (now * 0.1f);

        for (uint8_t lamp = 0; lamp < NUM_LAMPS; lamp++) {
            float hue = fmod(hueShift + (lamp * 30.0f), 360.0f);

            uint8_t r, g, b;
            hsvToRgb(hue, 1.0f, 1.0f, r, g, b);

            setAllLEDs(lamp, r, g, b);
            frames[lamp].cobBrightness = 128;
        }
    }

    // =============================================================================
    // FLASH EFFECT - All lamps flash color
    // =============================================================================

    void updateFlashEffect(unsigned long now, uint8_t r, uint8_t g, uint8_t b) {
        uint32_t elapsed = now - effectStartTime;
        bool on = (elapsed / 100) % 2 == 0;  // Toggle every 100ms

        if (on) {
            for (uint8_t lamp = 0; lamp < NUM_LAMPS; lamp++) {
                setAllLEDs(lamp, r, g, b);
                frames[lamp].cobBrightness = 255;
            }
        } else {
            clearAll();
        }
    }

    // =============================================================================
    // SEND FRAMES TO LAMPS
    // =============================================================================

    void sendFramesToLamps() {
        for (uint8_t lamp = 0; lamp < NUM_LAMPS; lamp++) {
            LampFrameMessage msg;
            PongCommon::initMessageHeader(msg.header, MSG_LAMP_FRAME, 1);  // Sequencer ID = 1
            msg.lampIndex = lamp;
            msg.cobBrightness = frames[lamp].cobBrightness;
            msg.numLeds = 20;

            // Copy RGB data
            for (int i = 0; i < 20; i++) {
                msg.rgbData[i * 3]     = frames[lamp].rgb[i][0];  // R
                msg.rgbData[i * 3 + 1] = frames[lamp].rgb[i][1];  // G
                msg.rgbData[i * 3 + 2] = frames[lamp].rgb[i][2];  // B
            }

            // Send to specific lamp (unicast for now, could optimize with broadcast)
            // For broadcast, set lampIndex in message and use broadcast MAC
            PongCommon::broadcastMessage(&msg, sizeof(msg));
        }
    }

    // =============================================================================
    // EFFECT START
    // =============================================================================

    void startEffect() {
        effectStartTime = millis();
        frameNumber = 0;
    }

    // =============================================================================
    // HSV TO RGB CONVERSION
    // =============================================================================

    void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
        float c = v * s;
        float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r_prime, g_prime, b_prime;

        if (h < 60) {
            r_prime = c; g_prime = x; b_prime = 0;
        } else if (h < 120) {
            r_prime = x; g_prime = c; b_prime = 0;
        } else if (h < 180) {
            r_prime = 0; g_prime = c; b_prime = x;
        } else if (h < 240) {
            r_prime = 0; g_prime = x; b_prime = c;
        } else if (h < 300) {
            r_prime = x; g_prime = 0; b_prime = c;
        } else {
            r_prime = c; g_prime = 0; b_prime = x;
        }

        r = (r_prime + m) * 255;
        g = (g_prime + m) * 255;
        b = (b_prime + m) * 255;
    }

    // =============================================================================
    // GETTERS
    // =============================================================================

    uint32_t getFrameNumber() const { return frameNumber; }
};

#endif // SEQUENCER_EFFECTS_H
