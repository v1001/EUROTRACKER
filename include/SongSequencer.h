#ifndef SONG_SEQUENCER_H
#define SONG_SEQUENCER_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "OutputHandler.h"
#include "SongData.h"
#include "SongUI.h"
#include "StepSequencer.h"
#include "EventQueue.h"

struct PerformanceState {
    bool active = false;
    int8_t selectedTrack = 0;
    
    // Per-track settings
    bool trackEnabled[6] = {false}; // Inverted header, enables global FX
    bool trackMuted[6] = {false};   // "M1" instead of "T1", hides patterns
    int16_t cvOffset[6] = {0};      // Per-track transpose (Encoder B rotate)
    
    // Global modifiers (apply ONLY to tracks where trackEnabled == true)
    bool globalCvHold = false;      // Encoder B click
    int8_t globalStepRepeat = 0;    // Joystick Right (0=Off, 1=1/4, 2=1/2, 3=1/1)
    int8_t globalGateHold = 0;      // Joystick Left (0=Off, 1=2x, 2=3x, 3=4x)
    int8_t globalRandomize = 0;     // Joystick Up (0=Off, 1=Low, 2=Med, 3=High)
    int8_t globalProbReduce = 0;    // Joystick Down (0=Off, 1=75%, 2=50%, 3=25%)
};

class SongSequencer {
public:
    // Song playback states
    enum SongState {
        STATE_STOP,
        STATE_PLAY_SONG,
        STATE_LOOP_STEP
    };
    
    SongSequencer(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler);
    ~SongSequencer();

    SongData& getSongData() { return _songData; }
    
    void begin(bool reset = false);
    void update();
    void drawSongUI();
    
    void processClockTick(uint16_t tickCount);
    void processOutputs();
    
    // Sequencer mode management
    void enterSequencerMode(int track, int step);
    void exitSequencerMode();
    void scheduleSequencerMode(int track, int step);
    StepSequencer* getSequencer(int track);
    void setSyncStart(bool enabled) { _syncStart = enabled; }
    
    // Performance Mode
    void enterPerformanceMode();
    void exitPerformanceMode();
    bool isPerformanceMode() const { return _perfState.active; }
    void updatePerformanceUI();
    void drawPerformanceUI();
    PerformanceState& getPerformanceState() { return _perfState; }
    
    // Getters
    SongState getSongState() const { return _songState; }
    int getSelectedTrack() const { return _songUI.getSelectedTrack(); }
    int getSelectedStep() const { return _songUI.getSelectedStep(); }
    int getPlayingStep() const { return _currentPlayStep; }
    bool isInSequencerMode() const {return _sequencerModeActive; }

    // Expose save flags for Core 0 autosave logic
    bool needsSave() const { return _songUI.needsSave(); }
    void clearSaveFlag() { _songUI.clearSaveFlag(); }

    // Structural Edits
    bool insertStep(int selectedStep);
    bool deleteStep(int selectedStep);
    bool cloneStep(int selectedStep);
    
    // Loop Edits (mutex protected)
    bool setStepLoop(int stepIndex, int targetIndex, uint8_t count);
    void clearStepLoop(int stepIndex);

    void saveCurrentProject();
    void loadCurrentProject();
    void saveCurrentProjectToFile(const char* filename);
    void loadCurrentProjectFromFile(const char* filename);
    void deleteCurrentProject();
    bool fileExists(const char* filename);

    void newCurrentProject();

    void resetEncoderTracking();
    void startPlayback();

    void markProjectDirty();
    void previewDAC(int track, uint16_t value);

private:
    static const int NUM_TRACKS = 6;
    static const char* PROJECT_FILENAME;
    static const uint32_t MAX_SONG_LENGTH = 64;
    uint16_t _lastDACValues[4];
  
    DisplayManager& _display;
    UserInput& _userInput;
    OutputHandler& _outputHandler;
    
    SongData _songData;
    SongUI _songUI;
    StepSequencer* _sequencers[NUM_TRACKS];
    uint8_t _stepLoopCounters[MAX_SONG_LENGTH];

    // Performance Mode State
    PerformanceState _perfState;
    unsigned long _lastPerfAnimTime;
    uint8_t _perfAnimFrame;
    long _perfLastEncAPos;
    long _perfLastEncBPos;
    char _perfPopupText[32];
    unsigned long _perfPopupTime;

    void showPerfPopup(const char* text);

    // Event queues
    GateQueue _gateQueues[6];
    CVQueue _cvQueues[4];

    // Mutex to protect SongData array shifts from the ISR
    portMUX_TYPE _songMutex = portMUX_INITIALIZER_UNLOCKED;
    
    volatile SongState _songState;
    int _currentPlayStep;
    int _nextPlayStep;
    int _openSequencerTrack;
    uint32_t _currentStepTickCounter;
    uint32_t _stepTicksRemaining;
    
    void initSequencers();
    void updateSequencersFromStep(int step, bool resetPosition);
    void advanceToNextStep();
    void handleStateMachine();

    bool _pendingStart;
    bool _pendingSequencer;
    bool _sequencerModeActive;

    bool _syncStart;
};

#endif