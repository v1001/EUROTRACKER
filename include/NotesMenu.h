#ifndef NOTES_MENU_H
#define NOTES_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class NotesMenu {
public:
    NotesMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track);
    ~NotesMenu();

    void enter();
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;

    int _selectedIndex;
    int _scrollOffset;
    bool _exitRequested;

    void handleNavigation();
    void handleEditing();

    long _lastEncPosA;
    long _lastEncPosB;
    unsigned long _lastNavTime;
    bool _wasCentered;
};

#endif