# 1D Pong - Deployment Guide

Complete step-by-step guide for building and deploying the 1D Pong system.

## 📦 Hardware Requirements

### Bill of Materials

#### For Sequencer (1x)
- 1x ESP32 Development Board
- 1x Power supply (USB or 5V)

#### For Each Buzzer (2x total)
- 1x ESP32 Development Board
- 1x Arcade Button (normally open)
- 1x 10kΩ pull-up resistor (if not using internal pull-up)
- 1x 100nF capacitor (for hardware debouncing)
- Jumper wires
- Enclosure (optional)

#### For Each Lamp (11x total)
- 1x ESP32 Development Board
- 1x COB LED (e.g., 10W white COB)
- 1x MOSFET (e.g., IRLZ44N) for COB control
- 1x 10kΩ resistor (MOSFET gate pull-down)
- 1x WS2812B RGBW LED Strip (20 LEDs, ~1.2m at 60 LEDs/m)
- 1x 1000µF capacitor (for LED strip power smoothing)
- 1x 470Ω resistor (LED strip data line protection)
- 4x DIP switches or jumpers (for lamp index configuration)
- Power wiring
- Heatsink for COB LED
- Enclosure/mounting

#### Power Supply
- 1x 5V/25A (125W) switching power supply
- Or multiple smaller supplies (e.g., 3x 5V/10A)
- Power distribution blocks/bus bars
- Thick wire (14-16 AWG) for main power runs
- Thinner wire (22-24 AWG) for device connections

#### Miscellaneous
- USB cables for initial programming
- Ethernet/network switch (if using wired network for programming)
- Heat shrink tubing
- Solder and soldering iron
- Multimeter

## 🔌 Wiring Diagrams

### Buzzer Wiring
```
ESP32 GPIO0 ────┬──── Arcade Button ──── GND
                │
              [10kΩ to 3.3V]
                │
              [100nF to GND]

ESP32 GPIO4 ────── Jumper ──── GND (for Player 1)
                            or floating (for Player 2)
```

### Lamp Wiring - COB LED
```
ESP32 GPIO12 ────[1kΩ]──── MOSFET Gate
                              │
                         [10kΩ to GND]

5V ────── COB LED (+)
          COB LED (-) ────── MOSFET Drain
                             MOSFET Source ──── GND
```

### Lamp Wiring - LED Strip
```
5V ──┬─── [1000µF] ─── GND
     │
     └─── LED Strip VCC

ESP32 GPIO13 ──── [470Ω] ──── LED Strip DATA

GND ──── LED Strip GND
```

### Lamp Index Configuration
```
DIP Switch    Binary    Lamp Index
  0 0 0 0   =  0000   =     0
  0 0 0 1   =  0001   =     1
  0 0 1 0   =  0010   =     2
  0 0 1 1   =  0011   =     3
  0 1 0 0   =  0100   =     4
  0 1 0 1   =  0101   =     5
  0 1 1 0   =  0110   =     6
  0 1 1 1   =  0111   =     7
  1 0 0 0   =  1000   =     8
  1 0 0 1   =  1001   =     9
  1 0 1 0   =  1010   =    10

GPIO Mapping:
  GPIO 16 = Bit 0 (LSB)
  GPIO 17 = Bit 1
  GPIO 18 = Bit 2
  GPIO 19 = Bit 3 (MSB)

DIP ON (closed) = Pull to GND = 1
DIP OFF (open) = Pull-up to 3.3V = 0
```

## 🛠️ Assembly Steps

### 1. Prepare ESP32 Boards (14x total)
1. Flash with appropriate firmware (see Software Setup)
2. Label each board (Sequencer, Buzzer-L, Buzzer-R, Lamp-0 through Lamp-10)
3. Test basic functionality (LED blink test)

### 2. Build Buzzer Units (2x)
1. Solder button to GPIO0 and GND
2. Add 100nF capacitor across button for debouncing
3. Install jumper on GPIO4:
   - **Buzzer Left:** GPIO4 connected to GND
   - **Buzzer Right:** GPIO4 left floating
4. Test: Press button, check serial monitor for event
5. Mount in enclosure with arcade button accessible

