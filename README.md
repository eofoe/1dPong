# 1D Pong - Physical Reaction Game

A physical implementation of Pong as a two-player reaction game with 11 LED lamps, arcade buzzers, and ESP32 wireless communication.

## 🎮 Game Concept

Two players face off in a fast-paced reaction game:
- A light "ball" travels across 11 lamps from left to right
- The right player must press their buzzer at the perfect moment
- On good timing (≤600ms), the ball returns to the left
- The left player must then react
- The game continues back and forth, getting faster with each round
- First player to score 3 points wins!

### Timing System
- **GodShot** (≤100ms): Super-speed boost - game becomes extremely fast
- **Perfect** (≤600ms): Normal play continues
- **Miss** (>600ms): Opponent scores a point

### Game Dynamics
- Speed increases with each round
- Configurable speed profiles (Linear/Exponential)
- All settings adjustable via web interface
- Statistics tracking for performance analysis

## 🏗️ System Architecture

```
[Buzzer 1] ←──ESP-NOW──→ [Sequencer] ←──ESP-NOW──→ [11x Lamps]
[Buzzer 2] ←──ESP-NOW──┘
```

### Components

#### 1. Sequencer (1x ESP32)
- Master controller with game logic
- Web interface for configuration
- OTA update capability
- Manages scoring, timing, and speed
- Coordinates all devices via ESP-NOW

#### 2. Buzzer (2x ESP32)
- Arcade button input for each player
- Hardware debouncing
- Auto-detects player assignment (Left/Right)
- Sends button events to Sequencer

#### 3. Lamp (11x ESP32)
- COB high-power LED (PWM-controlled; Pin 12)
- Pixel strip with 20 RGBW LEDs (Pin 13)
- Receives commands via ESP-NOW
- Displays ball position, effects, and scores

## 🔧 Hardware Setup

### Per Lamp
- 1x ESP32 Development Board
- 1x COB LED (connected to GPIO 12 via MOSFET/transistor)
- 1x WS2812B RGBW LED Strip (20 LEDs, connected to GPIO 13)
- Power supply (5V, sufficient for LEDs)

### Lamp Position Detection
Configure lamp index (0-10) using GPIO pins:
- GPIO 16, 17, 18, 19: Binary encoding with pull-up resistors
- Pull LOW for binary 1, leave HIGH for binary 0
- Example: Lamp 5 = 0101 = GPIO16=HIGH, GPIO17=LOW, GPIO18=HIGH, GPIO19=LOW

### Buzzer Setup
- 1x ESP32 per player
- Arcade button connected to GPIO 0 (active LOW with pull-up)
- Built-in LED on GPIO 2 for feedback
- Player detection: GPIO 4 (LOW = Player 1 Left, HIGH = Player 2 Right)

### Power Requirements
- **Total:** ~18-20A at peak (11 lamps + controllers)
- **Per Lamp:** ~1.5-2A (ESP32 + LEDs at full brightness)
- Recommended: 5V/25A power supply with proper wiring

## 💻 Software Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- USB cable for initial programming
- WiFi network (for OTA updates after initial flash)

### Configuration

1. **WiFi Credentials** (edit `platformio.ini`):
```ini
build_flags =
    -D WIFI_SSID=\"YourWiFiSSID\"
    -D WIFI_PASSWORD=\"YourWiFiPassword\"
```

2. **OTA IP Addresses** (edit `platformio.ini` for each device):
```ini
[env:sequencer]
upload_port = 192.168.1.100  ; Sequencer IP

[env:buzzer]
upload_port = 192.168.1.101  ; Buzzer 1 IP
; upload_port = 192.168.1.102  ; Buzzer 2 IP

[env:lamp]
upload_port = 192.168.1.110  ; Lamp 0-10 (110-120)
```

### Building and Uploading

#### Initial Upload (USB)
```bash
# Sequencer
pio run -e sequencer -t upload

# Buzzer
pio run -e buzzer -t upload

# Lamp
pio run -e lamp -t upload
```

#### OTA Updates
```bash
# Update sequencer
pio run -e sequencer -t upload --upload-port 192.168.1.100

# Update specific lamp
pio run -e lamp -t upload --upload-port 192.168.1.115
```

### Serial Monitor
```bash
# Monitor sequencer
pio device monitor -e sequencer

# Monitor buzzer
pio device monitor -e buzzer

# Monitor lamp
pio device monitor -e lamp
```

## 🌐 Web Interface

After powering on the Sequencer, connect to:
- **Access Point:** `1dPong-Config` (Password: `pong1234`)
- **Web UI:** `http://192.168.4.1` (AP mode) or `http://<sequencer-ip>` (Station mode)

### Configuration Options

#### Game Settings
- **Timing Windows:** GodShot (50-200ms), Perfect (200-1000ms)
- **Timing Mode:** When timing window starts (fade begin/full bright/complete)
- **Speed Settings:** Start speed, increment, profile (linear/exponential)
- **Max Speed:** Speed cap to prevent impossibly fast gameplay
- **Winning Score:** Points required to win (1-10)
- **Animation Mode:** Discrete jump, smooth fade, or smooth with strip position

