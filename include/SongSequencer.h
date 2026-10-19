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
    bool editMode = false;
    bool editYAxis = false;
    
    int8_t selectedTrackX = 0;
    int8_t selectedTrackY = 0;
    bool trackMuted[6] = {false};
    
    int8_t joyX = 0;
    int8_t joyY = 0;
};

class SongSequencer {
public:
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
    
    SongState getSongState() const { return _songState; }
    int getSelectedTrack() const { return _songUI.getSelectedTrack(); }
    int getSelectedStep() const { return _songUI.getSelectedStep(); }
    int getPlayingStep() const { return _currentPlayStep; }
    bool isInSequencerMode() const {return _sequencerModeActive; }

    bool needsSave() const { return _songUI.needsSave(); }
    void clearSaveFlag() { _songUI.clearSaveFlag(); }

    bool insertStep(int selectedStep);
    bool deleteStep(int selectedStep);
    bool cloneStep(int selectedStep);
    
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

    PerformanceState _perfState;
    unsigned long _lastPerfAnimTime;
    unsigned long _lastJoyMoveTime;
    uint8_t _perfAnimFrame;
    long _perfLastEncAPos;
    long _perfLastEncBPos;
    char _perfPopupText[32];
    unsigned long _perfPopupTime;

    const char* getEffectName(uint8_t eff);
    const char* getShortEffectDesc(uint8_t eff, bool pos);
    void formatEffectValue(int track, bool isY, char* buf, size_t len);
    void showPerfPopup(const char* text);

    GateQueue _gateQueues[6];
    CVQueue _cvQueues[4];

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