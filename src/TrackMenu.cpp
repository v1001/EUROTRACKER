#include "TrackMenu.h"

TrackMenu::TrackMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _track(0), _selectedIndex(0), _editValue(0), _exitRequested(false),
      _numItems(0), _lastNavTime(0), _lastJoystickMoveTime(0),
      _joystickWasCentered(true), _lastEncPosA(0), _lastEncPosB(0) {
}

TrackMenu::~TrackMenu() {
}

void TrackMenu::enter(int track) {
    _track = track;
    _selectedIndex = 0;
    _exitRequested = false;
    _lastNavTime = 0;
    _lastJoystickMoveTime = 0;
    _joystickWasCentered = true;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    buildItemList();
    loadCurrentValue();
}

void TrackMenu::buildItemList() {
    _numItems = 0;
    if (_track < 4) {
        // Melodic tracks: all items
        _itemIndices[_numItems++] = MENU_QUANTIZER_ENABLE;
        _itemIndices[_numItems++] = MENU_CV_RANGE_LOW;
        _itemIndices[_numItems++] = MENU_CV_RANGE_HIGH;
        _itemIndices[_numItems++] = MENU_SWING;
        _itemIndices[_numItems++] = MENU_RESET_ON_STEP;
        _itemIndices[_numItems++] = MENU_EXIT;
    } else {
        // Gate tracks: only Swing and Exit (Reset Step does not apply)
        _itemIndices[_numItems++] = MENU_SWING;
        _itemIndices[_numItems++] = MENU_EXIT;
    }
}

void TrackMenu::loadCurrentValue() {
    MenuItem item = _itemIndices[_selectedIndex];
    StepSequencer* seq = _songSequencer.getSequencer(_track);

    switch (item) {
        case MENU_QUANTIZER_ENABLE:
            _editValue = (_track < 4 && seq->isQuantizerEnabled()) ? 1 : 0;
            break;
        case MENU_CV_RANGE_LOW:
            _editValue = seq->getMinCV();
            break;
        case MENU_CV_RANGE_HIGH:
            _editValue = seq->getMaxCV();
            break;
        case MENU_SWING:
            _editValue = seq->getSwingAmount();
            break;
        case MENU_RESET_ON_STEP:
            _editValue = seq->getResetOnStep() ? 1 : 0;
            break;
        default:
            _editValue = 0;
            break;
    }
}

void TrackMenu::update() {
    handleNavigation();
    handleEditing();
}

void TrackMenu::handleNavigation() {
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (_joystickWasCentered || (now - _lastJoystickMoveTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex < _numItems) {
                applySetting();
                _selectedIndex = newIndex;
                loadCurrentValue();
                _lastJoystickMoveTime = now;
            }
            _joystickWasCentered = false;
        }
    } else {
        _joystickWasCentered = true;
    }
    _lastNavTime = now;
}

void TrackMenu::handleEditing() {
    long encPosA = _userInput.encoder_a.position;
    long encPosB = _userInput.encoder_b.position;
    bool encAChanged = (encPosA != _lastEncPosA);
    bool encBChanged = (encPosB != _lastEncPosB);

    StepSequencer* seq = _songSequencer.getSequencer(_track);

    if (encAChanged || encBChanged) {
        int deltaA = (encAChanged) ? ((encPosA > _lastEncPosA) ? 1 : -1) : 0;
        int deltaB = (encBChanged) ? ((encPosB > _lastEncPosB) ? 1 : -1) : 0;

        MenuItem item = _itemIndices[_selectedIndex];

        if (item != MENU_EXIT) {
            switch (item) {
                case MENU_CV_RANGE_LOW:
                    if (encAChanged) _editValue += deltaA * 50;
                    if (encBChanged) _editValue += deltaB;
                    if (_editValue < 0) _editValue = 0;
                    if (_editValue > seq->getMaxCV() - 100) _editValue = seq->getMaxCV() - 100;
                    applySetting();
                    break;
                case MENU_CV_RANGE_HIGH:
                    if (encAChanged) _editValue += deltaA * 50;
                    if (encBChanged) _editValue += deltaB;
                    if (_editValue > 4095) _editValue = 4095;
                    if (_editValue < seq->getMinCV() + 100) _editValue = seq->getMinCV() + 100;
                    applySetting();
                    break;
                case MENU_QUANTIZER_ENABLE:
                    if (encAChanged) {
                        _editValue = (_editValue + deltaA) % 2;
                        if (_editValue < 0) _editValue = 1;
                        applySetting();
                    }
                    break;
                case MENU_SWING:
                    if (encAChanged) _editValue += deltaA * 10;
                    if (encBChanged) _editValue += deltaB;
                    if (_editValue < 0) _editValue = 0;
                    if (_editValue > 100) _editValue = 100;
                    applySetting();
                    break;
                case MENU_RESET_ON_STEP:
                    if (encAChanged) {
                        _editValue = (_editValue + deltaA) % 2;
                        if (_editValue < 0) _editValue = 1;
                        applySetting();
                    }
                    break;
                default:
                    break;
            }
        }

        _lastEncPosA = encPosA;
        _lastEncPosB = encPosB;
    }

    if (_userInput.joystick_button.just_released) {
        MenuItem item = _itemIndices[_selectedIndex];
        if (item == MENU_EXIT) {
            applySetting();
            _exitRequested = true;
        }
    }

    if (_userInput.save_button.just_released) {
        applySetting();
        _exitRequested = true;
    }
}