### 3. Build Lamp Units (11x)
1. Configure DIP switches for lamp index (0-10)
2. Build COB LED driver circuit:
   - Solder MOSFET circuit on perfboard
   - Connect GPIO12 to gate via 1kΩ resistor
   - Add 10kΩ pull-down on gate
   - Wire COB LED to drain, source to GND
   - Attach heatsink to COB LED
3. Prepare LED strip:
   - Cut to 20 LEDs if needed
   - Solder power wires (thick gauge)
   - Add 1000µF capacitor at strip power input
   - Solder 470Ω resistor on data line
4. Connect LED strip to GPIO13
5. Test each lamp individually before final assembly
6. Mount in enclosure with LEDs visible

### 4. Power Distribution
1. Create main power bus from 5V supply
2. Run thick wire (14-16 AWG) along lamp array
3. Tap connections to each lamp (22-24 AWG)
4. Add inline fuses for safety (2A per lamp)
5. Measure voltage at furthest lamp (should be >4.8V)
6. If voltage drop is excessive, add power injection points

### 5. Testing Individual Components

#### Test Sequencer
```bash
pio run -e sequencer -t upload -t monitor
```
Expected output:
```
[OK] ESP-NOW initialized
[WiFi] AP started: 1dPong-Config
[OK] Sequencer ready!
```

#### Test Buzzer
```bash
pio run -e buzzer -t upload -t monitor
```
Press button, expect:
```
[BUTTON] Pressed
[TX] Button press sent
```

#### Test Lamp
```bash
pio run -e lamp -t upload -t monitor
```
Expected output:
```
Lamp Index: X
[LED] FastLED initialized
[OK] Lamp ready!
```
LEDs should show white flash on boot, then idle animation.

## 🚀 System Deployment

### Step 1: Configure WiFi
Edit `platformio.ini`:
```ini
-D WIFI_SSID=\"YourNetworkName\"
-D WIFI_PASSWORD=\"YourPassword\"
```

### Step 2: Initial USB Flash (All Devices)
```bash
# Flash sequencer
pio run -e sequencer -t upload

# Flash buzzers (one at a time)
pio run -e buzzer -t upload

# Flash all lamps (one at a time)
pio run -e lamp -t upload
```

### Step 3: Record IP Addresses
After initial boot, check serial monitor for each device's IP address:
```
[WiFi] Connected! IP: 192.168.1.XXX
```

Update `platformio.ini` with actual IPs:
```ini
[env:sequencer]
upload_port = 192.168.1.100  ; Your sequencer IP

[env:buzzer]
upload_port = 192.168.1.101  ; Buzzer 1 IP

[env:lamp]
upload_port = 192.168.1.110  ; Lamp 0 IP
# ... update for each lamp
```

### Step 4: Verify ESP-NOW Communication
1. Power on sequencer first
2. Power on lamps - check for idle animation
3. Power on buzzers
4. Access web interface: `http://<sequencer-ip>`
5. Check "Network Status" tab - all devices should show as online

### Step 5: Initial Configuration
Via web interface:
1. Set appropriate timing windows for your setup
2. Choose idle effect
3. Adjust brightness
4. Test different speed profiles
5. Save all settings

### Step 6: Game Testing
1. Press any buzzer to start game
2. Verify ball animation works
3. Test timing detection (GodShot, Perfect, Miss)
4. Verify score display
5. Play full game to 3 points
6. Check statistics tracking

## 🔧 Calibration

### Timing Calibration
1. Start with default settings
2. Have experienced player test reaction times
3. Adjust GodShot/Perfect windows based on actual performance
4. Typical adjustments:
   - Too easy: Reduce windows (GodShot: 75ms, Perfect: 500ms)
   - Too hard: Increase windows (GodShot: 150ms, Perfect: 800ms)

### Brightness Calibration
1. Set all lamps to same brightness in web UI
2. Visually compare brightness across all lamps
3. If uneven, adjust individual lamp brightness in firmware or via web API
4. Optimal: 60-80% for indoor use, 100% for bright environments

### Speed Calibration
1. Start with slow speed (300ms per lamp)
2. Play several games, adjusting increment
3. Find comfortable starting speed and increment
4. GodShot should make game noticeably faster
5. Max speed should be challenging but not impossible

## 📊 Monitoring & Maintenance

### Daily Checks
- Verify all devices show as online in network status
- Check for unusual RSSI values (should be >-70 dBm)
- Monitor free heap (should be >50 KB)

