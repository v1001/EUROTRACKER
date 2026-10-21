#include "SongSequencer.h"

const char* SongSequencer::PROJECT_FILENAME = "/current_project.song";

SongSequencer::SongSequencer(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler),
      _songUI(display, userInput, _songData), _sequencerModeActive(false),
      _songState(STATE_STOP), _currentPlayStep(0), _currentStepTickCounter(0), _stepTicksRemaining(0),
      _pendingStart(false), _pendingSequencer(false), _nextPlayStep(0), _openSequencerTrack(0),
      _gateQueues{}, _cvQueues{}, _lastDACValues{0, 0, 0, 0},
      _lastJoyMoveTime(0), _perfLastEncAPos(0), _perfLastEncBPos(0), _perfPopupTime(0) {
    
    for (int i = 0; i < NUM_TRACKS; i++) {
        _sequencers[i] = nullptr;
    }
    _perfPopupText[0] = '\0';
}

SongSequencer::~SongSequencer() {
    for (int i = 0; i < NUM_TRACKS; i++) delete _sequencers[i];
}

void SongSequencer::begin(bool reset) {
    _songData.init();
    initSequencers();
    
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    _pendingStart = false;

    if (reset) {
        deleteCurrentProject();
        saveCurrentProject();
    } else {
        loadCurrentProject();
    }
}

void SongSequencer::advanceToNextStep() {
    portENTER_CRITICAL(&_songMutex);
    
    int nextStep = _currentPlayStep;
    if (_songState == STATE_PLAY_SONG) {
        SongData::SongStep* current = _songData.getStep(_currentPlayStep);
        
        if (current != nullptr && current->loopTarget != nullptr && _stepLoopCounters[_currentPlayStep] > 0) {
            _stepLoopCounters[_currentPlayStep]--;
            int targetIdx = _songData.getStepIndex(current->loopTarget);
            
            if (targetIdx >= 0) {
                nextStep = targetIdx;
                for (int i = nextStep; i < _currentPlayStep; i++) {
                    _stepLoopCounters[i] = _songData.getStep(i)->loopCount;
                }
            } else {
                nextStep = nextStep + 1;
            }
        } else {
            nextStep = nextStep + 1;
        }

        if (nextStep >= _songData.getLength()) {
            nextStep = 0;
            for (int i = 0; i < _songData.getLength(); i++) {
                _stepLoopCounters[i] = _songData.getStep(i)->loopCount;
            }
        }
    }
    
    if(nextStep != _currentPlayStep){
        updateSequencersFromStep(nextStep, true);
    }else{
        updateSequencersFromStep(nextStep, false);
    }
    
    _currentPlayStep = nextStep;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    
    portEXIT_CRITICAL(&_songMutex);
}

bool SongSequencer::insertStep(int selectedStep) {
    portENTER_CRITICAL(&_songMutex);
    bool success = _songData.insertStep(selectedStep);
    if (success && _songState != STATE_STOP && selectedStep < _currentPlayStep) {
        _currentPlayStep++;
    }
    portEXIT_CRITICAL(&_songMutex);
    if (success) _songUI.setSaveFlag();
    return success;
}

bool SongSequencer::deleteStep(int selectedStep) {
    if (_songState != STATE_STOP && selectedStep == _currentPlayStep) return false; 
    portENTER_CRITICAL(&_songMutex);
    bool success = _songData.deleteStep(selectedStep);
    if (success && _songState != STATE_STOP && selectedStep < _currentPlayStep) {
        _currentPlayStep--;
    }
    portEXIT_CRITICAL(&_songMutex);
    if (success) _songUI.setSaveFlag();
    return success;
}

bool SongSequencer::cloneStep(int selectedStep) {
    portENTER_CRITICAL(&_songMutex);
    bool success = _songData.cloneStep(selectedStep);
    if (success && _songState != STATE_STOP && selectedStep < _currentPlayStep) {
        _currentPlayStep++;
    }
    portEXIT_CRITICAL(&_songMutex);
    if (success) _songUI.setSaveFlag();
    return success;
}

bool SongSequencer::setStepLoop(int stepIndex, int targetIndex, uint8_t count) {
    portENTER_CRITICAL(&_songMutex);
    bool success = _songData.setStepLoop(stepIndex, targetIndex, count);
    portEXIT_CRITICAL(&_songMutex);
    if (success) _songUI.setSaveFlag();
    return success;
}

