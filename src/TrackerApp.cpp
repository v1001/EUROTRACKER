#include "TrackerApp.h"
#include "MainMenu.h"
#include "PatternMenu.h"
#include "TrackMenu.h"

TrackerApp::TrackerApp(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler),
      _songSequencer(display, userInput, outputHandler), _currentUIState(STATE_SONG_UI),
      _sequencer(nullptr), _editingTrack(0), _editingStep(0), _lastStateTransitionTime(0),
      _mainMenu(nullptr), _patternMenu(nullptr), _quarterNoteTimeUs(0) {
}

TrackerApp::~TrackerApp() {
    delete static_cast<MainMenu*>(_mainMenu);
    delete static_cast<PatternMenu*>(_patternMenu);
    delete static_cast<TrackMenu*>(_trackMenu);
}

const char* TrackerApp::getDividerText(uint16_t divider) {
    for (int i = 0; i < SongData::getNumDividers(); i++) {
        if (SongData::getDividerValueByIndex(i) == divider) {
            return SongData::getDividerTextByIndex(i);
        }
    }
    return "x1";
}

void TrackerApp::enterSequencerUI(int track, int step) {
    _editingTrack = track;
    _editingStep = step;
    _songSequencer.scheduleSequencerMode(track, step);
}

void TrackerApp::exitSequencerUI() {
    _currentUIState = STATE_SONG_UI;
    _songSequencer.exitSequencerMode();
}

void TrackerApp::applyBrightness() {

}

void TrackerApp::applyJoystickSpeed() {
    // UserInput doesn't have setJoystickSpeed yet - will be implemented later
    // _userInput.setJoystickSpeed(GlobalSettings::joystickSpeed);
}

void TrackerApp::begin(bool reset) {
    // Load global settings first
    GlobalSettings::load();
    
    // Apply global settings to hardware
    applyBrightness();
    applyJoystickSpeed();
    
    // Initialize song sequencer (no clock config)
    _songSequencer.begin(reset);
    _songSequencer.setSyncStart(GlobalSettings::syncStart); 

    // If autoplay is enabled and this is not a reset, start playback
    if (!reset && GlobalSettings::autoplay) {
        _songSequencer.startPlayback();
    }
    
    // Initialize menu instances
    _mainMenu = new MainMenu(_display, _userInput, _outputHandler, this);
    _patternMenu = new PatternMenu(_display, _userInput, _songSequencer);
    _trackMenu = new TrackMenu(_display, _userInput, _songSequencer);
}

