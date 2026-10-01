#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "OutputHandler.h"
#include "GlobalSettings.h"

// Forward declaration
class TrackerApp;

class MainMenu {
public:
    MainMenu(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler, TrackerApp* trackerApp);
    ~MainMenu();

    void enter();
    void update();
    void draw();
    void saveAndExit();

    bool shouldExit() const { return _exitRequested; }
    void clearExitFlag() { _exitRequested = false; }

private:
    enum MenuItem {
        MENU_CLOCK_SOURCE,
        MENU_INTERNAL_BPM,
        MENU_CLOCK_IO,
        MENU_AUTOPLAY,
        MENU_SYNC_START,
        MENU_SONG_LENGTH,
        MENU_NEW_SONG,
        MENU_SAVE_SONG,
        MENU_LOAD_SONG,
        MENU_SAVE_AND_EXIT,
        MENU_EXIT_WO_SAVE,
        MENU_ITEM_COUNT
    };

    static const int MAX_VISIBLE = MENU_ITEM_COUNT;
    static const int VISIBLE_ROWS = 6;

    DisplayManager& _display;
    UserInput& _userInput;
    OutputHandler& _outputHandler;
    TrackerApp* _trackerApp;

    MenuItem _visibleItems[MAX_VISIBLE];
    int _visibleCount;

    int _selectedIndex;    // position in _visibleItems
    int _scrollOffset;
    bool _editing;
    int _editValue;

    unsigned long _lastNavTime;
    unsigned long _lastJoystickMoveTime;
    bool _joystickWasCentered;

    long _lastEncPosA;
    long _lastEncPosB;

    bool _exitRequested;

    int _saveSlot;
    int _loadSlot;

    bool _showWarning;
    int _pendingSlot;
    bool _pendingIsSave;
    bool _pendingNewSong;

    void rebuildVisibleItems();
    void clampSelectionToVisible();
    MenuItem currentItem() const;
    const char* itemLabel(MenuItem item) const;

    void handleNavigation();
    void handleEditing();
    void applySetting();
    void loadCurrentValue();
    void handleWarning();
    void drawWarning();
};

#endif