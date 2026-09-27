# Tracker-Based Modular Sequencer — Project Summary

## Overview

A hardware sequencer for modular synthesizers, built on ESP32. Tracker-style 6-track step sequencer with CV/gate outputs, real-time quantization, scale selection, soft-PLL external clock tracking, and non-volatile project storage.

Alternative firmware for the **SYNSO EUROLAB** module. Not affiliated with or endorsed by SYNSO.

---

## Core Features

### Sequencing
- **6 tracks**: 4 melodic CV/gate, 2 gate-only
- **64 song steps** maximum, each containing a 32-step pattern
- **Per-step dividers**: ×4, ×3, ×2, ×1., ×1, /1., /2, /3, /4
- **Per-step parameters**: gate on/off, CV (0–4095, 12-bit DAC), probability (0–99), gate length (0–99), attack (0–99), decay (0–99), ratchet (1–4), microtiming (0–99)

### Quantization
- **Real-time quantization** on tracks 0–3
- **18 built-in scales**: Chromatic, Major, Natural Minor, Pentatonic Major, Blues, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Whole Tone, Diminished, Augmented, Hungarian Minor, Lydian Dominant, Super Locrian
- **Per-track root note** selection
- **Per-track note editor** for custom scales
- **Adjustable CV range** per track (clamps before quantizing)

### Playback Modes
- **Song mode** — linear playback through song steps
- **Loop-step mode** — repeats the current step (pattern editing)
- **Internal BPM** or **external clock** with PPQN support
- **Autoplay** and **sync start** options
- **Clock output** — GPIO 5 reconfigures as an output when internal clock is selected, toggling at the configured PPQN rate

### External Clock
- PPQN options: 1, 2, 4, 24, 48
- **Soft PLL** with fixed-point phase detection (sub-µs resolution)
- Continuous 0–191 counter, not reset by pulses
- Two-loop alignment: fast tempo tracking plus slow counter-wrap phase correction
- Returns to IDLE on clock loss

### UI & Navigation
- **128×64 OLED** (SH1106)
- **Joystick** — 4-way navigation
- **Dual rotary encoders** with push buttons
- **Dedicated save button** (single / double / long press)
- **Parameter preview overlay** showing current value plus physical units (volts, milliseconds)

### Storage
- **SPIFFS** project storage
- **8 save slots**
- **Auto-save** every 15 seconds when playback is stopped
- **Manual dirty flag** set on save-button exit

---

## Hardware

| Component | Interface | Address |
|-----------|-----------|---------|
| ESP32 | — | — |
| SH1106 128×64 OLED | I²C | 0x3C |
| MCP4728 quad 12-bit DAC | I²C | 0x60 |

### Pin Mapping

| Function | GPIO |
|----------|------|
| Encoder A (A, B) | 36, 39 |
| Encoder A button | 18 |
| Encoder B (A, B) | 35, 34 |
| Encoder B button | 19 |
| Joystick X, Y | 33, 32 |
| Joystick button | 23 |
| Save button | 16 |
| Clock in/out | 5 |
| Gate outputs | 12, 15, 2, 4, 13, 17 |
| I²C SDA, SCL | 21, 22 |

---

## Architecture

```
main.cpp
├── SyncTimer        external/standalone clock, soft PLL, clock output
├── UserInput        joystick, encoders, buttons
├── DisplayManager   framebuffer and drawing primitives
├── DisplayGraphics  font and bitmaps
├── I2CGatekeeper    arbitrates all I²C traffic (DAC + display)
├── GlobalSettings   persisted global configuration
├── TrackerApp       application state machine
│   ├── SongSequencer  playback engine
│   │   ├── SongUI            song grid view
│   │   ├── StepSequencer[6]  per-track players
│   │   │   └── StepSequencerUI  step editor
│   │   └── SongData          project data model
│   │       ├── StepPattern[6][64]
│   │       ├── Quantizer[4]
│   │       └── Serialization (SPIFFS save/load)
│   ├── OutputHandler  DAC and GPIO state
│   └── Menus
│       ├── MainMenu          global settings, save/load, new song
│       ├── TrackMenu         per-track settings
│       │   ├── QuantizerMainMenu  enable, generate, scale, notes
│       │   ├── GenerateMenu       chromatic grid generator
│       │   ├── ScaleMenu          scale selection + root
│       │   └── NotesMenu          per-note editor
│       └── PatternMenu       divider, length, transpose
```

### State Machine

`TrackerApp` has six UI states:

| State | Meaning |
|-------|---------|
| `STATE_SONG_UI` | Song grid view |
| `STATE_SEQUENCER_UI` | Step editor (8×4 grid) |
| `STATE_MAIN_MENU` | Global settings |
| `STATE_TRACK_MENU` | Per-track settings |
| `STATE_PATTERN_MENU` | Per-pattern settings |