void TrackerApp::update(uint64_t quarterNoteTimeUs) {
    _quarterNoteTimeUs = quarterNoteTimeUs;
    // ===== RENDERING BASED ON STATE =====
    switch (_currentUIState) {
        case STATE_SONG_UI:
            _songSequencer.update();
            _songSequencer.drawSongUI();
            drawBPM();
            break;
            
        case STATE_SEQUENCER_UI:
            if (_sequencer) {
                _sequencer->update();
                drawPatternInfoText();
            }
            break;
            
        case STATE_MAIN_MENU:
            if (_mainMenu) {
                static_cast<MainMenu*>(_mainMenu)->update();
                static_cast<MainMenu*>(_mainMenu)->draw();
            }
            break;
            
        case STATE_PATTERN_MENU:
            if (_patternMenu) {
                static_cast<PatternMenu*>(_patternMenu)->update();
                static_cast<PatternMenu*>(_patternMenu)->draw();
            }
            break;

        case STATE_TRACK_MENU:
            if (_trackMenu) {
                static_cast<TrackMenu*>(_trackMenu)->update();
                static_cast<TrackMenu*>(_trackMenu)->draw();
            }
            break;
    }
    
    // ===== STATE TRANSITIONS =====
    unsigned long now = millis();
    if (now - _lastStateTransitionTime <= STATE_TRANSITION_COOLDOWN_MS) return;
    
    // Song UI -> Sequencer UI (short press joystick)
    if (_currentUIState == STATE_SONG_UI && _userInput.joystick_button.just_released) {
        _lastStateTransitionTime = now;
        int track = _songSequencer.getSelectedTrack();
        int step = _songSequencer.getSelectedStep();
        enterSequencerUI(track, step);
        return;
    }
    
    // Song UI -> Main Menu (long press joystick, only when not playing)
    if (_currentUIState == STATE_SONG_UI && 
        _userInput.joystick_button.press_duration > 500 && 
        _songSequencer.getSongState() == SongSequencer::STATE_STOP) {
        _lastStateTransitionTime = now;
        _currentUIState = STATE_MAIN_MENU;
        if (_mainMenu) static_cast<MainMenu*>(_mainMenu)->enter();
        return;
    }
    
    // Sequencer UI -> Pattern Menu (long press joystick, only when not playing)
    if (_currentUIState == STATE_SEQUENCER_UI && 
        _userInput.joystick_button.press_duration > 500) {
        _lastStateTransitionTime = now;
        _currentUIState = STATE_PATTERN_MENU;
        if (_patternMenu && _sequencer) {
            static_cast<PatternMenu*>(_patternMenu)->enter(_editingTrack, _editingStep, _sequencer);
        }
        return;
    }
    
    // Main Menu -> Song UI (save button: save settings and exit)
    if (_currentUIState == STATE_MAIN_MENU && _userInput.save_button.just_pressed) {
        _lastStateTransitionTime = now;
        if (_mainMenu) static_cast<MainMenu*>(_mainMenu)->saveAndExit();
        _currentUIState = STATE_SONG_UI;
        _songSequencer.resetEncoderTracking();
        return;
    }

    if (_currentUIState == STATE_MAIN_MENU && _mainMenu) {
        auto* menu = static_cast<MainMenu*>(_mainMenu);
        if (menu->shouldExit()) {
            _lastStateTransitionTime = now;
            menu->clearExitFlag();
            _currentUIState = STATE_SONG_UI;
            _songSequencer.resetEncoderTracking();
            return;
        }
    }
    
    // Pattern Menu -> Sequencer UI (save button: save settings and exit)
    if (_currentUIState == STATE_PATTERN_MENU && _userInput.save_button.just_pressed) {
        auto* pm = static_cast<PatternMenu*>(_patternMenu);
        if (pm && pm->isInSubMenu()) {
            // Submenu owns the save button while active. Do not exit
            // PatternMenu on press; StepEditMenu will handle the release.
            return;
        }
        if (_patternMenu) static_cast<PatternMenu*>(_patternMenu)->saveAndExit();
        _lastStateTransitionTime = now;
        _currentUIState = STATE_SEQUENCER_UI;
        _sequencer->onEnterUI();
        return;
    }

    // Pattern Menu exit via joystick selection (must be checked before save button to allow both)
    if (_currentUIState == STATE_PATTERN_MENU && _patternMenu){
        auto* menu = static_cast<PatternMenu*>(_patternMenu);
        if (menu->shouldExit()) {
            _lastStateTransitionTime = now;
            menu->clearExitFlag();
            _currentUIState = STATE_SEQUENCER_UI;
            _sequencer->onEnterUI();
            return;
        }
    }
    
    // Check if sequencer mode was scheduled (from Song UI)
    // Only transition if we are currently in Song UI
    if (_songSequencer.isInSequencerMode() && _currentUIState == STATE_SONG_UI) {
        _lastStateTransitionTime = now;
        _currentUIState = STATE_SEQUENCER_UI;
        _sequencer = _songSequencer.getSequencer(_editingTrack);
        return;
    }
    
    // Sequencer UI -> Song UI (save button short press)
    if (_currentUIState == STATE_SEQUENCER_UI && _userInput.save_button.just_pressed) {
        _lastStateTransitionTime = now;
        exitSequencerUI();
        return;
    }

    // Song UI -> Track Menu (long press encoder A, 3 seconds, playback stopped)
    if (_currentUIState == STATE_SONG_UI &&
        _userInput.encoder_a_button.press_duration > 500) {
        _currentUIState = STATE_TRACK_MENU;
        static_cast<TrackMenu*>(_trackMenu)->enter(_songSequencer.getSelectedTrack());
        return;
    }

    // Track Menu exit via joystick selection (Exit item)
    if (_currentUIState == STATE_TRACK_MENU && _trackMenu) {
        auto* menu = static_cast<TrackMenu*>(_trackMenu);
        if (menu->shouldExit()) {
            menu->clearExitFlag();
            _currentUIState = STATE_SONG_UI;
            _songSequencer.resetEncoderTracking();
            return;
        }
    }

    // Track Menu -> Song UI (save button)
    if (_currentUIState == STATE_TRACK_MENU && _userInput.save_button.just_pressed) {
        if (_trackMenu) static_cast<TrackMenu*>(_trackMenu)->saveAndExit();
        // The saveAndExit() sets _exitRequested, so the above check will also trigger.
        // To avoid double transition, we can also clear the flag here, but we can just return.
        _currentUIState = STATE_SONG_UI;
        _songSequencer.resetEncoderTracking();
        return;
    }
}

