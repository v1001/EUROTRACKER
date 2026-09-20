#include "SongSequencer.h"

const char* SongSequencer::PROJECT_FILENAME = "/current_project.song";

SongSequencer::SongSequencer(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler),
      _songUI(display, userInput, _songData),_sequencerModeActive(false), _lastSaveTime(0),
      _songState(STATE_STOP), _currentPlayStep(0), _currentStepTickCounter(0), _stepTicksRemaining(0),
      _pendingStart(false), _pendingSequencer(false), _nextPlayStep(0), _openSequencerTrack(0),
      _gateQueues{}, _cvQueues{} {;
    
    for (int i = 0; i < NUM_TRACKS; i++) {
        _sequencers[i] = nullptr;
    }
}

SongSequencer::~SongSequencer() {
    for (int i = 0; i < NUM_TRACKS; i++) {
        delete _sequencers[i];
    }
}

void SongSequencer::saveTaskWrapper(void* parameter) {
    SongSequencer* instance = (SongSequencer*)parameter;
    instance->saveTask();
}

void SongSequencer::saveTask() {
    const TickType_t interval = pdMS_TO_TICKS(15000);  // 15 seconds
    
    while (1) {
        vTaskDelay(interval);
        
        // Check if save is needed and we're in STOP state
        if (_songState == STATE_STOP && _songUI.needsSave()) {
            _songUI.clearSaveFlag();
            saveCurrentProject();
        }
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
        // Delete existing project and save new empty one
        deleteCurrentProject();
        saveCurrentProject();
    } else {
        // Normal load
        loadCurrentProject();
    }

    // Create save task on Core 0
    xTaskCreatePinnedToCore(
        saveTaskWrapper,
        "SaveTask",
        4096,
        this,
        1,
        &_saveTaskHandle,
        0  // Core 0
    );
}

void SongSequencer::initSequencers() {
    for (int i = 0; i < NUM_TRACKS; i++) {
        _sequencers[i] = new StepSequencer(_display, _userInput);
        
        // Pass per-track gate queue
        _sequencers[i]->setGateQueue(&_gateQueues[i]);
        
        // Pass quantizer reference and CV queue for melodic tracks (0-3)
        if (i < 4) {
            _sequencers[i]->setQuantizerRef(&_songData.getQuantizer(i));
            _sequencers[i]->setCVQueue(&_cvQueues[i]);   // Pass corresponding CV queue
        } else {
            // Gate tracks (4-5) have no CV queue
            _sequencers[i]->setCVQueue(nullptr);
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
    // Load the selected step pattern into the sequencer
    updateSequencersFromStep(step, false);
    _currentPlayStep = step;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);

    // Notify the sequencer that we are entering its UI
    if (_sequencers[track]) {
        _sequencers[track]->onEnterUI();
    }

    _sequencerModeActive = true;
    
    // Only change to LOOP_STEP if we were in PLAY_SONG mode
    if (_songState == STATE_PLAY_SONG) {
        _songState = STATE_LOOP_STEP;
    }
}

void SongSequencer::exitSequencerMode() {
    _songUI.resetEncoderTracking();
    _sequencerModeActive = false;
    if(_songState == STATE_STOP){
        saveCurrentProject();
    }
}

StepSequencer* SongSequencer::getSequencer(int track) {
    if (track >= 0 && track < NUM_TRACKS) {
        return _sequencers[track];
    }
    return nullptr;
}

void SongSequencer::processClockTick(uint16_t tickCount) {
    // Handle pending start (synchronized to next clock tick)
    if (_pendingStart && ( tickCount % 48 == 0 || !_syncStart)) {
        _pendingStart = false;
        _songState = STATE_PLAY_SONG;
        _currentStepTickCounter = 0;
        updateSequencersFromStep(_currentPlayStep, true);
        _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    }
    
    if (_songState!= STATE_STOP){
        // Pass tick to all sequencers
        for (int i = 0; i < NUM_TRACKS; i++) {
            if (_sequencers[i]) {
                _sequencers[i]->onTimerTick(tickCount);
            }
        }
        
        // Handle song step advancement
        _currentStepTickCounter++;
        if (_currentStepTickCounter >= _stepTicksRemaining) {
            _currentStepTickCounter = 0;
            advanceToNextStep();
        }
        // Handle pending sequencer mode (synchronized to next clock tick)
        if (_pendingSequencer && ( _currentStepTickCounter == 0 || _currentPlayStep == _nextPlayStep)){
            _pendingSequencer = false;
            enterSequencerMode(_openSequencerTrack,  _nextPlayStep);
        }
    }else{
        for (int i = 0; i < NUM_TRACKS; i++) {
            if (_sequencers[i]) {
                _sequencers[i]->onTimerTick(7);
            }
        }
    }
}

void SongSequencer::processOutputs() {
    static uint16_t lastDACValues[4] = {0, 0, 0, 0};
    
    for (int i = 0; i < NUM_TRACKS; i++) {
        if (_sequencers[i]) {
            if (_sequencers[i]->hasStepPending()) {
                _sequencers[i]->onStep();
            }
            _sequencers[i]->processStepOutput();
            
            if (_sequencers[i]->isGateOutputChanged()) {
                _outputHandler.setDigitalOutput(i, _sequencers[i]->getCurrentGateOutput());
                _sequencers[i]->clearGateOutputChanged();
            }
            
            if (i < 4) {
                uint16_t currentDACValue = _sequencers[i]->getCurrentDACValue();
                if (currentDACValue != lastDACValues[i]) {
                    _outputHandler.setDACChannel(i, currentDACValue);
                    lastDACValues[i] = currentDACValue;
                }
            }
        }
    }
}

void SongSequencer::advanceToNextStep() {
    int nextStep = _currentPlayStep;
    if (_songState == STATE_PLAY_SONG) {
        nextStep = nextStep + 1;
        if (nextStep >= _songData.getLength()) {
            nextStep = 0;
        }
    }
    
    if(nextStep != _currentPlayStep){
        updateSequencersFromStep(nextStep, true);
    }else{
        updateSequencersFromStep(nextStep, false);
    }
    
    _currentPlayStep = nextStep;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
}

void SongSequencer::updateSequencersFromStep(int step, bool resetPosition) {
    if (step >= _songData.getLength()) return;
    
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint16_t clockDivider = _songData.getDividerValue(track, step);
        _sequencers[track]->begin(&_songData.getPattern(track, step));
        _sequencers[track]->setClockDivision(clockDivider);
        if(resetPosition){
            _sequencers[track]->resetPosition();
        }
    }
}

