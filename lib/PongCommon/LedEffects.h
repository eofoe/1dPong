#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "Protocol.h"
#include "Config.h"

// =============================================================================
// LED EFFECTS LIBRARY
// Provides all visual effects for lamps
// =============================================================================

class LedEffects {
private:
    CRGB* strip;
    uint16_t numLeds;
    uint8_t cobPin;
    uint8_t globalBrightness;

    // Animation state
    uint32_t lastUpdate;
    uint16_t updateInterval;
    uint16_t animationStep;

    // Helper functions
    uint8_t scale8(uint8_t value, uint8_t scale) {
        return ((uint16_t)value * scale) >> 8;
    }

    CRGB ColorFromPalette(const CRGBPalette16& palette, uint8_t index, uint8_t brightness = 255) {
        uint8_t paletteIndex = (index * 16) / 256;
        CRGB color = palette[paletteIndex];
        return CRGB(
            scale8(color.r, brightness),
            scale8(color.g, brightness),
            scale8(color.b, brightness)
        );
    }

public:
    LedEffects(CRGB* ledStrip, uint16_t num, uint8_t cob)
        : strip(ledStrip), numLeds(num), cobPin(cob), globalBrightness(255),
          lastUpdate(0), updateInterval(50), animationStep(0) {

        // Configure COB LED pin (PWM)
        pinMode(cobPin, OUTPUT);
        ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit resolution
        ledcAttachPin(cobPin, 0);
    }

    void setBrightness(uint8_t brightness) {
        globalBrightness = brightness;
        FastLED.setBrightness(brightness);
    }

    void setUpdateInterval(uint16_t interval) {
        updateInterval = interval;
    }

    // =============================================================================
    // COB LED CONTROL
    // =============================================================================

    void setCobBrightness(uint8_t brightness) {
        uint8_t scaled = scale8(brightness, globalBrightness);
        ledcWrite(0, scaled);
    }

    void fadeCoB(uint8_t target, uint16_t duration) {
        // Smooth fade (would need to be called repeatedly in practice)
        setCobBrightness(target);
    }

    // =============================================================================
    // UTILITY FUNCTIONS
    // =============================================================================

    void clear() {
        fill_solid(strip, numLeds, CRGB::Black);
        setCobBrightness(0);
    }

    void setAll(CRGB color, uint8_t cobLevel = 0) {
        fill_solid(strip, numLeds, color);
        setCobBrightness(cobLevel);
    }

    void setPixel(uint16_t index, CRGB color) {
        if (index < numLeds) {
            strip[index] = color;
        }
    }

    void show() {
        FastLED.show();
    }

    // =============================================================================
    // IDLE EFFECTS
    // =============================================================================

    void updateIdleEffect(IdleEffect effect) {
        uint32_t now = millis();
        if (now - lastUpdate < updateInterval) return;
        lastUpdate = now;

        switch (effect) {
            case IDLE_RAINBOW:
                effectRainbow();
                break;
            case IDLE_BREATHING:
                effectBreathing();
                break;
            case IDLE_KNIGHT_RIDER:
                effectKnightRider();
                break;
            case IDLE_TWINKLE:
                effectTwinkle();
                break;
            case IDLE_FIRE:
                effectFire();
                break;
            case IDLE_PULSE:
                effectPulse();
                break;
            default:
                effectRainbow();
        }

        animationStep++;
        show();
    }

    // Rainbow cycle
    void effectRainbow() {
        uint8_t hue = animationStep & 0xFF;
        for (uint16_t i = 0; i < numLeds; i++) {
            strip[i] = CHSV(hue + (i * 255 / numLeds), 255, 255);
        }
        setCobBrightness(128 + sin8(animationStep * 2) / 2);
    }

    // Breathing effect
    void effectBreathing() {
        uint8_t breath = sin8(animationStep * 3);
        CRGB color = CHSV(160, 255, breath);  // Blue breathing
        fill_solid(strip, numLeds, color);
        setCobBrightness(breath);
    }

    // Knight Rider scanner
    void effectKnightRider() {
        fadeToBlackBy(strip, numLeds, 50);

        uint16_t pos = beatsin16(30, 0, numLeds - 1, 0, animationStep * 100);
        strip[pos] = CRGB::Red;

        if (pos > 0) strip[pos - 1] = CRGB(64, 0, 0);
        if (pos < numLeds - 1) strip[pos + 1] = CRGB(64, 0, 0);

        setCobBrightness(128);
    }

