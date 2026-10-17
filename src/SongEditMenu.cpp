#include "SongEditMenu.h"

SongEditMenu::SongEditMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _targetStep(0), _selectedIndex(0), _exitRequested(false), _lastNavTime(0),
      _lastJoystickMoveTime(0), _joystickWasCentered(true), 
      _lastEncPosA(0), _lastEncPosB(0),
      _loopTargetStep(-1), _loopCount(1), _popupTime(0) {
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

    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;

    // Load current loop settings from the song data
    SongData::SongStep* step = _songSequencer.getSongData().getStep(_targetStep);
    if (step && step->loopTarget != nullptr) {
        _loopTargetStep = _songSequencer.getSongData().getStepIndex(step->loopTarget);
        _loopCount = step->loopCount;
    } else {
        _loopTargetStep = -1;
        _loopCount = 1; // Default internal value (displays as 2)
    }
}

void SongEditMenu::update() {
    handleNavigation();
    handleEncoders();
    handleButtons();
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

void SongEditMenu::handleEncoders() {
    long encA = _userInput.encoder_a.position;
    long encB = _userInput.encoder_b.position;

    bool encAChanged = (encA != _lastEncPosA);
    bool encBChanged = (encB != _lastEncPosB);
    if (!encAChanged && !encBChanged) return;

    int deltaA = encAChanged ? ((encA > _lastEncPosA) ? 1 : -1) : 0;
    int deltaB = encBChanged ? ((encB > _lastEncPosB) ? 1 : -1) : 0;
    int delta = deltaA + deltaB;

    if (delta != 0) {
        if (_selectedIndex == ITEM_LOOP_TARGET) {
            _loopTargetStep += delta;
            // Clamp: -1 (None) up to the current step index (self-loop)
            if (_loopTargetStep < -1) _loopTargetStep = _targetStep;
            if (_loopTargetStep > _targetStep) _loopTargetStep = -1;
            
            applyLoopSettings(); // Auto-save on change
            
        } 
        else if (_selectedIndex == ITEM_LOOP_COUNT) {
            if (_loopTargetStep != -1) {
                _loopCount += delta;
                if (_loopCount < 1) _loopCount = 7;
                if (_loopCount > 7) _loopCount = 1;
                
                applyLoopSettings(); // Auto-save on change
            }
        }
    }

    _lastEncPosA = encA;
    _lastEncPosB = encB;
}

void SongEditMenu::applyLoopSettings() {
    if (_loopTargetStep == -1) {
        _songSequencer.clearStepLoop(_targetStep);
        showPopup("LOOP CLEARED");
    } else {
        if (_songSequencer.setStepLoop(_targetStep, _loopTargetStep, _loopCount)) {
            // Successfully set. Don't show a popup so it doesn't block the UI while turning.
            _popupText[0] = '\0'; 
        } else {
            // Find overlap to give a precise error message
            int overlap = -1;
            for (int i = _loopTargetStep; i < _targetStep; i++) {
                if (_songSequencer.getSongData().getStep(i)->loopTarget != nullptr) {
                    overlap = i;
                    break;
                }
            }
            
            if (overlap != -1) {
                char buf[32];
                snprintf(buf, sizeof(buf), "OVERLAP @ %02d", overlap + 1);
                showPopup(buf);
            } else {
                showPopup("INVALID LOOP");
            }
        }
    }
}

void SongEditMenu::handleButtons() {
    if (_userInput.joystick_button.just_released) {
        bool success = false;
        
        if (_selectedIndex == ITEM_INSERT) {
            success = _songSequencer.insertStep(_targetStep);
            showPopup(success ? "STEP INSERTED" : "SONG FULL");
        } 
        else if (_selectedIndex == ITEM_DELETE) {
            if (_songSequencer.getPlayingStep() == _targetStep && _songSequencer.getSongState() != SongSequencer::STATE_STOP) {
                showPopup("CANNOT DEL PLAYING");
            } else {
                success = _songSequencer.deleteStep(_targetStep);
                showPopup(success ? "STEP DELETED" : "CANNOT DELETE");
            }
        } 
        else if (_selectedIndex == ITEM_CLONE) {
            success = _songSequencer.cloneStep(_targetStep);
            showPopup(success ? "STEP CLONED" : "SONG FULL");
        } 
    }

    // Save button is now the only way to exit
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

    // Header
    char header[24];
    snprintf(header, sizeof(header), "EDIT STEP %02d", _targetStep + 1);
    _display.printAt(header, 2, 0, ALIGN_LEFT);
    _display.drawLine(0, 10, 128, 10, _display.colorWhite());

    const char* items[] = {
        "Insert Step", 
        "Delete Step", 
        "Clone Step", 
        "Loop Target", 
        "Loop Count"
    };

    // Format dynamic values
    char loopTargetStr[16];
    if (_loopTargetStep == -1) {
        snprintf(loopTargetStr, sizeof(loopTargetStr), "None");
    } else {
        snprintf(loopTargetStr, sizeof(loopTargetStr), "%02d", _loopTargetStep + 1);
    }

    char loopCountStr[16];
    if (_loopTargetStep == -1) {
        snprintf(loopCountStr, sizeof(loopCountStr), "--");
    } else {
        snprintf(loopCountStr, sizeof(loopCountStr), "%d", _loopCount + 1); // +1 for intuitive display
    }

    const char* values[] = {
        "", 
        "", 
        "", 
        loopTargetStr, 
        loopCountStr
    };

    // Draw 5 items (spacing of 10 pixels)
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = 14 + i * 10;
        bool selected = (i == _selectedIndex);

        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(values[i]) > 0) {
                _display.printAt(values[i], 96, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(values[i]) > 0) {
                _display.printAt(values[i], 96, y, ALIGN_LEFT);
            }
        }
    }

    // Draw Popup if active
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