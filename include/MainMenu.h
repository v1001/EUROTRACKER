#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "OutputHandler.h"
#include "GlobalSettings.h"

// Forward declaration
class TrackerApp;

class MainMenu {
public:
    MainMenu(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler, TrackerApp* trackerApp);
    ~MainMenu();
    
    void enter();
    void update();
    void draw();
    void saveAndExit();
    
    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        MENU_CLOCK_SOURCE,
        MENU_INTERNAL_BPM,
        MENU_EXTERNAL_PPQN,
        MENU_AUTOPLAY,
        MENU_SYNC_START,
        MENU_JOYSTICK_SPEED,
        MENU_BRIGHTNESS,
        MENU_SONG_LENGTH,
        MENU_NEW_SONG,
        MENU_SAVE_SONG,
        MENU_LOAD_SONG,
        MENU_SAVE_AND_EXIT,
        MENU_EXIT_WO_SAVE
    };
    
    DisplayManager& _display;
    UserInput& _userInput;
    OutputHandler& _outputHandler;
    TrackerApp* _trackerApp;
    
    int _selectedIndex;
    int _scrollOffset;
    bool _editing;
    int _editValue;
    
    // Navigation timing
    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;
    
    // Encoder tracking
    long _lastEncPosA;
    long _lastEncPosB;

    // Exit flag
    bool _exitRequested;
    
    // Slot selection (separate for save and load)
    int _saveSlot;
    int _loadSlot;
    
    // Warning state
    bool _showWarning;
    int _pendingSlot;
    bool _pendingIsSave;
    bool _pendingNewSong;      // NEW: flag for new song warning
    
    void handleNavigation();
    void handleEditing();
    void applySetting();
    void loadCurrentValue();
    void handleWarning();
    void drawWarning();
};

#endif