void SongSequencer::clearStepLoop(int stepIndex) {
    portENTER_CRITICAL(&_songMutex);
    _songData.clearStepLoop(stepIndex);
    portEXIT_CRITICAL(&_songMutex);
    _songUI.setSaveFlag();
}

void SongSequencer::initSequencers() {
    for (int i = 0; i < NUM_TRACKS; i++) {
        _sequencers[i] = new StepSequencer(_display, _userInput);
        _sequencers[i]->setGateQueue(&_gateQueues[i]);
        _sequencers[i]->setPerformanceState(&_perfState, &_songData, i);

        if (i < 4) {
            _sequencers[i]->setQuantizerRef(&_songData.getQuantizer(i));
            _sequencers[i]->setCVQueue(&_cvQueues[i]);
            _sequencers[i]->setQuantizerEnabled(true);
            _sequencers[i]->setResetOnStep(false);
        } else {
            _sequencers[i]->setCVQueue(nullptr);
            _sequencers[i]->setResetOnStep(true);
        }
    }
    updateSequencersFromStep(_currentPlayStep, true);
}

void SongSequencer::update() {
    handleStateMachine();
    _songUI.update();
}

void SongSequencer::drawSongUI() {
    _songUI.setCurrentPlayingStep(_currentPlayStep);
    _songUI.setPlaybackProgress(100*(float)_currentStepTickCounter/(float)_stepTicksRemaining);
    _songUI.draw();
    if(_pendingSequencer || _pendingStart){
        _display.drawHourglass(2, 0);
    }else{
        switch (_songState) {
            case STATE_STOP: _display.fillRect(2, 1, 7, 7, _display.colorWhite()); break;
            case STATE_PLAY_SONG: _display.drawPlaySign(2, 0); break;
            case STATE_LOOP_STEP: _display.drawLoopSign(2, 0); break;
        }
    }
}

void SongSequencer::scheduleSequencerMode(int track, int step) {
    if(_songState == STATE_STOP){
        enterSequencerMode(track, step);
    }else{
        _pendingSequencer = true;
        _nextPlayStep = step;
        _openSequencerTrack = track;
    }
}

void SongSequencer::enterSequencerMode(int track, int step) {
    updateSequencersFromStep(step, false);
    _currentPlayStep = step;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    if (_sequencers[track]) _sequencers[track]->onEnterUI();
    _sequencerModeActive = true;
    if (_songState == STATE_PLAY_SONG) _songState = STATE_LOOP_STEP;
}

void SongSequencer::exitSequencerMode() {
    _songUI.resetEncoderTracking();
    _sequencerModeActive = false;
    if(_songState == STATE_STOP) saveCurrentProject();
}

StepSequencer* SongSequencer::getSequencer(int track) {
    if (track >= 0 && track < NUM_TRACKS) return _sequencers[track];
    return nullptr;
}

void SongSequencer::processClockTick(uint16_t tickCount) {
    if (_pendingStart && ( tickCount == 0 || !_syncStart)) {
        _pendingStart = false;
        _songState = STATE_PLAY_SONG;
        _currentStepTickCounter = 0;
        for (int i = 0; i < _songData.getLength(); i++) {
            _stepLoopCounters[i] = _songData.getStep(i)->loopCount;
        }
        updateSequencersFromStep(_currentPlayStep, true);
        _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    }
    
    if (_songState!= STATE_STOP){
        for (int i = 0; i < NUM_TRACKS; i++) {
            if (_sequencers[i]) _sequencers[i]->onTimerTick(tickCount);
        }
        _currentStepTickCounter++;
        if (_currentStepTickCounter >= _stepTicksRemaining) {
            _currentStepTickCounter = 0;
            advanceToNextStep();
        }
        if (_pendingSequencer && ( _currentStepTickCounter == 0 || _currentPlayStep == _nextPlayStep)){
            _pendingSequencer = false;
            enterSequencerMode(_openSequencerTrack,  _nextPlayStep);
        }
    }else{
        for (int i = 0; i < NUM_TRACKS; i++) {
            if (_sequencers[i]) _sequencers[i]->onTimerTick(7);
        }
    }
}

