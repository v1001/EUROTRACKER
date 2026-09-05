#ifndef TRACK_MENU_H
#define TRACK_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class TrackMenu {
public:
    TrackMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer);
    ~TrackMenu();

    void enter(int track);
    void update();
    void draw();
    void saveAndExit();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        MENU_QUANTIZER_ENABLE,
        MENU_CV_RANGE_LOW,
        MENU_CV_RANGE_HIGH,
        MENU_SWING,
        MENU_RESET_ON_STEP,
        MENU_EXIT
    };

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;

    int _track;
    int _selectedIndex;
    int _editValue;
    bool _exitRequested;

    // Navigation timing
    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;
    long _lastEncPosA;
    long _lastEncPosB;

    void handleNavigation();
    void handleEditing();
    void applySetting();
    void loadCurrentValue();
};

#endif