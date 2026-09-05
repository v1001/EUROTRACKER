#ifndef PATTERN_MENU_H
#define PATTERN_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongData.h"
#include "StepSequencer.h"

class PatternMenu {
public:
    PatternMenu(DisplayManager& display, UserInput& userInput, SongData& songData);
    ~PatternMenu();
    
    void enter(int track, int step, StepSequencer* sequencer);
    void update();
    void draw();
    void saveAndExit();   // called by TrackerApp on save button press
    
    // Exit control (mimics MainMenu)
    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }
    
private:
    enum MenuItem {
        MENU_DIVIDER,
        MENU_LENGTH,
        MENU_TRANSPOSE,
        MENU_SAVE_EXIT,
        MENU_EXIT_NOSAVE
    };
    
    DisplayManager& _display;
    UserInput& _userInput;
    SongData& _songData;
    
    int _track;
    int _step;
    StepSequencer* _sequencer;
    
    int _selectedIndex;
    int _editValue;          // temporary value for the currently selected item
    
    // Temporary storage for all editable parameters
    int _tempDivider;
    int _tempLength;
    int _tempTranspose;
    
    // Exit state
    bool _exitRequested;
    bool _saveOnExit;        // true = save changes on exit, false = discard
    
    // Navigation timing
    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;
    
    // Encoder tracking
    long _lastEncPos;
    
    void handleNavigation();
    void handleEditing();
    void handleButtons();
    void applyChanges();      // writes temp values to SongData
    void loadCurrentValue();  // loads _editValue from temp based on selection
};

#endif