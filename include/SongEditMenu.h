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
        ITEM_EXIT,
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

    char _popupText[32];
    unsigned long _popupTime;

    void handleNavigation();
    void handleEditing();
    void showPopup(const char* text);
};

#endif