void SongSequencer::processOutputs() {
    for (int i = 0; i < NUM_TRACKS; i++) {
        if (_sequencers[i]) {
            if (_sequencers[i]->hasStepPending()) _sequencers[i]->onStep();
            _sequencers[i]->processStepOutput();
            if (_sequencers[i]->isGateOutputChanged()) {
                _outputHandler.setDigitalOutput(i, _sequencers[i]->getCurrentGateOutput());
                _sequencers[i]->clearGateOutputChanged();
            }
            if (i < 4 && _songState != STATE_STOP) {
                uint16_t currentDACValue = _sequencers[i]->getCurrentDACValue();
                if (currentDACValue != _lastDACValues[i]) {
                    _outputHandler.setDACChannel(i, currentDACValue);
                    _lastDACValues[i] = currentDACValue;
                }
            }
        }
    }
}

void SongSequencer::updateSequencersFromStep(int step, bool resetPosition) {
    if (step >= _songData.getLength()) return;
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint16_t clockDivider = _songData.getDividerValue(track, step);
        _sequencers[track]->begin(&_songData.getPattern(track, step));
        _sequencers[track]->setClockDivision(clockDivider);
        if(resetPosition) _sequencers[track]->resetPosition();
    }
}

void SongSequencer::handleStateMachine() {
    // NEW: Ignore the stop command if we just exited Performance Mode
    if (_ignoreSaveStop) {
        if (!_userInput.save_button.pressed) {
            _ignoreSaveStop = false; // Clear flag once the button is released
        }
        return;
    }

    if (_userInput.save_button.press_duration > 500) {
        _songState = STATE_STOP;
        _currentStepTickCounter = 0;
        _pendingStart = false;
        _pendingSequencer = false;
        for (int i = 0; i < _songData.getLength(); i++) {
            _stepLoopCounters[i] = _songData.getStep(i)->loopCount;
        }
        _songUI.setSaveFlag();
    } else if (_userInput.save_button.just_pressed) {
        if (_songState == STATE_STOP) {
            _currentPlayStep = _songUI.getSelectedStep();
            _pendingStart = true;
        } else if (_songState == STATE_PLAY_SONG) {
            _songState = STATE_LOOP_STEP;
            _pendingStart = false;
        } else if (_songState == STATE_LOOP_STEP) {
            _songState = STATE_PLAY_SONG;
            _pendingStart = false;
        }
    }
}

void SongSequencer::saveCurrentProject() { _songData.save(PROJECT_FILENAME, _sequencers); }
void SongSequencer::loadCurrentProject() {
    if (!_songData.load(PROJECT_FILENAME, _sequencers)) saveCurrentProject();
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);
    updateSequencersFromStep(0, true);
    _songUI.resetCursor();
}
void SongSequencer::deleteCurrentProject() { _songData.deleteFile(PROJECT_FILENAME); }
void SongSequencer::resetEncoderTracking() { _songUI.resetEncoderTracking(); }
void SongSequencer::saveCurrentProjectToFile(const char* filename) { _songData.save(filename, _sequencers); }
void SongSequencer::loadCurrentProjectFromFile(const char* filename) {
    if (_songData.load(filename, _sequencers)) {
        _currentPlayStep = 0;
        _currentStepTickCounter = 0;
        _stepTicksRemaining = _songData.getStepLengthInTicks(0);
        updateSequencersFromStep(0, true);
        _songUI.clearSaveFlag();
        _songUI.resetCursor();
    }
}
bool SongSequencer::fileExists(const char* filename) { return _songData.exists(filename); }

void SongSequencer::startPlayback() {
    if (_songState != STATE_STOP) return;
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    updateSequencersFromStep(_currentPlayStep, true);
    _pendingStart = true;
}

void SongSequencer::newCurrentProject() {
    _songData.init();
    for (int i = 0; i < NUM_TRACKS; i++) _gateQueues[i].clear();
    for (int i = 0; i < 4; i++) _cvQueues[i].clear();
    for (int i = 0; i < NUM_TRACKS; i++) { delete _sequencers[i]; }
    initSequencers();
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);
    saveCurrentProject();
    _songUI.clearSaveFlag();
    _songUI.resetCursor();
}

void SongSequencer::markProjectDirty() { _songUI.setSaveFlag(); }
void SongSequencer::previewDAC(int track, uint16_t value) {
    if (track < 0 || track >= 4) return;
    if (_songState != STATE_STOP) return;
    _outputHandler.setDACChannel(track, value);
    _lastDACValues[track] = value;
}

// ============================================================================
// Performance Mode Implementation
// ============================================================================

