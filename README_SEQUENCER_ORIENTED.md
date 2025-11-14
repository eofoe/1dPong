# 1D Pong - Sequencer-Oriented Architecture

**Branch:** `sequencer-oriented`

Alternative Implementierung, bei der alle visuellen Effekte zentral vom Sequencer berechnet und als Frame-Daten an die Lampen gesendet werden.

## 🔄 Architektur-Unterschied

### Autonomous Architecture (main branch)
```
Sequencer: "Zeige GodShot-Effekt!"
    ↓
Lampe: [empfängt Kommando] → [berechnet Effekt lokal] → [zeigt LEDs an]
```

### Sequencer-Oriented Architecture (dieser Branch)
```
Sequencer: [berechnet Frame 1] → sendet RGB-Daten → Lampe: [zeigt an]
Sequencer: [berechnet Frame 2] → sendet RGB-Daten → Lampe: [zeigt an]
Sequencer: [berechnet Frame 3] → sendet RGB-Daten → Lampe: [zeigt an]
...
```

## ✅ Vorteile

- **Perfekte Synchronisation** - Alle Lampen zeigen exakt dasselbe zur selben Zeit
- **Einfaches Debugging** - Alle Effekt-Logik an einem Ort
- **Dynamische Effekte** - Effekte ändern ohne Lamp-Firmware-Update
- **Kleinere Lamp-Firmware** - Nur ~5KB statt ~50KB
- **Zentrale Kontrolle** - Einfacher zu verstehen und zu warten

## ⚠️ Nachteile

- **Hohe Bandbreite** - ~70 Bytes × 11 Lampen × 30 FPS = ~23 KB/s
- **Netzwerk-Abhängigkeit** - Bei Verbindungsverlust: schwarze Lampen
- **Sequencer-Last** - Muss 330 Frames/Sekunde berechnen
- **Latenz** - ~5-20ms zusätzlich durch Übertragung

## 📦 Neue Dateien

```
src/
├── sequencer/
│   ├── sequencer_effects.h       # Zentrale Effekt-Berechnung
│   └── main_fade_demo.cpp        # Demo mit Fade-Effekt
└── lamp/
    └── main_sequencer_oriented.cpp  # "Dumme" Lamp-Firmware
```

## 🚀 Schnellstart - Fade Demo

### 1. Lamp-Firmware flashen
```bash
# Editiere platformio.ini und aktiviere sequencer-oriented Lamp
# (Oder kopiere main_sequencer_oriented.cpp → main.cpp)

pio run -e lamp -t upload
```

### 2. Sequencer Fade-Demo flashen
```bash
# Kopiere main_fade_demo.cpp als main.cpp oder
# erstelle neues Environment in platformio.ini

pio run -e sequencer_demo -t upload
```

### 3. Effekte testen

Serial Monitor öffnen (115200 baud):
```
========================================
   1D PONG - FADE DEMO
   Sequencer-Oriented Architecture
========================================

Controls:
  'f' - Fade wave effect
  'r' - Rainbow effect
  'b' - Ball animation

[FPS] 30 fps (Frame #124)
```

**Drücke Tasten:**
- `f` → Fade-Welle über alle Lampen
- `r` → Synchronisierter Rainbow
- `b` → Ball-Animation
- `+` / `-` → Ball schneller/langsamer

## 📊 Performance-Messung

### Bandwidth-Kalkulation

```
Pro Frame:
- Header: 10 Bytes
- LED Data: 60 Bytes (20 LEDs × RGB)
- COB + Meta: 4 Bytes
= 74 Bytes pro Lampe

Bei 30 FPS × 11 Lampen:
74 × 30 × 11 = 24.420 Bytes/s = ~24 KB/s
```

### ESP-NOW Limits
- Max Message Size: 250 Bytes ✅ (74 < 250)
- Max Throughput: ~1 MB/s ✅ (24 KB/s << 1 MB/s)
- Latency: 5-20ms ✅ (akzeptabel bei 30 FPS)

## 🎨 Fade-Effekt Erklärung

Der Fade-Effekt demonstriert perfekte Synchronisation:

```cpp
void updateFadeEffect(unsigned long now) {
    for (uint8_t lamp = 0; lamp < 11; lamp++) {
        // Sine-Wave über Lampen
        float phase = (lamp / 3.0f) - (now * 0.002f);
        float intensity = (sin(phase * 2 * PI) + 1.0f) / 2.0f;

        // Farbe rotiert mit Position
        float hue = lamp * 30.0f + now * 0.05f;

        // HSV → RGB Konvertierung
        hsvToRgb(hue, 1.0f, intensity, r, g, b);

        // Alle 11 Lampen setzen
        setAllLEDs(lamp, r, g, b);
    }

    // EINMAL senden → ALLE zeigen synchron!
    sendFramesToLamps();
}
```

**Resultat:** Smooth color wave fließt von links nach rechts, alle Lampen perfekt synchron!

## 🔧 Integration in Haupt-Spiel

Um diese Architektur im Haupt-Spiel zu nutzen:

### 1. Ersetze Effekt-Aufrufe

**Vorher (autonomous):**
```cpp
// Sequencer sendet nur Kommando
LampEffectMessage msg;
msg.effectType = EFFECT_GODSHOT;
broadcastMessage(&msg);
```

**Nachher (sequencer-oriented):**
```cpp
// Sequencer berechnet und sendet Frames
void loop() {
    if (currentEffect == EFFECT_GODSHOT) {
        effects.updateGodShotEffect(millis());
        effects.sendFramesToLamps();
    }
}
```

### 2. Füge Effekte zu sequencer_effects.h hinzu

```cpp
void updateGodShotEffect(unsigned long now) {
    // Berechne Rainbow-Flash für alle Lampen
    uint8_t hue = (now / 10) % 360;
    for (uint8_t lamp = 0; lamp < 11; lamp++) {
        hsvToRgb(hue, 1.0f, 1.0f, r, g, b);
        setAllLEDs(lamp, r, g, b);
        frames[lamp].cobBrightness = 255;
    }
}
```

### 3. Loop mit Frame-Rate

```cpp
void loop() {
    // 30 FPS Update
    if (millis() - lastFrame > 33) {
        lastFrame = millis();

        // Berechne aktuellen Effekt
        updateCurrentEffect();

        // Sende an alle Lampen
        effects.sendFramesToLamps();
    }
}
```

## 🧪 Vergleich mit Autonomous

| Feature | Autonomous | Sequencer-Oriented |
|---------|-----------|-------------------|
| Sync-Qualität | ⚠️ 5-20ms Offset | ✅ Perfekt |
| Bandbreite | ✅ Minimal | ⚠️ 24 KB/s |
| Lamp-Firmware | 50 KB | 5 KB |
| Debugging | Schwer | Einfach |
| Updates | 11× flashen | 1× flashen |
| Autonomie | ✅ Läuft weiter | ❌ Abhängig |

## 🎯 Empfehlung

**Nutze Sequencer-Oriented wenn:**
- ✅ Perfekte Synchronisation kritisch
- ✅ Komplexe Animationen über mehrere Lampen
- ✅ Häufige Effekt-Änderungen
- ✅ Stabiles Netzwerk garantiert

**Nutze Autonomous wenn:**
- ✅ Netzwerk-Bandbreite limitiert
- ✅ Autonomie bei Störungen wichtig
- ✅ Weniger CPU-Last auf Sequencer
- ✅ Simple, unabhängige Effekte

## 📝 Nächste Schritte

1. **Teste Fade-Demo** mit echten Lampen
2. **Messe FPS** und Bandbreite in deinem Setup
3. **Vergleiche** Sync-Qualität beider Architekturen
4. **Entscheide** welche Architektur besser passt
5. **Integriere** bevorzugte Lösung in Haupt-Game

## 🔍 Debugging-Tipps

### Sequencer Serial Monitor
```
[FPS] 30 fps (Frame #1234)  # Frame-Rate OK?
```

### Lamp Serial Monitor
```
[STATS] FPS: 30, Total: 5678, Missed: 12  # Frames ankommen?
[WARN] No frames from sequencer!          # Verbindung verloren?
```

### Netzwerk-Test
```bash
# Auf Sequencer
ping <lamp-ip>  # Latenz testen
```

---

**Viel Erfolg beim Testen der Sequencer-Oriented Architektur!** 🎨⚡
