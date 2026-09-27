# EUROTRACKER

Alternative firmware for the **SYNSO EUROLAB** module — a Eurorack step sequencer and tracker built on ESP32.

Six tracks of CV/gate sequencing with a tracker-style workflow, real-time quantization, scale selection, and persistent project storage.

---

## Features

**Sequencing**
- 6 tracks: 4 CV/gate, 2 gate-only
- 64 song steps, each holding a 32-step pattern
- Per-step parameters: CV, gate on/off, probability, gate length, attack, decay, ratchet, microtiming
- Per-step clock divider/multiplier: ×4, ×3, ×2, ×1., ×1, /1., /2, /3, /4
- Song mode and loop-step mode

**Quantization**
- Real-time quantization on the 4 CV tracks
- 18 built-in scales (Chromatic, Major, Minor, Pentatonic, Blues, modes, exotic scales)
- Per-track root note selection
- Per-track note editor for custom scales
- Adjustable CV range per track

**Clock**
- Internal BPM (30.0 – 300.0)
- External clock input: 1, 2, 4, 24, 48 PPQN
- Soft PLL for external clock tracking
- Clock output (pin reconfigures automatically when internal clock is selected)
- Sync start, autoplay

**Storage**
- SPIFFS project storage
- 8 save slots
- Auto-save every 15 seconds when idle

**UI**
- 128×64 OLED (SH1106)
- Joystick navigation
- Dual rotary encoders with push buttons
- Dedicated save button
- Parameter preview overlay with physical units (volts, milliseconds)

---

## Hardware Platform

This firmware targets the **SYNSO EUROLAB** Eurorack module.

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

> This is an alternative firmware, not the official SYNSO firmware. It is not affiliated with or endorsed by SYNSO.

---

## Build

This is a PlatformIO project.

```bash
# Install PlatformIO CLI or use the VS Code extension
pio run                # build
pio run --target upload --upload-port <PORT>
pio device monitor
```

Dependencies are declared in `platformio.ini`:
- `ESP32Encoder`
- `OneButton`

---

## Quick Start

1. Power on. The Song UI appears.
2. Move the joystick to select a track and step.
3. Short-press the joystick to enter the step editor for the selected track/step.
4. Press **save** to start playback. Press **save** again to toggle between song loop and single-step loop. Long-press **save** to stop.
5. In the step editor, the joystick moves the cursor, the joystick button toggles a step on/off, and the encoders edit parameters.

---

## Menu Reference

**Song UI**
- Long-press **joystick** → Main Menu
- Long-press **encoder A** → Track Menu

**Step Editor**
- Long-press **joystick** → Pattern Menu

**Main Menu** (playback must be stopped)
- Clock source, internal BPM, external PPQN
- Autoplay, sync start
- Brightness, joystick speed, song length
- New song, save/load song, save and exit, exit without saving

**Track Menu**
- Quantizer settings (enable, generate, scale, notes)
- CV low/high (or lowest/highest note when quantized)
- Swing, reset-on-step

**Pattern Menu**
- Divider, pattern length, transpose
- Save and exit, exit without saving

---

## Repository Layout

```
platformio.ini
src/
  main.cpp
  TrackerApp.*         application state machine
  SongSequencer.*      playback engine
  SongData.*           project data model
  SongUI.*             song grid view
  StepSequencer.*      per-track player
  StepSequencerUI.*    step editor
  StepPattern.*        pattern data
  Quantizer.*          note grid and scale mask
  Scales.*             scale definitions
  SyncTimer.*          clock input/output, PLL
  I2CGatekeeper.*      I²C bus arbitration
  DisplayManager.*     framebuffer and drawing
  DisplayGraphics.*    font and bitmaps
  OutputHandler.*      DAC and gate state
  UserInput.*          joystick, encoders, buttons
  GlobalSettings.*     persisted global configuration
  MainMenu.*           global settings menu
  TrackMenu.*          per-track settings
  PatternMenu.*        per-pattern settings
  QuantizerMainMenu.*  quantizer submenu
  GenerateMenu.*       chromatic grid generator
  ScaleMenu.*          scale selection
  NotesMenu.*          note editor
```

---

## Documentation

See `ProjectSummary.md` for the architecture, data flow, and implementation notes.

---

## License

MIT. See `LICENSE`.