    // Twinkling stars
    void effectTwinkle() {
        fadeToBlackBy(strip, numLeds, 30);

        if (random8() < 80) {
            uint16_t pos = random16(numLeds);
            strip[pos] = CHSV(random8(), 200, 255);
        }

        setCobBrightness(64 + random8(128));
    }

    // Fire effect
    void effectFire() {
        // Simple fire simulation
        static uint8_t heat[60];  // Heat map (max 60 LEDs)
        uint8_t maxHeat = min((uint16_t)numLeds, (uint16_t)60);

        // Cool down every cell a little
        for (uint16_t i = 0; i < maxHeat; i++) {
            heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / maxHeat) + 2));
        }

        // Heat from each cell drifts up
        for (uint16_t k = maxHeat - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }

        // Randomly ignite new sparks
        if (random8() < 120) {
            uint8_t y = random8(7);
            heat[y] = qadd8(heat[y], random8(160, 255));
        }

        // Map heat to LED colors
        for (uint16_t j = 0; j < numLeds && j < maxHeat; j++) {
            CRGB color = HeatColor(heat[j]);
            strip[j] = color;
        }

        setCobBrightness(128 + random8(64));
    }

    // Pulse wave
    void effectPulse() {
        uint8_t pulse = beatsin8(60, 0, 255, 0, animationStep);
        CRGB color = CHSV(200, 255, pulse);  // Purple pulse
        fill_solid(strip, numLeds, color);
        setCobBrightness(pulse);
    }

    // =============================================================================
    // GAME EFFECTS
    // =============================================================================

    void effectGodShot(Player player) {
        // Super fast rainbow flash
        for (int repeat = 0; repeat < 5; repeat++) {
            for (uint8_t hue = 0; hue < 255; hue += 15) {
                fill_solid(strip, numLeds, CHSV(hue, 255, 255));
                setCobBrightness(255);
                show();
                delay(10);
            }
        }
        clear();
        show();
    }

    void effectPerfect(Player player) {
        // Green flash
        CRGB color = CRGB::Green;
        for (int i = 0; i < 3; i++) {
            setAll(color, 255);
            show();
            delay(100);
            clear();
            show();
            delay(100);
        }
    }

    void effectMiss(Player player) {
        // Red warning flash
        for (int i = 0; i < 5; i++) {
            setAll(CRGB::Red, 255);
            show();
            delay(80);
            clear();
            show();
            delay(80);
        }
    }

    void effectPointWin(Player player, uint32_t playerColor) {
        // Color wave from winning side
        CRGB color = CRGB(
            (playerColor >> 16) & 0xFF,
            (playerColor >> 8) & 0xFF,
            playerColor & 0xFF
        );

        for (int wave = 0; wave < 2; wave++) {
            for (int i = 0; i < numLeds; i++) {
                clear();
                for (int j = 0; j <= i; j++) {
                    strip[j] = color;
                }
                setCobBrightness(map(i, 0, numLeds - 1, 0, 255));
                show();
                delay(30);
            }
        }
        clear();
        show();
    }

    void effectPointLose(Player player) {
        // Dim fade out
        for (int brightness = 255; brightness >= 0; brightness -= 5) {
            fill_solid(strip, numLeds, CRGB(brightness / 4, 0, 0));
            setCobBrightness(brightness);
            show();
            delay(10);
        }
        clear();
        show();
    }

    void effectGameWin(uint32_t playerColor) {
        // Celebration animation
        CRGB color = CRGB(
            (playerColor >> 16) & 0xFF,
            (playerColor >> 8) & 0xFF,
            playerColor & 0xFF
        );

        for (int repeat = 0; repeat < 10; repeat++) {
            // Flash
            setAll(color, 255);
            show();
            delay(100);
            clear();
            show();
            delay(100);

            // Sparkle
            for (int i = 0; i < 20; i++) {
                strip[random16(numLeds)] = color;
            }
            setCobBrightness(200);
            show();
            delay(100);
        }
        clear();
        show();
    }

    void effectGameLose(uint32_t playerColor) {
        // Sad fade
        CRGB color = CRGB(64, 64, 64);  // Dim gray
        for (int brightness = 255; brightness >= 0; brightness -= 2) {
            fill_solid(strip, numLeds, color);
            setCobBrightness(brightness / 3);
            show();
            delay(15);
        }
        clear();
        show();
    }

    // =============================================================================
    // BALL ANIMATION
    // =============================================================================

    void showBallPosition(uint8_t lampIndex, uint8_t position, Player direction, uint32_t ballColor) {
        clear();

        // Main COB shows ball at this lamp
        setCobBrightness(255);

        // Strip shows sub-position for smooth animation
        if (position > 0) {
            // Ball is between lamps - show gradient
            uint8_t intensity1 = 255 - position;
            uint8_t intensity2 = position;

            CRGB color = CRGB(
                (ballColor >> 16) & 0xFF,
                (ballColor >> 8) & 0xFF,
                ballColor & 0xFF
            );

            // Show position on strip (center LED bright, fade to edges)
            uint8_t centerLed = map(position, 0, 255, 0, numLeds - 1);

            for (int i = 0; i < numLeds; i++) {
                int distance = abs(i - centerLed);
                int brightCalc = 255 - (distance * 60);
                uint8_t brightness = (brightCalc < 0) ? 0 : (uint8_t)brightCalc;
                strip[i] = color;
                strip[i].nscale8(brightness);
            }
        } else {
            // Ball is at this lamp - show full brightness
            CRGB color = CRGB(
                (ballColor >> 16) & 0xFF,
                (ballColor >> 8) & 0xFF,
                ballColor & 0xFF
            );

            // Center pulse on strip
            uint8_t centerLed = numLeds / 2;
            for (int i = 0; i < numLeds; i++) {
                int distance = abs(i - centerLed);
                int brightCalc = 255 - (distance * 40);
                uint8_t brightness = (brightCalc < 0) ? 0 : (uint8_t)brightCalc;
                strip[i] = color;
                strip[i].nscale8(brightness);
            }
        }

        show();
    }

    // =============================================================================
    // SCORE DISPLAY
    // =============================================================================

    void showScore(uint8_t lampIndex, uint8_t leftScore, uint8_t rightScore,
                    uint32_t p1Color, uint32_t p2Color) {
        clear();

        // Mode: Edges (3 lamps per player)
        // Lamps 0-2: Player 1 (left)
        // Lamps 8-10: Player 2 (right)

        CRGB color1 = CRGB((p1Color >> 16) & 0xFF, (p1Color >> 8) & 0xFF, p1Color & 0xFF);
        CRGB color2 = CRGB((p2Color >> 16) & 0xFF, (p2Color >> 8) & 0xFF, p2Color & 0xFF);

        if (lampIndex <= 2) {
            // Left side lamps show player 1 score
            if (lampIndex < leftScore) {
                setAll(color1, 200);
            } else {
                setAll(CRGB(20, 20, 20), 30);  // Dim for unscored points
            }
        } else if (lampIndex >= NUM_LAMPS - 3) {
            // Right side lamps show player 2 score
            uint8_t rightLampIndex = lampIndex - (NUM_LAMPS - 3);
            if (rightLampIndex < rightScore) {
                setAll(color2, 200);
            } else {
                setAll(CRGB(20, 20, 20), 30);
            }
        } else {
            // Middle lamps stay neutral/off
            setAll(CRGB(10, 10, 10), 20);
        }

        show();
    }

    // =============================================================================
    // TRANSITION EFFECTS
    // =============================================================================

    void fadeIn(CRGB color, uint16_t duration) {
        for (int brightness = 0; brightness <= 255; brightness += 5) {
            fill_solid(strip, numLeds, color);
            FastLED.setBrightness(brightness);
            setCobBrightness(brightness);
            show();
            delay(duration / 51);  // 51 steps (0 to 255 by 5)
        }
    }

    void fadeOut(uint16_t duration) {
        for (int brightness = 255; brightness >= 0; brightness -= 5) {
            FastLED.setBrightness(brightness);
            setCobBrightness(brightness);
            show();
            delay(duration / 51);
        }
        clear();
        show();
    }
};

#endif // LED_EFFECTS_H
