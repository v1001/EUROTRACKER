#ifndef TRACKER_APP_H
#define TRACKER_APP_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "OutputHandler.h"
#include "SongSequencer.h"
#include "GlobalSettings.h"

// Forward declarations
class MainMenu;
class PatternMenu;

class TrackerApp {
public:
    TrackerApp(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler);
    ~TrackerApp();
    
    void begin(bool reset = false);
    void update(uint64_t quarterNoteTimeUs);
    
    // Timer and output handling
    void processClockTick(uint16_t tickCount);
    void processOutputs();

    int getCurrentUIState() const { return static_cast<int>(_currentUIState); }
    
    // Getters for UI
    bool isInSequencerMode() const { return _currentUIState == STATE_SEQUENCER_UI; }
    StepSequencer* getCurrentSequencer() { return _sequencer; }
    
    // Access to global settings for main.cpp
    static GlobalSettings::ClockSource getClockSource() { return GlobalSettings::clockSource; }
    static uint16_t getInternalBPM() { return GlobalSettings::internalBPM; }          // raw scaled ×10
    static float getInternalBPMFloat() { return GlobalSettings::internalBPM / 10.0f; } // for display/timer
    static uint8_t getExternalPPQN() { return GlobalSettings::externalPPQN; }
    static uint16_t getExternalPPQNValue() { return GlobalSettings::getExternalPPQNValue(); }
    static bool getAutoplay() { return GlobalSettings::autoplay; }
    static bool getSyncStart() { return GlobalSettings::syncStart; }
    static uint8_t getJoystickSpeed() { return GlobalSettings::joystickSpeed; }
    static uint8_t getBrightness() { return GlobalSettings::brightness; }
    
    // Methods for menu to modify settings
    void setClockSource(GlobalSettings::ClockSource source);
    void setInternalBPM(uint16_t bpm);          // scaled ×10 (e.g., 1200 = 120.0)
    void setExternalPPQN(uint8_t index);
    void setAutoplay(bool enabled);
    void setSyncStart(bool enabled);
    void setJoystickSpeed(uint8_t speed);
    void setBrightness(uint8_t value);
    
    // Save/load project
    void saveCurrentProject();
    void loadCurrentProject();
    void saveCurrentProjectToSlot(int slot);
    void loadCurrentProjectFromSlot(int slot);
    bool songExists(const char* filename);

    void newCurrentProject();
    
    // SongSequencer access for menus
    SongSequencer& getSongSequencer() { return _songSequencer; }

    uint32_t getSongLength();
    void setSongLength(uint32_t length);

private:

    // UI State Machine
    enum UIState {
        STATE_SONG_UI,
        STATE_SEQUENCER_UI,
        STATE_MAIN_MENU,
        STATE_PATTERN_MENU,
        STATE_TRACK_MENU
    };
    
    DisplayManager& _display;
    UserInput& _userInput;
    OutputHandler& _outputHandler;
    SongSequencer _songSequencer;
    
    UIState _currentUIState;
    StepSequencer* _sequencer;
    int _editingTrack;
    int _editingStep;
    // State transition cooldown
    uint64_t _lastStateTransitionTime;
    uint64_t _quarterNoteTimeUs;
    static const uint64_t STATE_TRANSITION_COOLDOWN_MS = 500;
    
    // Menu instances (forward declared, implemented elsewhere)
    void* _mainMenu;
    void* _patternMenu;
    void* _trackMenu;
    
    void enterSequencerUI(int track, int step);
    void exitSequencerUI();
    const char* getDividerText(uint16_t divider);
    void drawPatternInfoText();
    
    // Apply settings to hardware
    void applyBrightness();
    void applyJoystickSpeed();

    void drawBPM();

};

#endif // TRACKER_APP_H