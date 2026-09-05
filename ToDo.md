# TODO List: Tracker Sequencer Enhancements

## StepSequencer Improvements

### Event Queue System
- [X] Implement event queue in StepSequencer for precise timing
- [X] Add ratchet timing support via event queue (multiple triggers per step)
- [X] Implement microtiming offsets (shift individual steps forward/backward)
- [X] Add swing timing support (alternating step delays)
- [O] CV only step - won't do, can use the overlapping gates

### Glide/Portamento - now based on attack/decay
- [X] Implement glide/portamento function
- [X] Smoothly slide CV from previous note value to new note value
- [X] Add configurable glide time parameter (decay to glide down, attack to glide up)

---

## Main Menu System

### Menu Structure
- [X] Create main menu framework
- [ ] Implement app selection interface
- [X] Add global settings section
- [ ] Add app-specific settings section

---

## Global Settings

### Clock Configuration
- [X] Clock source selector (internal / external)
- [X] External clock sync values: 1ppqn, 2ppqn, 4ppqn, 24ppqn, 48ppqn
- [X] Internal BPM range: 30-300 BPM

### System Settings
- [X] Autoplay on start (enable/disable)
- [X] Synchronize start (enable/disable)
- [ ] Joystick calibration / reset
- [ ] Joystick speed adjustment
- [X] Display brightness control

---

## Tracker App Settings

### Song Management
- [ ] Song name editor
- [X] Song length selector (1-64 steps)
- [X] Save song to slot (0-8)
- [X] Load song from slot (0-8)

### Track Settings (Tracks 1-4)
- [X] Track settings menu (open with encoder A long click)
- [X] Quantizer enable/disable per track
- [X] CV range low (0 to cv_high-100) - when quantizer disabled
- [X] CV range high (cv_low+100 to 4095) - when quantizer disabled
- [X] Swing amount per track
- [X] CV attack/decay behavior: reset or keep previous value on new step

### Quantizer editor
- [ ] Note enable/disable grid (12 squares, toggle notes on/off)
- [ ] Automatic Quantizer with CV range and note number
- [ ] Select root note
- [ ] Edit/delete individual notes
- [ ] Edit steps individually
- [ ] Parametric generator for scales
- [ ] Note toggle grid (turn notes on/off)

### Data Management
- [ ] Load patterns or track settings from another song

---

## Quantizer Editor



---

## Pattern Settings (per pattern)

- [ ] Divider selection
- [ ] Pattern length selector (1-32 steps)
- [ ] Transpose/offset (shift all notes up/down)

---

## Performance Mode / Punch-in Effects

### Real-time Performance Controls
- [ ] Pause/activate track independently
- [ ] Repeat current step (hold)
- [ ] Increase/decrease probability in real-time
- [ ] Ratchet control (add extra triggers)
- [ ] Increase/decrease gate time in real-time