void SongSequencer::enterPerformanceMode() {
    _perfState.active = true;
    _perfState.editMode = false;
    _perfState.editYAxis = false;
    _perfState.selectedTrackX = 0;
    _perfState.selectedTrackY = 0;
    _perfState.activeBank = 0;
    _perfState.enterTime = millis();  // <-- Grace period start
    
    for (int i = 0; i < 6; i++) {
        _perfState.trackMuted[i] = false;
    }
    
    _perfLastEncAPos = _userInput.encoder_a.position;
    _perfLastEncBPos = _userInput.encoder_b.position;
    _perfPopupText[0] = '\0';
    _lastJoyMoveTime = 0;

    // Swallow the entry gesture's button events
    _userInput.save_button.just_pressed = false;
    _userInput.save_button.just_released = false;
    _userInput.encoder_a_button.just_pressed = false;
    _userInput.encoder_b_button.just_pressed = false;
    
    _perfState.ignoreNextSaveRelease = true;
}

void SongSequencer::exitPerformanceMode() {
    _perfState.active = false;
    _perfState.editMode = false;
}

const char* SongSequencer::getEffectName(uint8_t eff) {
    switch (eff) {
        case 1: return "CV MOD";
        case 2: return "SLIDE";
        case 3: return "RANDOM";
        case 4: return "DENSITY";
        default: return "NONE";
    }
}

const char* SongSequencer::getShortEffectDesc(uint8_t eff, bool pos) {
    switch (eff) {
        case 1: return pos ? "+CV" : "-CV";
        case 2: return pos ? "+Attack" : "+Decay";
        case 3: return pos ? "Rnd CV/Ratch" : "-Prob";
        case 4: return pos ? "Repeat steps" : "Skip steps";
        default: return "";
    }
}

void SongSequencer::formatEffectValue(int track, bool isY, char* buf, size_t len) {
    uint8_t eff = isY ? _songData.getEffectY(_perfState.activeBank, track) : _songData.getEffectX(_perfState.activeBank, track);
    if (eff == 0) { snprintf(buf, len, "---"); return; }
    
    int joy = isY ? _perfState.joyY : _perfState.joyX;
    if (joy == 0) { snprintf(buf, len, "---"); return; }

    int8_t mag = isY ? _songData.getMagY(_perfState.activeBank, track) : _songData.getMagX(_perfState.activeBank, track);
    float scaled = (joy / 99.0f) * mag;  // Combined joy*mag, range ±|mag|

    if (scaled == 0) { snprintf(buf, len, "---"); return; }

    if (eff == 1) { // CV MOD
        snprintf(buf, len, "CV%+d", (int)(scaled * 205));
    } 
    else if (eff == 2) { // SLIDE
        int offset = (int)(abs(scaled) * 24);
        if (scaled > 0) snprintf(buf, len, "ATK+%d", offset);
        else snprintf(buf, len, "DCY+%d", offset);
    } 
    else if (eff == 3) { // RANDOM
        if (scaled > 0) {
            snprintf(buf, len, "RND+%d%%", (int)((scaled / 4.0f) * 100.0f));
        } else {
            snprintf(buf, len, "RND-%d%%", (int)((abs(scaled) / 4.0f) * 75.0f));
        }
    } 
    else if (eff == 4) { // DENSITY
        int n_max = round(1 + 3 * (abs(mag) / 4.0f));
        if (n_max < 1) n_max = 1;
        
        float abs_scaled = abs(scaled);
        float abs_mag = abs(mag);
        
        if (scaled > 0) {
            float ratio = abs_scaled / abs_mag;
            int n = (int)(n_max - (n_max - 1) * ratio + 0.5f);
            if (n < 1) n = 1;
            if (n > n_max) n = n_max;
            snprintf(buf, len, "DNS+%d", n);
        } else {
            float ratio = abs_scaled / abs_mag;
            int n = (int)(1 + (n_max - 1) * ratio + 0.5f);
            if (n < 1) n = 1;
            if (n > n_max) n = n_max;
            snprintf(buf, len, "DNS-%d", n);
        }
    }
}

void SongSequencer::showPerfPopup(const char* text) {
    snprintf(_perfPopupText, sizeof(_perfPopupText), "%s", text);
    _perfPopupTime = millis();
}

