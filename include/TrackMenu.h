#ifndef TRACK_MENU_H
#define TRACK_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"
#include "QuantizerMainMenu.h"
#include "GenerateMenu.h"
#include "NotesMenu.h"

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
        MENU_QUANTIZER_SETTINGS,
        MENU_CV_RANGE_LOW,
        MENU_CV_RANGE_HIGH,
        MENU_SWING,
        MENU_RESET_ON_STEP,
        MENU_EXIT
    };

    enum SubMenuState {
        SUB_NONE,
        SUB_QUANTIZER_MAIN,
        SUB_GENERATE,
        SUB_NOTES
        // SUB_SCALE added later
    };

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;

    int _track;
    int _selectedIndex;
    int _editValue;
    bool _exitRequested;

    int _numItems;
    MenuItem _itemIndices[6];

    SubMenuState _subState;
    QuantizerMainMenu* _quantizerMenu;
    GenerateMenu* _generateMenu;
    NotesMenu* _notesMenu;

    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;
    long _lastEncPosA;
    long _lastEncPosB;

    void handleNavigation();
    void handleEditing();
    void applySetting();
    void loadCurrentValue();
    void buildItemList();

    void enterSubMenu(SubMenuState state);
    void exitSubMenu();
    void handleSubMenuUpdate();
    void drawSubMenu();
};

#endif