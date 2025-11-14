# 1D Pong - Branch Structure

This repository contains two independent branches with different architectures.

## 🌿 Branches

### 1. `claude/incomplete-description-011CV65uQvXLqsvXvGpkVeQq` (Autonomous Architecture)

**Main implementation** - Full game with autonomous lamps.

**Files:**
- `src/sequencer/main.cpp` - Complete game logic with WebUI
- `src/sequencer/webserver.h` - Web configuration interface
- `src/lamp/main.cpp` - Autonomous lamp with local effect execution
- `src/buzzer/main.cpp` - Button input handling
- `lib/PongCommon/LedEffects.h` - LED effects library (used by lamps)

**Characteristics:**
- Lamps execute effects locally
- Low bandwidth (~2 KB/s)
- 5-20ms synchronization offset between lamps
- Autonomous operation (lamps continue if network fails)
- Full WebUI for configuration
- Statistics tracking
- OTA updates

**Use this branch for:**
- Production deployment
- Stable, reliable game
- Network bandwidth constraints
- Independent lamp operation

---

### 2. `claude/sequencer-oriented-011CV65uQvXLqsvXvGpkVeQq` (Sequencer-Oriented Architecture)

**Experimental** - Fade demo with centralized control.

**Files:**
- `src/sequencer/main.cpp` - Fade demo with effect engine (was main_fade_demo.cpp)
- `src/sequencer/sequencer_effects.h` - Central effect calculation
- `src/lamp/main.cpp` - "Dumb" display lamp (was main_sequencer_oriented.cpp)
- `src/buzzer/main.cpp` - Same as autonomous branch
- `README_SEQUENCER_ORIENTED.md` - Architecture documentation

**Characteristics:**
- Sequencer calculates all effects centrally
- Perfect synchronization (0ms offset)
- Higher bandwidth (~24 KB/s @ 30 FPS)
- Network-dependent (lamps go dark if connection lost)
- Interactive demo with serial controls
- FPS counter and statistics

**Use this branch for:**
- Testing perfect synchronization
- Experimenting with centralized control
- Learning about different architectures
- Demo presentations

---

## 🔄 Switching Between Branches

```bash
# View all branches
git branch -v

# Switch to autonomous (full game)
git checkout claude/incomplete-description-011CV65uQvXLqsvXvGpkVeQq

# Switch to sequencer-oriented (fade demo)
git checkout claude/sequencer-oriented-011CV65uQvXLqsvXvGpkVeQq
```

## 🔨 Building

Both branches are **completely independent** and can be built without conflicts.

### Autonomous Branch
```bash
git checkout claude/incomplete-description-011CV65uQvXLqsvXvGpkVeQq

# Build all components
pio run -e sequencer  # Full game + WebUI
pio run -e buzzer     # Button input
pio run -e lamp       # Autonomous lamp with effects
```

### Sequencer-Oriented Branch
```bash
git checkout claude/sequencer-oriented-011CV65uQvXLqsvXvGpkVeQq

# Build all components
pio run -e sequencer  # Fade demo
pio run -e buzzer     # Same as autonomous
pio run -e lamp       # Dumb display lamp
```

## 📊 Quick Comparison

| Aspect | Autonomous | Sequencer-Oriented |
|--------|-----------|-------------------|
| **Game Mode** | ✅ Full Pong game | ⚠️ Fade demo only |
| **WebUI** | ✅ Yes | ❌ No (future) |
| **Synchronization** | ⚠️ 5-20ms offset | ✅ Perfect (0ms) |
| **Bandwidth** | ✅ ~2 KB/s | ⚠️ ~24 KB/s |
| **Lamp Firmware** | 50 KB | 5 KB |
| **Network Dependency** | ✅ Autonomous | ❌ Dependent |
| **Maturity** | ✅ Production | ⚠️ Experimental |

## 🎯 Recommended Workflow

1. **Start with Autonomous** - Build and test the full game
2. **Experiment with Sequencer-Oriented** - Compare synchronization quality
3. **Decide** which architecture fits your needs better
4. **Develop** on chosen branch

## 📝 Notes

- Both branches share the same `lib/PongCommon/` code
- Branches diverge at commit `0ee0924`
- No merging needed - they are separate implementations
- Each branch compiles independently without conflicts

## 🐛 Troubleshooting

**Error: Multiple definition of `setup()`**
- You're on wrong branch or have modified files
- Clean build: `pio run --target clean`
- Verify branch: `git branch` (should show current branch)

**Missing files:**
- Ensure you're on correct branch
- Each branch has different main.cpp files
- Don't mix files between branches

---

**Questions? See individual branch READMEs:**
- Autonomous: `README.md`
- Sequencer-Oriented: `README_SEQUENCER_ORIENTED.md`