void SongSequencer::updatePerformanceUI() {
    // Drive the 3-second pattern/modifier auto-revert timer
    _songUI.handlePerformanceTimeout(); 
    // ===== GRACE PERIOD =====
    // Ignore all input for 200ms after entering performance mode.
    // This prevents the entry gesture (Save + Encoder B) from leaking
    // any events (press, release, or click) into the performance UI.
    if (millis() - _perfState.enterTime < 200) {
        // Still update the animation and joystick centering during grace
        _perfState.joyX = 0;
        _perfState.joyY = 0;
        return;
    }

    int jx = _userInput.joystick.x_position;
    int jy = _userInput.joystick.y_position;
    
    _perfState.joyX = (abs(jx) > 10) ? jx : 0;
    _perfState.joyY = (abs(jy) > 10) ? jy : 0;
    
    if (abs(jx) > 10 || abs(jy) > 10) {
        _lastJoyMoveTime = millis();
    }

    if (!_perfState.editMode) {
        // ===== PLAY MODE =====
        if (_userInput.encoder_a.position != _perfLastEncAPos) {
            int delta = (_userInput.encoder_a.position > _perfLastEncAPos) ? 1 : -1;
            _perfState.selectedTrackX = (_perfState.selectedTrackX + delta + 6) % 6;
            _perfLastEncAPos = _userInput.encoder_a.position;
        }
        if (_userInput.encoder_b.position != _perfLastEncBPos) {
            int delta = (_userInput.encoder_b.position > _perfLastEncBPos) ? 1 : -1;
            _perfState.selectedTrackY = (_perfState.selectedTrackY + delta + 6) % 6;
            _perfLastEncBPos = _userInput.encoder_b.position;
        }
        
        if (_userInput.joystick_button.just_pressed) {
            bool newState = !_perfState.trackMuted[_perfState.selectedTrackX];
            _perfState.trackMuted[_perfState.selectedTrackX] = newState;
            _perfState.trackMuted[_perfState.selectedTrackY] = newState;
        }

        if (_userInput.encoder_a_button.just_pressed) {
            _perfState.editMode = true;
            _perfState.editYAxis = false;
        }
        if (_userInput.encoder_b_button.just_pressed) {
            _perfState.editMode = true;
            _perfState.editYAxis = true;
        }
        
        // Cycle Bank on Save Release
        if (_userInput.save_button.just_released) {
            if (_perfState.ignoreNextSaveRelease) {
                _perfState.ignoreNextSaveRelease = false;
            } else {
                _perfState.activeBank = (_perfState.activeBank + 1) % 4;
                char buf[16];
                snprintf(buf, sizeof(buf), "SCENE FX%d", _perfState.activeBank + 1);
                showPerfPopup(buf);
            }
        }
    } else {
        // ===== EDIT MODE =====
        int track = _perfState.editYAxis ? _perfState.selectedTrackY : _perfState.selectedTrackX;
        uint8_t eff = _perfState.editYAxis ? _songData.getEffectY(_perfState.activeBank, track) : _songData.getEffectX(_perfState.activeBank, track);
        int8_t mag = _perfState.editYAxis ? _songData.getMagY(_perfState.activeBank, track) : _songData.getMagX(_perfState.activeBank, track);
        bool isCv = (track < 4);

        // 1. Effect Selection (Encoder A)
        if (_userInput.encoder_a.position != _perfLastEncAPos) {
            int delta = (_userInput.encoder_a.position > _perfLastEncAPos) ? 1 : -1;
            
            uint8_t otherEff = _perfState.editYAxis ? _songData.getEffectX(_perfState.activeBank, track) : _songData.getEffectY(_perfState.activeBank, track);
            
            if (delta > 0) {
                do { 
                    eff = (eff + 1) % 5; 
                } while ((!isCv && (eff == 1 || eff == 2)) || (eff != 0 && eff == otherEff));
            } else {
                do { 
                    eff = (eff + 4) % 5; 
                } while ((!isCv && (eff == 1 || eff == 2)) || (eff != 0 && eff == otherEff));
            }
            
            if (_perfState.editYAxis) _songData.setEffectY(_perfState.activeBank, track, eff);
            else _songData.setEffectX(_perfState.activeBank, track, eff);
            _perfLastEncAPos = _userInput.encoder_a.position;
        }

        // 2. Depth Selection (Encoder B)
        if (_userInput.encoder_b.position != _perfLastEncBPos) {
            int delta = (_userInput.encoder_b.position > _perfLastEncBPos) ? 1 : -1;
            mag = mag + delta;
            if (mag > 4) mag = -4;
            if (mag < -4) mag = 4;
            if (mag == 0) mag = (delta > 0) ? 1 : -1;
            
            if (_perfState.editYAxis) _songData.setMagY(_perfState.activeBank, track, mag);
            else _songData.setMagX(_perfState.activeBank, track, mag);
            _perfLastEncBPos = _userInput.encoder_b.position;
        }

        // 3. Confirm and Exit
        if (_userInput.encoder_a_button.just_pressed || _userInput.encoder_b_button.just_pressed) {
            _perfState.editMode = false;
            markProjectDirty();
        }
    }
}