#### Visual Settings
- **Idle Effect:** Rainbow, Breathing, Knight Rider, Twinkle, Fire, Pulse
- **Idle Speed:** Animation speed (10-200ms)
- **Global Brightness:** 0-100%
- **Score Display Mode:** Edge lamps or minimal
- **Player Colors:** Customizable RGB colors for each player

#### Statistics
- Total games played
- Win count per player
- GodShot/Perfect/Miss counts
- Average reaction times (overall and per player)
- Best/worst reaction times

#### Network Status
- Connected buzzers (with RSSI)
- Active lamps (with RSSI)
- Real-time device monitoring

## 🎨 LED Effects

### Idle Effects (6 modes)
1. **Rainbow:** Cycling rainbow across all LEDs
2. **Breathing:** Pulsing blue breathing effect
3. **Knight Rider:** Red scanner effect
4. **Twinkle:** Random twinkling stars
5. **Fire:** Realistic fire simulation
6. **Pulse:** Purple pulsing wave

### Game Effects
- **GodShot:** Rapid rainbow flash (super impressive!)
- **Perfect:** Green confirmation flash
- **Miss:** Red warning flash
- **Point Win:** Color wave from winner's side
- **Point Lose:** Dim fade out
- **Game Win:** Celebration animation with sparkles
- **Game Lose:** Sad gray fade

### Ball Animation
- **Discrete:** Jump between lamps
- **Smooth Fade:** Fade transition between lamps
- **Smooth Strip:** Shows precise ball position on LED strip (default)

## 📡 Communication Protocol

### ESP-NOW Messages
All communication uses ESP-NOW on Channel 1 for low-latency (~5-20ms).

**Buzzer → Sequencer:**
- `MSG_BUTTON_PRESS`: Button press with microsecond timestamp

**Sequencer → Lamps:**
- `MSG_LAMP_SET_POSITION`: Ball position update
- `MSG_LAMP_EFFECT`: Trigger visual effect
- `MSG_LAMP_IDLE`: Set idle animation mode
- `MSG_LAMP_SCORE_DISPLAY`: Update score display
- `MSG_LAMP_CONFIG`: Configuration update

**Bidirectional:**
- `MSG_STATUS_REQUEST/RESPONSE`: Device health monitoring
- `MSG_PING/PONG`: Network testing
- `MSG_RESET`: Remote device reset

## 🔋 Persistent Settings

All settings are stored in NVS (Non-Volatile Storage) and persist across power cycles:
- Game configuration
- Visual settings
- Statistics
- Device IDs

Factory reset: Long-press buzzer button for 3 seconds.

## 🚀 Project Structure

```
1dPong/
├── platformio.ini              # PlatformIO configuration (3 environments)
├── lib/
│   └── PongCommon/            # Shared library
│       ├── Protocol.h          # ESP-NOW message definitions
│       ├── Config.h            # Configuration structures
│       ├── PongCommon.h        # Utility functions
│       └── LedEffects.h        # LED effects library
├── src/
│   ├── sequencer/
│   │   ├── main.cpp           # Sequencer firmware
│   │   └── webserver.h        # Web interface
│   ├── buzzer/
│   │   └── main.cpp           # Buzzer firmware
│   └── lamp/
│       └── main.cpp           # Lamp firmware
└── README.md
```

## 🎯 Default Game Settings

- **GodShot Window:** 100ms
- **Perfect Window:** 600ms
- **Start Speed:** 200ms per lamp
- **Speed Increment:** 20ms (linear) or 1.15x (exponential)
- **Max Speed:** 50ms per lamp
- **Winning Score:** 3 points
- **Timing Mode:** When fully bright
- **Animation Mode:** Smooth with strip position

## 🐛 Troubleshooting

### Lamps not responding
- Check ESP-NOW channel (should be 1)
- Verify sequencer is powered on
- Check RSSI in network status (weak signal?)
- Restart lamp (power cycle)

### Buzzer not registering
- Check button wiring (active LOW)
- Verify player assignment (GPIO 4)
- Monitor serial output for button events
- Check ESP-NOW connection to sequencer

### Web interface not accessible
- Verify WiFi credentials in platformio.ini
- Try Access Point mode: Connect to "1dPong-Config"
- Check sequencer serial output for IP address
- Ensure WiFi and ESP-NOW are on same channel

### OTA update fails
- Verify IP address in platformio.ini
- Check OTA password ("1dPongOTA")
- Ensure device is on network
- Try USB upload if OTA consistently fails

### Game too fast/slow
- Adjust speed settings in web interface
- Change speed profile (linear vs exponential)
- Modify max speed cap
- Adjust timing windows for difficulty

## 🔮 Future Enhancements

Possible additions:
- Sound effects via piezo speakers
- Battery-powered mode with level monitoring
- Mobile app for configuration
- Tournament mode with multiple rounds
- Replay system
- Cloud statistics sync
- Multi-game support (different game modes)

## 📝 License

This project is open source. Feel free to modify and improve!

## 🙏 Credits

Developed using:
- [PlatformIO](https://platformio.org/)
- [Arduino Framework](https://www.arduino.cc/)
- [FastLED](https://github.com/FastLED/FastLED)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduinoJson](https://arduinojson.org/)

---

**Have fun playing 1D Pong!** 🏓⚡