void TrackerApp::processClockTick(uint16_t tickCount) {
    _songSequencer.processClockTick(tickCount);
}

void TrackerApp::processOutputs() {
    _songSequencer.processOutputs();
}

void TrackerApp::drawPatternInfoText() {
    if (!_sequencer) return;
    
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    char infoText[32];
    
    uint8_t currentStep = _songSequencer.getSelectedStep();
    uint16_t divider = _sequencer->getClockDivision();
    const char* dividerText = getDividerText(divider);
    
    sprintf(infoText, "T%d-%02d%s", _editingTrack + 1, currentStep + 1, dividerText);
    _display.printAt(infoText, 0, 54, ALIGN_LEFT);
}

// ===== Settings Methods =====

void TrackerApp::setClockSource(GlobalSettings::ClockSource source) {
    GlobalSettings::clockSource = source;
    GlobalSettings::save();
    // Clock reconfiguration will be handled by main.cpp
}

void TrackerApp::setInternalBPM(uint16_t bpm) {
    if (bpm < 300) bpm = 300;
    if (bpm > 3000) bpm = 3000;
    GlobalSettings::internalBPM = bpm;
    GlobalSettings::save();
}

void TrackerApp::setExternalPPQN(uint8_t index) {
    GlobalSettings::externalPPQN = index;
    GlobalSettings::save();
}

void TrackerApp::setAutoplay(bool enabled) {
    GlobalSettings::autoplay = enabled;
    GlobalSettings::save();
}

void TrackerApp::setSyncStart(bool enabled) {
    GlobalSettings::syncStart = enabled;
    GlobalSettings::save();
    _songSequencer.setSyncStart(enabled);   // propagate to SongSequencer
}

void TrackerApp::setJoystickSpeed(uint8_t speed) {
    GlobalSettings::joystickSpeed = speed;
    GlobalSettings::save();
    applyJoystickSpeed();
}

void TrackerApp::setBrightness(uint8_t value) {
    GlobalSettings::brightness = value;
    GlobalSettings::save();
    applyBrightness();
}

void TrackerApp::saveCurrentProject() {
    _songSequencer.saveCurrentProject();
}

void TrackerApp::loadCurrentProject() {
    _songSequencer.loadCurrentProject();
}

void TrackerApp::saveCurrentProjectToSlot(int slot) {
    char filename[32];
    sprintf(filename, "/song%d.song", slot);
    _songSequencer.saveCurrentProjectToFile(filename);
}

void TrackerApp::loadCurrentProjectFromSlot(int slot) {
    char filename[32];
    sprintf(filename, "/song%d.song", slot);
    _songSequencer.loadCurrentProjectFromFile(filename);
}

bool TrackerApp::songExists(const char* filename){
    return _songSequencer.fileExists(filename);
}

uint32_t TrackerApp::getSongLength(){
    return _songSequencer.getSongData().getLength();
}

void TrackerApp::setSongLength(uint32_t length) {
    _songSequencer.getSongData().setLength(length);
}

void TrackerApp::drawBPM() {
    float bpm = 0.0f;
    if (_quarterNoteTimeUs != 0){
        bpm  = 60000000.0f / (float)_quarterNoteTimeUs;
    }
    _display.drawQuarterNote(2, 58);
    char bpmText[8];
    sprintf(bpmText, "%.1f", bpm);
    _display.printAt(bpmText, 12, 54, ALIGN_LEFT);
}

void TrackerApp::newCurrentProject() {
    _songSequencer.newCurrentProject();
}