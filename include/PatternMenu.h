#ifndef PATTERN_MENU_H
#define PATTERN_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongData.h"
#include "StepSequencer.h"
#include "SongSequencer.h"
#include "StepEditMenu.h"

class PatternMenu {
public:
    PatternMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer);
    ~PatternMenu();
    
    void enter(int track, int step, StepSequencer* sequencer);
    void update();
    void draw();
    void saveAndExit();   // called by TrackerApp on save button press
    
    // Exit control (mimics MainMenu)
    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }
    bool isInSubMenu() const { return _inStepEditMenu; }
    
private:
    enum MenuItem {
        MENU_TRANSPOSE,
        MENU_STEP_EDIT,
        MENU_DIVIDER,
        MENU_LENGTH,
        MENU_SAVE_EXIT,
        MENU_EXIT_NOSAVE
    };

    StepEditMenu* _stepEditMenu;
    bool _inStepEditMenu;

    void enterStepEdit();
    
    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
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