Submenus (Quantizer, Generate, Scale, Notes) are managed internally by `TrackMenu` and do not appear in the top-level state enum.

State transitions are gated by a 500 ms cooldown to prevent input bounce from triggering multiple transitions.

### Data Flow

```
Core 1 (main loop)                    Core 0 (UI task)
─────────────────                     ────────────────
SyncTimer ISR
  └─ processClockTick()
       └─ SongSequencer
            ├─ advances song step
            └─ dispatches ticks
                 └─ StepSequencer
                      ├─ onStep()      (queue gate/CV events)
                      └─ processStepOutput()
                           └─ OutputHandler
                                ├─ setDACChannel()
                                └─ setDigitalOutput()

loop():
  ├─ processOutputs()
  ├─ I2CGatekeeper.setDAC() × 4
  ├─ OutputHandler.setAllDigitalOutputs()
  └─ I2CGatekeeper.displayWritePage()  (one block per iteration)

                                      uiTask():
                                        ├─ userInput.readInputs()
                                        ├─ display.clear()
                                        ├─ trackerApp.update()
                                        └─ display.update()
```

The UI task waits for the display to finish flushing before starting the next frame. Display blocks and DAC updates are interleaved by the main loop, so the UI cannot block real-time I/O.

---

## Code Standards

### Naming Conventions
- **Classes**: `PascalCase` (`DisplayManager`, `StepSequencer`)
- **Methods**: `camelCase` (`processClockTick`, `drawProgressBar`)
- **Private members**: leading underscore (`_currentStep`, `_songState`)
- **Constants**: `UPPER_SNAKE_CASE` (`MAX_SONG_LENGTH`, `DISPLAY_WIDTH`)
- **Enums**: `PascalCase` (`TextSize`, `SongState`)

### Architecture Patterns
- **Composition over inheritance** — managers own dependencies via references
- **Single responsibility** — UI logic separated from data logic (`SongUI` vs `SongData`)
- **RAII** — constructors initialize, destructors clean up (`DisplayManager`, `StepPattern`)
- **Observer pattern** — timer callbacks via function pointers (`TimerCallback`)

### Critical Implementation Details
- **ISR safety** — `volatile` flags, minimal work in interrupts
- **RTOS** — UI task pinned to Core 0, main loop on Core 1, save task on Core 0
- **I²C** — 1 MHz clock, all traffic routed through `I2CGatekeeper`
- **Encoder debouncing** — hardware filter via `ESP32Encoder::setFilter` plus software direction tracking
- **Cross-core data** — SongData is written by the UI task and read by the main loop. Structural edits (quantizer regeneration, pattern length change) are gated to playback-stopped to avoid reallocation races.

### Memory Management
- `StepPattern` uses dynamic arrays (`1–32` steps)
- `Quantizer` uses `std::vector<Note>`
- **Copy/Paste** — deep copy via copy constructor and assignment operator
- Large serialisation buffers eliminated; `SongData` writes directly to `File` handles

---

## Serialization Format

Binary file, little-endian, magic-guarded and versioned.

```
Header:
  magic          (4 bytes)   "SONG" = 0x534F4E47
  version        (1 byte)    encoded as major*10 + minor (e.g. 5 = v0.5)
  reserved       (3 bytes)

Song length      (4 bytes)

Dividers         _length × 6 tracks × 1 byte

Pattern data     for each track, each step, each pattern step:
  CV             (2 bytes)
  flags          (1 byte)    bit 0 = gate on
  probability    (1 byte)
  gate length    (1 byte)
  decay          (1 byte)
  attack         (1 byte)
  ratchet        (1 byte)
  microtiming    (1 byte)

Pattern lengths  _length × 6 tracks × 1 byte

Quantizers       for each of 4 tracks:
  enabled        (1 byte)
  numNotes       (1 byte)
  for each note:
    name         (4 bytes)
    DAC value    (2 bytes)
    inScale      (1 byte)

CV ranges        4 tracks × (min 2 bytes + max 2 bytes)

Reset flags      6 tracks × 1 byte

Swing amounts    6 tracks × 1 byte

Quantizer range  4 tracks × (startDAC 2 bytes + endDAC 2 bytes + numNotes 1 byte)

Scale/root       4 tracks × (scaleIndex 1 byte + rootIndex 1 byte)
```

Loader rejects files whose version does not match the current `FILE_VERSION`. No migration path is maintained for pre-release formats.

---

## Build

PlatformIO project.

```bash
pio run
pio run --target upload --upload-port <PORT>
pio device monitor
```

Dependencies (`platformio.ini`):
- `ESP32Encoder`
- `OneButton`

---

## License

MIT.