### Weekly Maintenance
- Backup statistics (screenshot or export if implemented)
- Check for firmware updates
- Verify power supply voltage (should be stable at 5V±0.2V)
- Inspect LED strips for dead pixels

### Monthly Maintenance
- Clean COB LED heatsinks
- Check all solder connections
- Verify DIP switch settings (lamp indices)
- Update firmware if new features available

## 🐛 Common Issues & Solutions

### Issue: Lamps not synchronized
**Cause:** ESP-NOW channel mismatch
**Solution:** Verify all devices on Channel 1, restart all devices

### Issue: Random button presses
**Cause:** Electrical noise or insufficient debouncing
**Solution:**
- Add/increase debounce capacitor (100nF → 220nF)
- Check for loose connections
- Increase `DEBOUNCE_TIME` in buzzer firmware

### Issue: LED strip flickering
**Cause:** Insufficient power or poor connections
**Solution:**
- Check power supply voltage under load
- Verify all GND connections
- Add power injection if voltage drops below 4.8V
- Increase capacitor size (1000µF → 2200µF)

### Issue: COB LED dim or off
**Cause:** MOSFET not switching or damaged COB
**Solution:**
- Measure gate voltage (should switch 0V → 3.3V)
- Check MOSFET is correct type (logic-level)
- Test COB LED directly with power supply
- Verify PWM signal on GPIO12

### Issue: Web UI not loading
**Cause:** WiFi connection failed or wrong IP
**Solution:**
- Connect to AP mode: "1dPong-Config"
- Check serial monitor for actual IP
- Verify WiFi credentials in platformio.ini
- Restart sequencer

### Issue: High latency (slow response)
**Cause:** Network congestion or weak signal
**Solution:**
- Minimize distance between devices
- Remove WiFi interference sources
- Use dedicated WiFi channel for ESP-NOW
- Check RSSI values (should be better than -70 dBm)

## 🔐 Security Notes

- Default OTA password: `1dPongOTA` - **CHANGE IN PRODUCTION**
- Default AP password: `pong1234` - **CHANGE IN PRODUCTION**
- Web interface has no authentication - **ADD IF PUBLIC**
- ESP-NOW uses broadcast - **ENCRYPTION OPTIONAL**

To change passwords, edit:
```cpp
// In main.cpp files
ArduinoOTA.setPassword("YourNewPassword");
WiFi.softAP("1dPong-Config", "YourNewPassword");
```

## 📈 Performance Targets

- **Button to Sequencer latency:** <20ms
- **Sequencer to Lamp latency:** <30ms
- **Total system latency:** <50ms
- **Button debounce time:** 50ms
- **ESP-NOW success rate:** >99%
- **Frame rate (LEDs):** >30 FPS
- **Power consumption:** 15-20A peak, 8-10A average

## ✅ Acceptance Testing

Before declaring system complete:

- [ ] All 13 devices power on successfully
- [ ] All devices connect via ESP-NOW
- [ ] Web interface accessible and functional
- [ ] Both buzzers register button presses
- [ ] Ball animation flows smoothly across all 11 lamps
- [ ] All 6 idle effects work on all lamps
- [ ] GodShot, Perfect, and Miss detection accurate
- [ ] Score display shows correctly
- [ ] Statistics track and persist across reboots
- [ ] Complete game playable from start to finish
- [ ] OTA updates work for all device types
- [ ] Settings persist after power cycle
- [ ] No obvious bugs in 10+ consecutive games

## 🎓 Training Users

### Quick Start Guide for Players
1. **Starting a game:** Press any buzzer
2. **Timing your press:**
   - Watch the ball travel to your side
   - Press when COB LED is fully bright
   - Green flash = good, Red flash = miss
3. **Winning:** First to 3 points wins
4. **GodShot:** Super fast press (<100ms) makes game crazy fast!
5. **Reset stats:** Hold buzzer for 3 seconds

### Administrator Guide
- **Access config:** Connect to WiFi, open web interface
- **Change difficulty:** Adjust timing windows and speed
- **Change visuals:** Select idle effect and colors
- **View stats:** Statistics tab shows performance data
- **Update firmware:** Use PlatformIO OTA commands
- **Factory reset:** Web UI or long-press buzzer

---

**System deployment complete! Enjoy your 1D Pong game!** 🎮⚡