void TrackMenu::applySetting() {
    MenuItem item = _itemIndices[_selectedIndex];
    StepSequencer* seq = _songSequencer.getSequencer(_track);

    switch (item) {
        case MENU_QUANTIZER_ENABLE: {
            if (_track < 4) {
                bool enabled = (_editValue == 1);
                if (seq) seq->setQuantizerEnabled(enabled);
            }
            break;
        }
        case MENU_CV_RANGE_LOW: {
            if (_track < 4 && seq) seq->setMinCV(_editValue);
            break;
        }
        case MENU_CV_RANGE_HIGH: {
            if (_track < 4 && seq) seq->setMaxCV(_editValue);
            break;
        }
        case MENU_SWING: {
            if (seq) seq->setSwingAmount(_editValue);
            break;
        }
        case MENU_RESET_ON_STEP: {
            bool enabled = (_editValue == 1);
            if (seq) seq->setResetOnStep(enabled);
            break;
        }
        default:
            break;
    }
}

void TrackMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    StepSequencer* seq = _songSequencer.getSequencer(_track);

    char valueBuffer[16];
    for (int i = 0; i < _numItems; i++) {
        MenuItem item = _itemIndices[i];
        int y = i * 10;
        const char* itemName = "";
        const char* value = "";

        switch (item) {
            case MENU_QUANTIZER_ENABLE: itemName = "Quantizer"; break;
            case MENU_CV_RANGE_LOW:    itemName = "CV Low";    break;
            case MENU_CV_RANGE_HIGH:   itemName = "CV High";   break;
            case MENU_SWING:           itemName = "Swing";     break;
            case MENU_RESET_ON_STEP:   itemName = "Reset Step"; break;
            case MENU_EXIT:            itemName = "Exit";      break;
        }

        bool selected = (i == _selectedIndex);

        if (selected) {
            if (item != MENU_EXIT) {
                switch (item) {
                    case MENU_QUANTIZER_ENABLE: value = _editValue ? "ON" : "OFF"; break;
                    case MENU_CV_RANGE_LOW:     sprintf(valueBuffer, "%d", _editValue); value = valueBuffer; break;
                    case MENU_CV_RANGE_HIGH:    sprintf(valueBuffer, "%d", _editValue); value = valueBuffer; break;
                    case MENU_SWING:            sprintf(valueBuffer, "%d%%", _editValue); value = valueBuffer; break;
                    case MENU_RESET_ON_STEP:    value = _editValue ? "RESET" : "KEEP"; break;
                    default: break;
                }
            }
        } else {
            if (item != MENU_EXIT) {
                switch (item) {
                    case MENU_QUANTIZER_ENABLE: value = (_track < 4 && seq->isQuantizerEnabled()) ? "ON" : "OFF"; break;
                    case MENU_CV_RANGE_LOW:     sprintf(valueBuffer, "%d", seq->getMinCV()); value = valueBuffer; break;
                    case MENU_CV_RANGE_HIGH:    sprintf(valueBuffer, "%d", seq->getMaxCV()); value = valueBuffer; break;
                    case MENU_SWING:            sprintf(valueBuffer, "%d%%", seq->getSwingAmount()); value = valueBuffer; break;
                    case MENU_RESET_ON_STEP:    value = seq->getResetOnStep() ? "RESET" : "KEEP"; break;
                    default: break;
                }
            }
        }

        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(itemName, 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(itemName, 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
        }
    }
}

void TrackMenu::saveAndExit() {
    applySetting();
    _exitRequested = true;
}