void SongSequencer::handleStateMachine() {
    // Song State transitions
    if (_userInput.save_button.press_duration > 500) {
        _songState = STATE_STOP;
        _currentStepTickCounter = 0;
        _pendingStart = false;
        _songUI.setSaveFlag();
    } else if (_userInput.save_button.just_pressed) {
        if (_songState == STATE_STOP) {
            // Don't start immediately - wait for next clock tick
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
    if (!_songData.load(PROJECT_FILENAME, _sequencers)) {
        saveCurrentProject();
    }
    // Reset to first step
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);
    updateSequencersFromStep(0, true);
    // Also reset the UI cursor
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
    
    // Start from the first step
    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(_currentPlayStep);
    
    // Update sequencers to load the first step's patterns
    updateSequencersFromStep(_currentPlayStep, true);
    
    // Schedule start on next clock tick (for sync)
    _pendingStart = true;
}

void SongSequencer::newCurrentProject() {
    _songData.init();

    // Clear any pending gate/CV events from the previous song.
    // Queues are owned by SongSequencer, not by the sequencers, so they
    // survive recreation and must be emptied explicitly.
    for (int i = 0; i < NUM_TRACKS; i++) _gateQueues[i].clear();
    for (int i = 0; i < 4; i++)         _cvQueues[i].clear();

    // Recreate sequencers with fresh state.
    // Null-then-delete: the timer ISR reads _sequencers[i] and must never
    // see a dangling pointer.
    for (int i = 0; i < NUM_TRACKS; i++) {
        StepSequencer* old = _sequencers[i];
        _sequencers[i] = nullptr;
        delete old;
    }
    initSequencers();   // also wires queues, quantizer refs, calls updateSequencersFromStep(0, true)

    _currentPlayStep = 0;
    _currentStepTickCounter = 0;
    _stepTicksRemaining = _songData.getStepLengthInTicks(0);

    saveCurrentProject();
    _songUI.clearSaveFlag();
    _songUI.resetCursor();
}