void SongSequencer::drawPerformanceUI() {
    _songUI.setCurrentPlayingStep(_currentPlayStep);
    _songUI.setPlaybackProgress(100*(float)_currentStepTickCounter/(float)_stepTicksRemaining);
    _songUI.draw(&_perfState);

    // Draw FX Bank indicator instead of Staircase
    char fxBuf[8];
    snprintf(fxBuf, sizeof(fxBuf), "FX%d", _perfState.activeBank + 1);
    _display.printAt(fxBuf, 2, 2, ALIGN_LEFT);

    if (_perfState.editMode) {
        // ===== EDIT MODE POPUP =====
        int track = _perfState.editYAxis ? _perfState.selectedTrackY : _perfState.selectedTrackX;
        uint8_t eff = _perfState.editYAxis ? _songData.getEffectY(_perfState.activeBank, track) : _songData.getEffectX(_perfState.activeBank, track);
        int8_t mag = _perfState.editYAxis ? _songData.getMagY(_perfState.activeBank, track) : _songData.getMagX(_perfState.activeBank, track);

        _display.fillRect(2, 4, 124, 56, _display.colorWhite());
        _display.setTextColor(_display.colorBlack());
        
        char buf[32];
        // Line 1: Title
        snprintf(buf, sizeof(buf), "T%d-%c FX: %s", track+1, _perfState.editYAxis ? 'Y' : 'X', getEffectName(eff));
        _display.printCentered(buf, 8);
        
        // Line 2: Depth
        snprintf(buf, sizeof(buf), "DEPTH: %+d", mag);
        _display.printCentered(buf, 18);
        
        // Lines 3 & 4: Direction + Effect
        const int colonX = 44;
        const int effX = colonX + 8;
        
        // Flip the descriptions if depth is negative
        bool depthPositive = (mag >= 0);
        const char* descRightUp = getShortEffectDesc(eff, depthPositive);
        const char* descLeftDown = getShortEffectDesc(eff, !depthPositive);

        if (_perfState.editYAxis) {
            _display.printAt("Up:", colonX - 18, 32, ALIGN_LEFT);
            _display.printAt("Down:", colonX - 30, 42, ALIGN_LEFT);
        } else {
            _display.printAt("Right:", colonX - 36, 32, ALIGN_LEFT);
            _display.printAt("Left:", colonX - 30, 42, ALIGN_LEFT);
        }
        _display.printAt(descRightUp, effX, 32, ALIGN_LEFT);
        _display.printAt(descLeftDown, effX, 42, ALIGN_LEFT);
        
        _display.setTextColor(_display.colorWhite());
    } 
    else if (millis() - _lastJoyMoveTime < 500) {
        // ===== JOYSTICK FULL-SCREEN POPUP =====
        _display.fillRect(0, 0, 128, 64, _display.colorWhite());
        _display.setTextColor(_display.colorBlack());
        
        // 1. Column Headers
        char buf[16];
        snprintf(buf, sizeof(buf), "X=%+d", _perfState.joyX);
        _display.printAt(buf, 32, 2, ALIGN_LEFT);
        snprintf(buf, sizeof(buf), "Y=%+d", _perfState.joyY);
        _display.printAt(buf, 80, 2, ALIGN_LEFT);
        
        // 2. Grid Lines
        _display.drawLine(0, 12, 127, 12, _display.colorBlack());
        _display.drawLine(24, 0, 24, 63, _display.colorBlack());
        _display.drawLine(74, 0, 74, 63, _display.colorBlack());
        
        // 3. The 6 track rows
        for (int i = 0; i < 6; i++) {
            char valX[16], valY[16], label[8];
            formatEffectValue(i, false, valX, sizeof(valX));
            formatEffectValue(i, true, valY, sizeof(valY));
            
            int y = 14 + i * 8;
            snprintf(label, sizeof(label), "T%d", i+1);
            
            _display.printAt(label, 4, y, ALIGN_LEFT);
            _display.printAt(valX, 32, y, ALIGN_LEFT);
            _display.printAt(valY, 80, y, ALIGN_LEFT);
        }
        
        _display.setTextColor(_display.colorWhite());
    }
}