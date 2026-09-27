#include "PatternMenu.h"

PatternMenu::PatternMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _songData(songSequencer.getSongData()),
      _track(0), _step(0), _sequencer(nullptr), _selectedIndex(0),
      _editValue(0), _tempDivider(0), _tempLength(0), _tempTranspose(0),
      _exitRequested(false), _saveOnExit(true),
      _lastNavTime(0), _lastJoystickMoveTime(0),
      _joystickWasCentered(true), _lastEncPos(0) {
}

PatternMenu::~PatternMenu() {
}

void PatternMenu::enter(int track, int step, StepSequencer* sequencer) {
    _track = track;
    _step = step;
    _sequencer = sequencer;
    _selectedIndex = 0;
    _exitRequested = false;
    _saveOnExit = true;   // default to save if physical save button is pressed
    
    // Copy current values into temporary storage
    _tempDivider = _songData.getDividerIndex(_track, _step);
    _tempLength = _songData.getPattern(_track, _step).getNumSteps();
    _tempTranspose = 0;
    
    _lastNavTime = 0;
    _lastJoystickMoveTime = 0;
    _joystickWasCentered = true;
    _lastEncPos = _userInput.encoder_a.position;
    
    loadCurrentValue();
}

void PatternMenu::loadCurrentValue() {
    switch (_selectedIndex) {
        case MENU_DIVIDER:   _editValue = _tempDivider; break;
        case MENU_LENGTH:    _editValue = _tempLength; break;
        case MENU_TRANSPOSE: _editValue = _tempTranspose; break;
        default:             _editValue = 0; break;
    }
}

void PatternMenu::update() {
    handleNavigation();
    handleEditing();
    handleButtons();
}

void PatternMenu::handleNavigation() {
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;
    
    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;
    
    if (dy != 0) {
        if (_joystickWasCentered || (now - _lastJoystickMoveTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex <= MENU_EXIT_NOSAVE) {
                // Save current edited value into temp before leaving
                switch (_selectedIndex) {
                    case MENU_DIVIDER:   _tempDivider = _editValue; break;
                    case MENU_LENGTH:    _tempLength = _editValue; break;
                    case MENU_TRANSPOSE: _tempTranspose = _editValue; break;
                    default: break;
                }
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

void PatternMenu::handleEditing() {
    long encPos = _userInput.encoder_a.position;
    if (encPos == _lastEncPos) return;
    
    int delta = (encPos > _lastEncPos) ? 1 : -1;
    
    switch (_selectedIndex) {
        case MENU_DIVIDER:
            _editValue += delta;
            if (_editValue < 0) _editValue = 0;
            if (_editValue >= SongData::getNumDividers()) _editValue = SongData::getNumDividers() - 1;
            _tempDivider = _editValue;
            break;
            
        case MENU_LENGTH:
            if (_songSequencer.getSongState() != SongSequencer::STATE_STOP) break;
            _editValue += delta;
            if (_editValue < 1) _editValue = 1;
            if (_editValue > 32) _editValue = 32;
            _tempLength = _editValue;
            break;
            
        case MENU_TRANSPOSE:
            if (_track >= 4 || !_sequencer->isQuantizerEnabled()) {
                _editValue = 0;
                _tempTranspose = 0;
                break;
            }
            _editValue += delta;
            if (_editValue < -12) _editValue = -12;
            if (_editValue > 12) _editValue = 12;
            _tempTranspose = _editValue;
            break;
            
        default:
            break;
    }
    _lastEncPos = encPos;
}

void PatternMenu::handleButtons() {
    if (_userInput.joystick_button.just_released) {
        if (_selectedIndex == MENU_SAVE_EXIT) {
            applyChanges();
            _exitRequested = true;
        } else if (_selectedIndex == MENU_EXIT_NOSAVE) {
            _exitRequested = true;
        }
    }
}

void PatternMenu::saveAndExit() {
    // Called by TrackerApp when the physical save button is pressed.
    // Uses the current _saveOnExit flag (set by last highlighted exit item, or default true)
    applyChanges();
    _songSequencer.markProjectDirty();
}

void PatternMenu::applyChanges() {
    _songData.setDividerIndex(_track, _step, _tempDivider);
    _songData.getPattern(_track, _step).setNumSteps(_tempLength);
    
    if (_track < 4 && _sequencer->isQuantizerEnabled()) {
        Quantizer& quantizer = _songData.getQuantizer(_track);
        uint8_t numNotes = quantizer.getNumNotes();
        if (numNotes > 0) {
            StepPattern& pattern = _songData.getPattern(_track, _step);
            for (int s = 0; s < pattern.getNumSteps(); s++) {
                uint16_t cv = pattern.getCV(s);
                uint8_t bestIdx = 0;
                uint16_t bestDiff = 0xFFFF;
                for (uint8_t i = 0; i < numNotes; i++) {
                    uint16_t noteDac = quantizer.getNoteDAC(i);
                    uint16_t diff = (cv > noteDac) ? (cv - noteDac) : (noteDac - cv);
                    if (diff < bestDiff) {
                        bestDiff = diff;
                        bestIdx = i;
                    }
                }
                int newIdx = bestIdx + _tempTranspose;
                if (newIdx < 0) newIdx = 0;
                if (newIdx >= numNotes) newIdx = numNotes - 1;
                pattern.setCV(s, quantizer.getNoteDAC(newIdx));
            }
        }
    }
    
    if (_sequencer) {
        _sequencer->setClockDivision(_songData.getDividerValue(_track, _step));
    }
}

void PatternMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    
    const char* items[] = {"Divider", "Length", "Transpose", "Save && Exit", "Exit w/o Save"};
    char valueBuffer[16];
    
    for (int i = 0; i < 5; i++) {
        int y = i * 10;
        const char* value = "";
        
        if (i == _selectedIndex) {
            switch (i) {
                case MENU_DIVIDER:   value = SongData::getDividerTextByIndex(_editValue); break;
                case MENU_LENGTH:    sprintf(valueBuffer, "%d", _editValue); value = valueBuffer; break;
                case MENU_TRANSPOSE: sprintf(valueBuffer, "%+d", _editValue); value = valueBuffer; break;
                default: value = ""; break;
            }
        } else {
            switch (i) {
                case MENU_DIVIDER:   value = SongData::getDividerTextByIndex(_tempDivider); break;
                case MENU_LENGTH:    sprintf(valueBuffer, "%d", _tempLength); value = valueBuffer; break;
                case MENU_TRANSPOSE: sprintf(valueBuffer, "%+d", _tempTranspose); value = valueBuffer; break;
                default: value = ""; break;
            }
        }
        
        if (i == _selectedIndex) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) _display.printAt(value, 96, y, ALIGN_LEFT);
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) _display.printAt(value, 96, y, ALIGN_LEFT);
        }
    }
}