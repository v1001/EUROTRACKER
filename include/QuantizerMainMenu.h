#ifndef QUANTIZER_MAIN_MENU_H
#define QUANTIZER_MAIN_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "SongSequencer.h"

class QuantizerMainMenu {
public:
    QuantizerMainMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track);
    ~QuantizerMainMenu();

    void enter();
    void update();
    void draw();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

    // Indicates that the user wants to open the Generate submenu
    bool shouldOpenGenerate() const { return _openGenerate; }
    void clearOpenGenerateFlag() { _openGenerate = false; }
    
    bool shouldOpenNotes() const { return _openNotes; }
    void clearOpenNotesFlag() { _openNotes = false; }

    bool shouldOpenScale() const { return _openScale; }
    void clearOpenScaleFlag() { _openScale = false; }

private:
    enum MenuItem {
        ITEM_ENABLE,
        ITEM_GENERATE,
        ITEM_SCALE,
        ITEM_NOTES,
        ITEM_EXIT
    };



    DisplayManager& _display;
    UserInput& _userInput;
    SongSequencer& _songSequencer;
    int _track;

    int _selectedIndex;
    int _editValue;      // 0 or 1 for enable toggle
    bool _exitRequested;
    bool _openGenerate;
    bool _openNotes;
    bool _openScale;

    void handleNavigation();
    void handleEditing();
    void applySetting();
    void loadCurrentValue();
};

#endif