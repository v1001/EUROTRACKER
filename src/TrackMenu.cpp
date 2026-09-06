#include "TrackMenu.h"

TrackMenu::TrackMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _track(0), _selectedIndex(0), _editValue(0), _exitRequested(false), _lastEncPosB(0),
      _lastNavTime(0), _lastJoystickMoveTime(0), _joystickWasCentered(true), _lastEncPosA(0) {
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
    loadCurrentValue();
}

void TrackMenu::loadCurrentValue() {
    StepSequencer* seq = _songSequencer.getSequencer(_track);
    switch (_selectedIndex) {
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
            if (newIndex >= 0 && newIndex <= MENU_EXIT) {
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

        SongData& songData = _songSequencer.getSongData();

        switch (_selectedIndex) {
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
                // Toggle only, use encoder A
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    applySetting();
                }
                break;
            case MENU_SWING:
                // Use encoder A for coarse (10), encoder B for fine (1)
                if (encAChanged) _editValue += deltaA * 10;
                if (encBChanged) _editValue += deltaB;
                if (_editValue < 0) _editValue = 0;
                if (_editValue > 100) _editValue = 100;
                applySetting();
                break;
            case MENU_RESET_ON_STEP:
                // Toggle only, use encoder A
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    applySetting();
                }
                break;
            default:
                break;
        }

        _lastEncPosA = encPosA;
        _lastEncPosB = encPosB;
    }

    // Save button exits menu (unchanged)
    if (_userInput.save_button.just_released) {
        applySetting();
        _exitRequested = true;
    }
}

void TrackMenu::applySetting() {
    SongData& songData = _songSequencer.getSongData();
    StepSequencer* seq = _songSequencer.getSequencer(_track);

    switch (_selectedIndex) {
        case MENU_QUANTIZER_ENABLE:
            if (_track < 4) {
                bool enabled = (_editValue == 1);
                if (seq) seq->setQuantizerEnabled(enabled);
            }
            break;
        case MENU_CV_RANGE_LOW:
            if (seq && _track < 4) seq->setMinCV(_editValue);
            break;
        case MENU_CV_RANGE_HIGH:
            if (seq && _track < 4) seq->setMaxCV(_editValue);
            break;
        case MENU_SWING:
            if (seq) seq->setSwingAmount(_editValue);
            break;
        case MENU_RESET_ON_STEP:
            bool enabled = (_editValue == 1);
            if (seq) seq->setResetOnStep(enabled);
            break;
    }
}

void TrackMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    StepSequencer* seq = _songSequencer.getSequencer(_track);

    const char* items[] = {
        "Quantizer", "CV Low", "CV High", "Swing", "Reset Step", "Exit"
    };

    char valueBuffer[16];
    for (int i = 0; i <= MENU_EXIT; i++) {
        int y = i * 10;
        const char* value = "";

        if (i == _selectedIndex) {
            // Show edit buffer
            switch (i) {
                case MENU_QUANTIZER_ENABLE:
                    value = _editValue ? "ON" : "OFF";
                    break;
                case MENU_CV_RANGE_LOW:
                    sprintf(valueBuffer, "%d", _editValue);
                    value = valueBuffer;
                    break;
                case MENU_CV_RANGE_HIGH:
                    sprintf(valueBuffer, "%d", _editValue);
                    value = valueBuffer;
                    break;
                case MENU_SWING:
                    sprintf(valueBuffer, "%d%%", _editValue);
                    value = valueBuffer;
                    break;
                case MENU_RESET_ON_STEP:
                    value = _editValue ? "RESET" : "KEEP";
                    break;
                default:
                    value = "";
                    break;
            }
        } else {
            // Show saved value
            switch (i) {
                case MENU_QUANTIZER_ENABLE:
                    value = (_track < 4 && seq->isQuantizerEnabled()) ? "ON" : "OFF";
                    break;
                case MENU_CV_RANGE_LOW:
                    sprintf(valueBuffer, "%d", seq->getMinCV());
                    value = valueBuffer;
                    break;
                case MENU_CV_RANGE_HIGH:
                    sprintf(valueBuffer, "%d", seq->getMaxCV());
                    value = valueBuffer;
                    break;
                case MENU_SWING:
                    sprintf(valueBuffer, "%d%%", seq->getSwingAmount());
                    value = valueBuffer;
                    break;
                case MENU_RESET_ON_STEP:
                    value = seq->getResetOnStep() ? "RESET" : "KEEP";
                    break;
                default:
                    value = "";
                    break;
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

void TrackMenu::saveAndExit() {
    // Settings already applied live in applySetting()
    // The project will be saved by the auto-save mechanism
}