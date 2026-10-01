#ifndef STEP_EDIT_MENU_H
#define STEP_EDIT_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class StepEditMenu {
public:
    StepEditMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer);
    ~StepEditMenu();

    void enter(int track, int songStep);
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    static const int ROW_COUNT = 4;

    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;
    int _songStep;
    StepPattern* _pattern;

    int _selectedStep;
    int _selectedRow;

    bool _exitRequested;
    unsigned long _lastNavTime;
    bool _wasCentered;

    long _lastEncPosA;
    long _lastEncPosB;

    bool _popupActive;
    char _popupText[24];
    unsigned long _popupTime;

    void handleNavigation();
    void handleEncoders();
    void handleButtons();
    void navigateStep(int delta);
    void navigateRow(int delta);
    void editCV(int delta);
    void editGateLength(int delta);
    void editProbability(int delta);
    void editAttack(int delta);
    void editDecay(int delta);
    void editRatchet(int delta);
    void editMicrotiming(int delta);
    void markDirty();
    void showPopup(const char* text);

    const char* getCVLabel() const;
    const char* getCVDisplay(char* buffer, size_t bufSize);
    int getRowY(int rowIndex) const;

    void drawRow(int rowIndex);
    void drawStatusBar();
    void drawPopup();
};

#endif