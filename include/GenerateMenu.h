#ifndef GENERATE_MENU_H
#define GENERATE_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class GenerateMenu {
public:
    GenerateMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track);
    ~GenerateMenu();

    void enter();
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        ITEM_MIN_CV,
        ITEM_MAX_CV,
        ITEM_NUM_NOTES,
        ITEM_GENERATE,
        ITEM_EXIT
    };

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;

    int _selectedIndex;
    uint16_t _minCV;
    uint16_t _maxCV;
    uint8_t _numNotes;
    bool _exitRequested;

    void handleNavigation();
    void handleEditing();
    void loadCurrentValue();
    void generate();

    long _lastEncPosA;
    long _lastEncPosB;
    unsigned long _lastNavTime;
    bool _wasCentered;
};

#endif