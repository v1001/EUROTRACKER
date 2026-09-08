#include "QuantizerMainMenu.h"

QuantizerMainMenu::QuantizerMainMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _editValue(0), _exitRequested(false), _openGenerate(false), _openNotes(false) {
}

QuantizerMainMenu::~QuantizerMainMenu() {
}

void QuantizerMainMenu::enter() {
    _selectedIndex = 0;
    _exitRequested = false;
    _openGenerate = false;
    _openNotes = false;
    loadCurrentValue();
}

void QuantizerMainMenu::loadCurrentValue() {
    StepSequencer* seq = _songSequencer.getSequencer(_track);
    _editValue = seq->isQuantizerEnabled() ? 1 : 0;
}

void QuantizerMainMenu::update() {
    handleNavigation();
    handleEditing();
}

void QuantizerMainMenu::handleNavigation() {
    unsigned long now = millis();
    static unsigned long lastNavTime = 0;
    static bool wasCentered = true;
    if (now - lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (wasCentered || (now - lastNavTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex <= ITEM_EXIT) {
                applySetting();
                _selectedIndex = newIndex;
                loadCurrentValue();
                lastNavTime = now;
            }
            wasCentered = false;
        }
    } else {
        wasCentered = true;
    }
    lastNavTime = now;
}

void QuantizerMainMenu::handleEditing() {
    // Joystick click
    if (_userInput.joystick_button.just_released) {
        switch (_selectedIndex) {
            case ITEM_ENABLE:
                _editValue = (_editValue == 0) ? 1 : 0;
                applySetting();
                break;
            case ITEM_GENERATE:
                _openGenerate = true;
                _exitRequested = true;  // exit main menu to go to generate submenu
                break;
            case ITEM_NOTES:
                _openNotes = true;
                _exitRequested = true;
                break;
            case ITEM_EXIT:
                _exitRequested = true;
                break;
        }
    }

    // Save button: exit and save
    if (_userInput.save_button.just_released) {
        applySetting();
        _exitRequested = true;
    }
}

void QuantizerMainMenu::applySetting() {
    StepSequencer* seq = _songSequencer.getSequencer(_track);
    if (_selectedIndex == ITEM_ENABLE) {
        seq->setQuantizerEnabled(_editValue == 1);
    }
}

void QuantizerMainMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    const char* items[] = {"Enable", "Generate", "Notes", "Exit"};
    char valueBuffer[8];

    for (int i = 0; i <= ITEM_EXIT; i++) {
        int y = i * 10;
        const char* value = "";

        if (i == _selectedIndex) {
            // Show edit buffer for Enable
            if (i == ITEM_ENABLE) {
                value = _editValue ? "ON" : "OFF";
            }
        } else {
            // Show saved value for Enable
            if (i == ITEM_ENABLE) {
                StepSequencer* seq = _songSequencer.getSequencer(_track);
                value = seq->isQuantizerEnabled() ? "ON" : "OFF";
            }
        }

        if (i == _selectedIndex) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
        }
    }
}