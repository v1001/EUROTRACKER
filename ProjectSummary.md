# Tracker-Based Modular Sequencer - Project Summary

## Overview
A **hardware sequencer for modular synthesizers** built on ESP32, featuring a tracker-style 6-track step sequencer with CV/gate outputs, real-time quantization, and non-volatile project storage.

---

## Core Features

### Sequencing
- **6 tracks** (4 melodic CV/gate, 2 pure gate triggers)
- **64 song steps** max, each containing a 32-step pattern
- **Per-step dividers**: x4, x3, x2, x1., x1, /1., /2, /3, /4 (9 options)
- **Real-time quantization** on tracks 0-3 (chromatic scales from C4-C9)
- **Per-step parameters**: Gate ON/OFF, CV (0-99), Probability (0-99), Gate Length (0-99), Decay (0-99), Ratchet (1-4)

### Playback Modes
- **Song Mode** – linear playback through song steps
- **Loop Step Mode** – repeats current step (pattern editing mode)
- **Clock sync** – external clock input with PPQN support or standalone BPM

### UI & Navigation
- **128×64 OLED** (SH1106) with custom UI
- **Joystick** – 4-way navigation
- **Dual rotary encoders** – parameter editing with detent feel
- **Push buttons** on encoders + dedicated save button (single/double/long press)

### Hardware Integration
- **MCP4728** – 4-channel 12-bit DAC for CV outputs (0-3.3V)
- **6 digital outputs** – gate/trigger signals
- **SPIFFS** – auto-save project every 15 seconds (idle only)

---

## Architecture

```
main.cpp
├── SyncTimer (external/standalone clock)
├── UserInput (read all inputs)
├── DisplayManager (handles graphics)
├── TrackerApp (state machine)
│   ├── SongSequencer (playback engine)
│   │   ├── SongUI (song grid view)
│   │   ├── StepSequencer[6] (per-track pattern players)
│   │   └── SongData (project data model)
│   │       ├── StepPattern[6][64] (patterns)
│   │       ├── Quantizer[4] (per-track quantization)
│   │       └── Serialization (SPIFFS save/load)
│   └── OutputHandler (DAC + GPIO)
```

### State Machine
- **STATE_SONG_UI** – grid editor for song structure
- **STATE_SEQUENCER_UI** – detailed step editing (8×4 grid)

### Data Flow
1. **SyncTimer** triggers timer interrupts → `processClockTick()`
2. **SongSequencer** advances steps, updates tick counters
3. **StepSequencer** evaluates current step (probability, ratchet)
4. **OutputHandler** updates DACs and gate pins
5. **UserInput** polls controls (non-blocking, 50ms task)

---

## Code Standards

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `DisplayManager`, `StepSequencer`)
- **Methods**: `camelCase` (e.g., `processClockTick`, `drawProgressBar`)
- **Private members**: `_leadingUnderscore` (e.g., `_currentStep`, `_songState`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_SONG_LENGTH`, `DISPLAY_I2C_ADDRESS`)
- **Enums**: `PascalCase` (e.g., `TextSize`, `SongState`)

### Architecture Patterns
- **Composition over inheritance** – managers own dependencies via references
- **Single responsibility** – UI logic separated from business logic (`SongUI` vs `SongData`)
- **RAII** – constructors initialize, destructors clean up (`DisplayManager`, `StepPattern`)
- **Observer pattern** – timer callbacks via function pointers (`TimerCallback`)

### Critical Implementation Details
- **ISR safety** – `volatile` flags, minimal work in interrupts
- **RTOS** – FreeRTOS tasks (UI on Core 0, save task on Core 0, main loop on Core 1)
- **I2C** – 800kHz clock for faster display updates
- **Encoder debouncing** – hardware + software filter (`ESP32Encoder::setFilter`)

### Memory Management
- `StepPattern` uses **dynamic arrays** (flexible step length: 24 or 32 steps)
- `Quantizer` uses `std::vector`
- **Copy/Paste** – deep copy via copy constructor/assignment operator

### Serialization Format
Projects saved as binary with versioning implied via field order:
- Song length (4 bytes)
- Divider indices (track×step)
- Pattern data (packed CV+gate, prob, gateLen, decay)
- Pattern lengths
- Quantizer data (enabled flag, notes, DAC values)
- CV ranges (min/max per melodic track)

---

## Hardware Pin Mapping

| Component | Pins |
|-----------|------|
| Encoder A | 36 (A), 39 (B) |
| Encoder A Button | 18 |
| Encoder B | 35 (A), 34 (B) |
| Encoder B Button | 19 |
| Joystick X/Y | 33, 32 |
| Joystick Button | 23 |
| Save Button | 16 |
| Clock Input | 5 |
| Digital Outs | 12, 15, 2, 4, 13, 17 |
| I2C (DAC + Display) | SDA=21, SCL=22 |