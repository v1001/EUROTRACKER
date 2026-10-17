#ifndef SONG_EDIT_MENU_H
#define SONG_EDIT_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class SongEditMenu {
public:
    SongEditMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer);
    ~SongEditMenu();

    void enter(int selectedStep);
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        ITEM_INSERT,
        ITEM_DELETE,
        ITEM_CLONE,
        ITEM_LOOP_TARGET,
        ITEM_LOOP_COUNT,
        ITEM_COUNT
    };

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _targetStep;

    int _selectedIndex;
    bool _exitRequested;
    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;

    long _lastEncPosA;
    long _lastEncPosB;

    // Loop settings being edited
    int _loopTargetStep; // -1 means None
    int _loopCount;      // 1 to 7 internally (displays as 2 to 8)

    char _popupText[32];
    unsigned long _popupTime;

    void handleNavigation();
    void handleEncoders();
    void handleButtons();
    void applyLoopSettings(); // New helper
    void showPopup(const char* text);
};

#endif