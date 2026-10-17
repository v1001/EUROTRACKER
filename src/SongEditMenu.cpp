#include "SongEditMenu.h"

SongEditMenu::SongEditMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _targetStep(0), _selectedIndex(0), _exitRequested(false), _lastNavTime(0),
      _lastJoystickMoveTime(0), _joystickWasCentered(true), _popupTime(0) {
    _popupText[0] = '\0';
}

SongEditMenu::~SongEditMenu() {}

void SongEditMenu::enter(int selectedStep) {
    _targetStep = selectedStep;
    _selectedIndex = 0;
    _exitRequested = false;
    _lastNavTime = 0;
    _lastJoystickMoveTime = 0;
    _joystickWasCentered = true;
    _popupText[0] = '\0';
}

void SongEditMenu::update() {
    handleNavigation();
    handleEditing();
}

void SongEditMenu::handleNavigation() {
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (_joystickWasCentered || (now - _lastJoystickMoveTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex < ITEM_COUNT) {
                _selectedIndex = newIndex;
                _lastJoystickMoveTime = now;
            }
            _joystickWasCentered = false;
        }
    } else {
        _joystickWasCentered = true;
    }
    _lastNavTime = now;
}

void SongEditMenu::handleEditing() {
    if (_userInput.joystick_button.just_released) {
        bool success = false;
        if (_selectedIndex == ITEM_INSERT) {
            success = _songSequencer.insertStep(_targetStep);
            showPopup(success ? "STEP INSERTED" : "SONG FULL");
        } else if (_selectedIndex == ITEM_DELETE) {
            if (_songSequencer.getPlayingStep() == _targetStep && _songSequencer.getSongState() != SongSequencer::STATE_STOP) {
                showPopup("CANNOT DEL PLAYING");
            } else {
                success = _songSequencer.deleteStep(_targetStep);
                showPopup(success ? "STEP DELETED" : "CANNOT DELETE");
            }
        } else if (_selectedIndex == ITEM_CLONE) {
            success = _songSequencer.cloneStep(_targetStep);
            showPopup(success ? "STEP CLONED" : "SONG FULL");
        } else if (_selectedIndex == ITEM_EXIT) {
            _exitRequested = true;
        }
    }

    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void SongEditMenu::showPopup(const char* text) {
    snprintf(_popupText, sizeof(_popupText), "%s", text);
    _popupTime = millis();
}

void SongEditMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    char header[24];
    snprintf(header, sizeof(header), "EDIT STEP %02d", _targetStep + 1);
    _display.printAt(header, 2, 0, ALIGN_LEFT);
    _display.drawLine(0, 10, 128, 10, _display.colorWhite());

    const char* items[] = {"Insert Step", "Delete Step", "Clone Step", "Exit"};

    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = 14 + i * 11;
        if (i == _selectedIndex) {
            _display.fillRect(0, y - 2, 128, 11, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
        }
    }

    if (_popupText[0] != '\0' && (millis() - _popupTime < 1000)) {
        const int BOX_W = 108;
        const int BOX_H = 16;
        const int BOX_X = (128 - BOX_W) / 2;
        const int BOX_Y = (64 - BOX_H) / 2;
        _display.fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, _display.colorWhite());
        _display.setTextColor(_display.colorBlack());
        _display.printCentered(_popupText, BOX_Y + 4);
        _display.setTextColor(_display.colorWhite());
    } else {
        _popupText[0] = '\0';
    }
}