#include "SongSequencer.h"

const char* SongSequencer::PROJECT_FILENAME = "/current_project.song";

SongSequencer::SongSequencer(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler),
      _songUI(display, userInput, _songData), _sequencerModeActive(false),
      _songState(STATE_STOP), _currentPlayStep(0), _currentStepTickCounter(0), _stepTicksRemaining(0),
      _pendingStart(false), _pendingSequencer(false), _nextPlayStep(0), _openSequencerTrack(0),
      _gateQueues{}, _cvQueues{}, _lastDACValues{0, 0, 0, 0},
      _lastPerfAnimTime(0), _perfAnimFrame(0), _perfLastEncAPos(0), _perfLastEncBPos(0), _perfPopupTime(0) {
    
    for (int i = 0; i < NUM_TRACKS; i++) {
        _sequencers[i] = nullptr;
    }
    _perfPopupText[0] = '\0';
}

SongSequencer::~SongSequencer() {
    for (int i = 0; i < NUM_TRACKS; i++) {
        delete _sequencers[i];
    }
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
        
        // Wire up Performance State
        _sequencers[i]->setPerformanceState(&_perfState, i);

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
            case STATE_STOP:
                _display.fillRect(2, 1, 7, 7, _display.colorWhite());
                break;
            case STATE_PLAY_SONG:
                _display.drawPlaySign(2, 0);
                break;
            case STATE_LOOP_STEP:
                _display.drawLoopSign(2, 0);
                break;
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

void SongSequencer::saveCurrentProject() {
    _songData.save(PROJECT_FILENAME, _sequencers);
}

void SongSequencer::loadCurrentProject() {
    if (!_songData.load(PROJECT_FILENAME, _sequencers)) saveCurrentProject();
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);
    updateSequencersFromStep(0, true);
    _songUI.resetCursor();
}

void SongSequencer::deleteCurrentProject() {
    _songData.deleteFile(PROJECT_FILENAME);
}

void SongSequencer::resetEncoderTracking() {
    _songUI.resetEncoderTracking();
}

void SongSequencer::saveCurrentProjectToFile(const char* filename) {
    _songData.save(filename, _sequencers);
}

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

bool SongSequencer::fileExists(const char* filename) {
    return _songData.exists(filename);
}

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
    for (int i = 0; i < 4; i++)         _cvQueues[i].clear();
    for (int i = 0; i < NUM_TRACKS; i++) {
        StepSequencer* old = _sequencers[i];
        _sequencers[i] = nullptr;
        delete old;
    }
    initSequencers();
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);
    saveCurrentProject();
    _songUI.clearSaveFlag();
    _songUI.resetCursor();
}

void SongSequencer::markProjectDirty() {
    _songUI.setSaveFlag();
}

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
    _perfState.selectedTrack = 0;
    
    for (int i = 0; i < 6; i++) {
        _perfState.trackEnabled[i] = false;
        _perfState.trackMuted[i] = false;
        _perfState.cvOffset[i] = 0;
    }
    _perfState.globalCvHold = false;
    _perfState.globalStepRepeat = 0;
    _perfState.globalGateHold = 0;
    _perfState.globalRandomize = 0;
    _perfState.globalProbReduce = 0;
    
    _perfLastEncAPos = _userInput.encoder_a.position;
    _perfLastEncBPos = _userInput.encoder_b.position;
    _perfPopupText[0] = '\0';
}

void SongSequencer::exitPerformanceMode() {
    _perfState.active = false;
}

void SongSequencer::showPerfPopup(const char* text) {
    snprintf(_perfPopupText, sizeof(_perfPopupText), "%s", text);
    _perfPopupTime = millis();
}

