#ifndef SCALE_MENU_H
#define SCALE_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"
#include "Scales.h"

class ScaleMenu {
public:
    ScaleMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track);
    ~ScaleMenu();

    void enter();
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        ITEM_SCALE,
        ITEM_ROOT,
        ITEM_APPLY,
        ITEM_EXIT
    };

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;

    int _selectedIndex;
    uint8_t _selectedScaleIndex;
    uint8_t _selectedRootIndex;
    bool _exitRequested;

    void handleNavigation();
    void handleEditing();
    void apply();
    void loadCurrentValue();

    long _lastEncPosA;
    long _lastEncPosB;
    unsigned long _lastNavTime;
    bool _wasCentered;
};

#endif