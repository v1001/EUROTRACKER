#### 2026.06.06
[X] Event Queue
[X] AR - envelope
[X] Synchronized DAC on core 0
[X] Portamento

#### 2026.06.07
[X] Ratchet
[X] Microtiming
[X] Swing
[X] Menu!

#### 2026.06.08
[X] finalize menu
[X] fix bug with encoders
[X] check why no menu in pattern mode

#### 2026.06.13
[X] make a custom I2C driver to gatekeep all communication
[X] implement new DisplayManager, which is just a bufer
[X] implement new OutputHandler, where DAC handling is just buffer

#### 2026.06.14
[X] reset view to pattern preview in song mode
[X] move all UI logic into the tracker app
[X] average BPM
[X] move all graphics to separate DisplayGraphics.h
[X] use special characters for sharp notes and .5 for UI
[X] fix problem of step timing in sequencer for starting of playback
[x] implement autoplay and play on start logic
[X] add track menu - long press A
[X] new song

#### 2026.06.15
[X] first gate doesn't go off

#### 2026.06.20
[X] change the CV / quantizer behavior, store 16 bit in CV, quantizer always works directly on DAC value
[X] implement decimal edit and function for BPM
[X] implement transpose
[X] can't turn up the CV if not quantizer active
##### open bugs
[ ] click save to play, click joystick to open pattern, long press save to stop. hourglass remains.
[X] loading shorter song can still keep the step selection on further than last step in song UI
#### later
[ ] add song editing tools - long press B: copy step, insert step, insert empty step
[ ] add step menu - long press A
[ ] implement copy step logic (open step menu, copy step, setting new steps will use the copied one)
[ ] change ratchet / trigger behavior: 8 bit to store subtrigger: 1 bit is main trigger on/off, then total number of subtriggers 3 bits, then the euclidean number of subtriggers
[ ] test