void SongSequencer::updatePerformanceUI() {
    // 1. Encoder A: Track Selection
    if (_userInput.encoder_a.position != _perfLastEncAPos) {
        int delta = (_userInput.encoder_a.position > _perfLastEncAPos) ? 1 : -1;
        _perfState.selectedTrack += delta;
        if (_perfState.selectedTrack < 0) _perfState.selectedTrack = 5;
        if (_perfState.selectedTrack > 5) _perfState.selectedTrack = 0;
        _perfLastEncAPos = _userInput.encoder_a.position;
    }

    // 2. Encoder B: CV Offset
    if (_userInput.encoder_b.position != _perfLastEncBPos) {
        int delta = (_userInput.encoder_b.position > _perfLastEncBPos) ? 1 : -1;
        _perfState.cvOffset[_perfState.selectedTrack] += (delta * 50);
        if (_perfState.cvOffset[_perfState.selectedTrack] > 2000) _perfState.cvOffset[_perfState.selectedTrack] = 2000;
        if (_perfState.cvOffset[_perfState.selectedTrack] < -2000) _perfState.cvOffset[_perfState.selectedTrack] = -2000;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "T%d CV: %+d", _perfState.selectedTrack + 1, _perfState.cvOffset[_perfState.selectedTrack]);
        showPerfPopup(buf);
        _perfLastEncBPos = _userInput.encoder_b.position;
    }

    // 3. Buttons
    if (_userInput.encoder_a_button.just_released) {
        _perfState.trackEnabled[_perfState.selectedTrack] = !_perfState.trackEnabled[_perfState.selectedTrack];
        char buf[32];
        snprintf(buf, sizeof(buf), "T%d FX: %s", _perfState.selectedTrack + 1, _perfState.trackEnabled[_perfState.selectedTrack] ? "ON" : "OFF");
        showPerfPopup(buf);
    }
    
    if (_userInput.encoder_b_button.just_released) {
        _perfState.globalCvHold = !_perfState.globalCvHold;
        showPerfPopup(_perfState.globalCvHold ? "HOLD: ON" : "HOLD: OFF");
    }
    
    if (_userInput.joystick_button.just_released) {
        _perfState.trackMuted[_perfState.selectedTrack] = !_perfState.trackMuted[_perfState.selectedTrack];
        char buf[32];
        snprintf(buf, sizeof(buf), "T%d %s", _perfState.selectedTrack + 1, _perfState.trackMuted[_perfState.selectedTrack] ? "MUTED" : "UNMUTED");
        showPerfPopup(buf);
    }

    // 4. Joystick Modifiers
    int jx = _userInput.joystick.x_position;
    int jy = _userInput.joystick.y_position;

    if (jx > 30) _perfState.globalStepRepeat = (jx > 70) ? 3 : ((jx > 50) ? 2 : 1);
    else _perfState.globalStepRepeat = 0;

    if (jx < -30) _perfState.globalGateHold = (jx < -70) ? 3 : ((jx < -50) ? 2 : 1);
    else _perfState.globalGateHold = 0;

    if (jy < -30) _perfState.globalRandomize = (jy < -70) ? 3 : ((jy < -50) ? 2 : 1);
    else _perfState.globalRandomize = 0;

    if (jy > 30) _perfState.globalProbReduce = (jy > 70) ? 3 : ((jy > 50) ? 2 : 1);
    else _perfState.globalProbReduce = 0;

    // 5. Animation
    if (millis() - _lastPerfAnimTime > 100) {
        _lastPerfAnimTime = millis();
        _perfAnimFrame = (_perfAnimFrame + 1) % 8;
    }
}

void SongSequencer::drawPerformanceUI() {
    // 1. Draw the base song UI
    _songUI.setCurrentPlayingStep(_currentPlayStep);
    _songUI.setPlaybackProgress(100*(float)_currentStepTickCounter/(float)_stepTicksRemaining);
    _songUI.draw();

    // 2. Overlay Track Headers (Invert for FX, M for Mute)
    const int trackX[6] = {20, 38, 56, 74, 92, 110};
    for (int i = 0; i < 6; i++) {
        int x = trackX[i] + 4; // Text position from SongUI
        bool enabled = _perfState.trackEnabled[i];
        bool muted = _perfState.trackMuted[i];

        if (muted) {
            // Mute: Black background, "M", hide patterns
            _display.fillRect(trackX[i], 0, 16, 10, _display.colorBlack());
            _display.setTextColor(_display.colorWhite());
            char label[4];
            snprintf(label, sizeof(label), "M%d", i + 1);
            _display.printAt(label, x, 0, ALIGN_LEFT);
            
            // Hide patterns for muted track
            _display.fillRect(trackX[i], 11, 16, 43, _display.colorBlack());
        } else if (enabled) {
            // FX Enabled: Inverted background
            _display.fillRect(trackX[i], 0, 16, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            char label[4];
            snprintf(label, sizeof(label), "T%d", i + 1);
            _display.printAt(label, x, 0, ALIGN_LEFT);
            _display.setTextColor(_display.colorWhite());
        }
    }

    // 3. Selection Frame
    int selX = trackX[_perfState.selectedTrack];
    _display.drawRect(selX - 2, 0, 20, 10, _display.colorWhite());

    // 4. Staircase Animation (Replaces Play/Stop icons)
    _display.drawStaircase(2, 0, _perfAnimFrame);

    // 5. Status Bar (Global modifiers)
    char status[32] = "";
    if (_perfState.globalCvHold) strcat(status, "HLD ");
    if (_perfState.globalStepRepeat > 0) {
        char r[8]; snprintf(r, sizeof(r), "REP%d ", _perfState.globalStepRepeat);
        strcat(status, r);
    }
    if (_perfState.globalGateHold > 0) {
        char g[8]; snprintf(g, sizeof(g), "GAT%d ", _perfState.globalGateHold);
        strcat(status, g);
    }
    if (_perfState.globalRandomize > 0) {
        char rnd[8]; snprintf(rnd, sizeof(rnd), "RND%d ", _perfState.globalRandomize);
        strcat(status, rnd);
    }
    if (_perfState.globalProbReduce > 0) {
        char p[8]; snprintf(p, sizeof(p), "PRB%d ", _perfState.globalProbReduce * 25);
        strcat(status, p);
    }
    
    if (strlen(status) > 0) {
        _display.printAt(status, 2, 54, ALIGN_LEFT);
    }

    // 6. Popup
    if (_perfPopupText[0] != '\0' && (millis() - _perfPopupTime < 1000)) {
        const int BOX_W = 108;
        const int BOX_H = 16;
        const int BOX_X = (128 - BOX_W) / 2;
        const int BOX_Y = (64 - BOX_H) / 2;
        
        _display.fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, _display.colorWhite());
        _display.setTextColor(_display.colorBlack());
        _display.printCentered(_perfPopupText, BOX_Y + 4);
        _display.setTextColor(_display.colorWhite());
    } else {
        _perfPopupText[0] = '\0';
    }
}