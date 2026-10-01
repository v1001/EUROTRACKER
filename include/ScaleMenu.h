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
        ITEM_NOTES,
        ITEM_APPLY,
        ITEM_EXIT,
        MENU_ITEM_COUNT
    };

    static const int PITCH_COUNT = 12;
    static const int NOTE_ROW_X = 5;
    static const int NOTE_ROW_SPACING = 10;
    static const int NOTE_ROW_Y = 30;

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;

    int _selectedIndex;
    uint8_t _selectedScaleIndex;
    uint8_t _selectedRootIndex;
    int _noteCursor;
    bool _workingNotes[PITCH_COUNT];
    bool _exitRequested;

    void handleNavigation();
    void handleEditing();
    void apply();
    void loadCurrentValue();
    void loadMaskFromQuantizer();
    void rebuildMaskFromScale();
    void toggleCurrentNote();
    void drawNotesRow(int y);
    uint8_t getDisplayPitchClass(int displayPos) const;

    long _lastEncPosA;
    long _lastEncPosB;
    unsigned long _lastNavTime;
    bool _wasCentered;
};

#endif
