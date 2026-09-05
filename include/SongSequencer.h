#ifndef SONG_SEQUENCER_H
#define SONG_SEQUENCER_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "OutputHandler.h"
#include "SongData.h"
#include "SongUI.h"
#include "StepSequencer.h"
#include "EventQueue.h"

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
    
    // Sequencer mode management (called from main.cpp)
    void enterSequencerMode(int track, int step);
    void exitSequencerMode();
    void setQuarterNoteTimeUs(uint64_t quarterNoteTimeUs) { _quarterNoteTimeUs = quarterNoteTimeUs; }
    void scheduleSequencerMode(int track, int step);
    StepSequencer* getSequencer(int track);
    void setSyncStart(bool enabled) { _syncStart = enabled; }
    
    // Getters
    SongState getSongState() const { return _songState; }
    int getSelectedTrack() const { return _songUI.getSelectedTrack(); }
    int getSelectedStep() const { return _songUI.getSelectedStep(); }
    bool isInSequencerMode() const {return _sequencerModeActive; }

    void saveCurrentProject();
    void loadCurrentProject();
    void saveCurrentProjectToFile(const char* filename);
    void loadCurrentProjectFromFile(const char* filename);
    void deleteCurrentProject();
    bool fileExists(const char* filename);

    void newCurrentProject();

    void resetEncoderTracking();
    void startPlayback();

private:
    static const int NUM_TRACKS = 6;
    static const char* PROJECT_FILENAME;
  
    DisplayManager& _display;
    UserInput& _userInput;
    OutputHandler& _outputHandler;
    
    SongData _songData;
    SongUI _songUI;
    StepSequencer* _sequencers[NUM_TRACKS];

    // Event queues
    GateQueue _gateQueues[6];
    CVQueue _cvQueues[4];

    TaskHandle_t _saveTaskHandle;
    static void saveTaskWrapper(void* parameter);
    void saveTask();
    
    volatile SongState _songState;
    int _currentPlayStep;
    int _nextPlayStep;
    int _openSequencerTrack;
    uint32_t _currentStepTickCounter;
    uint32_t _stepTicksRemaining;
    uint64_t _quarterNoteTimeUs;
    
    void initSequencers();
    void updateSequencersFromStep(int step, bool resetPosition);
    void advanceToNextStep();
    void handleStateMachine();

    bool _pendingStart;      // Flag for synchronized start
    bool _pendingSequencer;  // Flag for switching to sequencer mode
    bool _sequencerModeActive;

    unsigned long _lastSaveTime;
    static const unsigned long SAVE_TIMEOUT_MS = 5000;  // 5 seconds

    bool _syncStart;

};

#endif