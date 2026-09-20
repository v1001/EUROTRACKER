#include "StepSequencerUI.h"
#include "StepSequencer.h"  // to use getMinCV/getMaxCV

StepSequencerUI::StepSequencerUI(DisplayManager& display, UserInput& userInput)
    : _display(display), _userInput(userInput), _pattern(nullptr), _sequencer(nullptr),
      _selectedStep(0), _currentStep(0), _displayMode(0), _editModeEncA(0), _editModeEncB(0),
      _cursorVisible(true),
      _lastCursorBlink(0), _lastMoveTime(0), _lastNavTime(0), _lastJoystickMoveTime(0),
      _joystickWasCentered(true), _lastEncoderAPos(0), _lastEncoderBPos(0),
      _lastEncoderAMove(0), _lastEncoderBMove(0),
      _quantizer(nullptr), _quantizerEnabled(false),
      _copiedStep{false, 0, 0, 0, 0, 0, 0, 0, false}, _copyTriggered(false) {
}

StepSequencerUI::~StepSequencerUI() {
}

void StepSequencerUI::begin(StepPattern* pattern, StepSequencer* sequencer) {
    _pattern = pattern;
    _sequencer = sequencer;
}

void StepSequencerUI::update() {
    handleJoystick();
    handleEncoders();
    handleButtons();
    blinkCursor();
}

void StepSequencerUI::draw() {
    if (!_pattern) return;
    
    uint8_t numSteps = _pattern->getNumSteps();
    for (uint8_t i = 0; i < numSteps; i++) {
        int x, y;
        getStepPosition(i, x, y);
        drawStep(i, x, y);
    }
    drawStatusBar();
}

void StepSequencerUI::getStepPosition(uint8_t step, int& x, int& y) {
    uint8_t col = step % COLS;
    uint8_t row = step / COLS;
    const int GRID_START_X = (128 - (CELL_SPACING_X * (COLS - 1) + RECT_WIDTH)) / 2;
    const int GRID_START_Y = 4;
    x = GRID_START_X + col * CELL_SPACING_X;
    y = GRID_START_Y + row * CELL_SPACING_Y;
}

void StepSequencerUI::drawStep(uint8_t step, int x, int y) {
    if (_pattern->getOn(step)) {
        _display.fillRect(x, y, RECT_WIDTH, RECT_HEIGHT, _display.colorWhite());
        
        int dashX = x;
        if (_pattern->getProbability(step) < 99) _display.drawPixel(dashX, y + 1, _display.colorBlack());
        if (_pattern->getProbability(step) < 79) _display.drawPixel(dashX, y + 3, _display.colorBlack());
        if (_pattern->getProbability(step) < 59) _display.drawPixel(dashX, y + 5, _display.colorBlack());
        if (_pattern->getProbability(step) < 39) _display.drawPixel(dashX, y + 7, _display.colorBlack());
        
        _display.setTextSize(TEXT_SMALL);
        char valStr[6];   // enough for "99%" or note name like "C#4"
        
        switch(_displayMode) {
            case 0: { // CV mode
                uint16_t raw = _pattern->getCV(step);
                const char* displayText = getQuantizedNoteNameOrCV(raw);
                strcpy(valStr, displayText);
                break;
            }
            case 2: sprintf(valStr, "%02d", _pattern->getDecay(step)); break;
            case 1: sprintf(valStr, "%02d", _pattern->getProbability(step)); break;
            case 3: sprintf(valStr, "%02d", _pattern->getGateLength(step)); break;
            case 4: sprintf(valStr, "%02d", _pattern->getAttack(step)); break;
            case 5: sprintf(valStr, "%02d", _pattern->getRatchet(step)); break;
            case 7: sprintf(valStr, "%02d", _pattern->getMicrotiming(step)); break;
            default: sprintf(valStr, "%02d", 0);
        }

        int textX = x + (RECT_WIDTH / 2) - 5;
        int textY = y + (RECT_HEIGHT / 2) - 4;
        _display.setTextColor(_display.colorBlack());
        _display.printAt(valStr, textX, textY, ALIGN_LEFT);

        if (_pattern->getDecay(step) > 0) {
            int triangleX = x + 2;
            int triangleY = y + RECT_HEIGHT - 1;
            _display.drawPixel(triangleX, triangleY, _display.colorBlack());
            _display.drawPixel(triangleX, triangleY - 1, _display.colorBlack());
            _display.drawPixel(triangleX + 1, triangleY, _display.colorBlack());
        }
        
        if (_pattern->getAttack(step) > 0) {
            int triangleX = x + 1;
            int triangleY = y + RECT_HEIGHT - 1;
            _display.drawPixel(triangleX, triangleY, _display.colorBlack());
            _display.drawPixel(triangleX + 1, triangleY - 1, _display.colorBlack());
            _display.drawPixel(triangleX + 1, triangleY, _display.colorBlack());
        }
        
        // Ratchet indicator
        uint8_t ratchet = _pattern->getRatchet(step);
        if (ratchet > 1 && ratchet <= 4) {
            int dotX = x + RECT_WIDTH - 8;
            int dotY = y + RECT_HEIGHT - 1;
            for (uint8_t i = 0; i < ratchet; i++) {
                _display.drawPixel(dotX + 2 * i, dotY, _display.colorBlack());
            }
        }
    } else {
        int smallRectW = 6;
        int smallRectH = 4;
        int smallRectX = x + (RECT_WIDTH / 2) - (smallRectW / 2);
        int smallRectY = y + (RECT_HEIGHT / 2) - (smallRectH / 2);
        _display.drawRect(smallRectX, smallRectY, smallRectW, smallRectH, _display.colorWhite());
    }
    
    // Current step marker
    if (_currentStep == step) {
        int frameX = x - 2;
        int frameY = y - 2;
        int frameW = RECT_WIDTH + 4;
        int frameH = RECT_HEIGHT + 4;
        _display.drawRect(frameX, frameY, frameW, frameH, _display.colorWhite());
    }
    
    // Selected step cursor
    if (_selectedStep == step && _cursorVisible) {
        _display.drawRect(x - 1, y - 1, RECT_WIDTH + 2, RECT_HEIGHT + 2, _display.colorWhite());
    }
}

void StepSequencerUI::drawStatusBar() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    
    char encAText[8];
    if (_editModeEncA == 0) {
        if(_quantizerEnabled){
            sprintf(encAText, "A:NOTE");
        }else{
            sprintf(encAText, "A:CV");
        }
    } else if (_editModeEncA == 1) {
        sprintf(encAText, "A:DECY");
    } else {
        sprintf(encAText, "A:ATCK");
    }
    _display.printAt(encAText, 50, STATUS_BAR_Y, ALIGN_LEFT);
    
    char encBText[8];
    if (_editModeEncB == 0) {
        sprintf(encBText, "B:PROB");
    } else if (_editModeEncB == 1) {
        sprintf(encBText, "B:GATE");
    } else if (_editModeEncB == 2){
        sprintf(encBText, "B:RCHT");
    } else if (_editModeEncB == 3){
        sprintf(encBText, "B:MCRT");
    }
    _display.printAt(encBText, 90, STATUS_BAR_Y, ALIGN_LEFT);
}

void StepSequencerUI::handleJoystick() {
    uint64_t now = millis();
    if (now - _lastMoveTime < MOVE_DELAY) return;
    
    int dx = 0, dy = 0;
    if (_userInput.joystick.x_position > 30) dx = 1;
    else if (_userInput.joystick.x_position < -30) dx = -1;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;
    
    if (dx != 0 || dy != 0) {
        uint8_t numSteps = _pattern->getNumSteps();
        uint8_t row = _selectedStep / COLS;
        uint8_t col = _selectedStep % COLS;
        int newRow = row + dy;
        int newCol = col + dx;
        
        if (newRow >= 0 && newRow < ROWS && newCol >= 0 && newCol < COLS) {
            uint8_t newStep = newRow * COLS + newCol;
            if (newStep < numSteps) {
                _selectedStep = newStep;
                _lastMoveTime = now;
                _cursorVisible = true;
                _lastCursorBlink = now;
            }
        }
    }
}

void StepSequencerUI::handleEncoders() {
    uint64_t now = millis();
    
    long encAPos = _userInput.encoder_a.position;
    if (encAPos != _lastEncoderAPos) {
        int change = (encAPos > _lastEncoderAPos) ? 1 : -1;
        
        if (change != 0) {
            if (_editModeEncA == 0) { // CV editing
            
                uint16_t raw = _pattern->getCV(_selectedStep);
                uint16_t minCV = _sequencer->getMinCV();
                uint16_t maxCV = _sequencer->getMaxCV();

                // Clamp
                uint16_t clamped = raw;
                if (clamped < minCV) clamped = minCV;
                if (clamped > maxCV) clamped = maxCV;
                // Check for valid note index
                uint8_t currentIdx = 0xFF;
                if (_quantizerEnabled && _quantizer) {
                    currentIdx = _quantizer->getNoteIndex(clamped);
                }
                if (currentIdx != 0xFF) {
                    // Reachable bounds
                    uint8_t qMinIdx = _quantizer->getNoteIndex(minCV);
                    uint8_t qMaxIdx = _quantizer->getNoteIndex(maxCV);
                    // Both are valid since currentIdx != 0xFF
                    int newIdx = currentIdx;
                    if (change > 0) {
                        for (int i = currentIdx + 1; i <= (int)qMaxIdx; i++) {
                            if (_quantizer->getNote(i).inScale) { newIdx = i; break; }
                        }
                    } else if (change < 0) {
                        for (int i = currentIdx - 1; i >= (int)qMinIdx; i--) {
                            if (_quantizer->getNote(i).inScale) { newIdx = i; break; }
                        }
                    }
                    _pattern->setCV(_selectedStep, _quantizer->getNoteDAC((uint8_t)newIdx));
                } else {
                    // Unquantized
                    uint16_t step = (maxCV - minCV) / 100;
                    if (step == 0) step = 1;
                    int newRaw = raw + change * step;
                    if (newRaw > (int)maxCV) newRaw = maxCV;
                    if (newRaw < (int)minCV) newRaw = minCV;
                    _pattern->setCV(_selectedStep, (uint16_t)newRaw);
                }
                _displayMode = 0;
            } else if (_editModeEncA == 1) {
                int newValue = _pattern->getDecay(_selectedStep) + change;
                _pattern->setDecay(_selectedStep, constrainValue(newValue));
                _displayMode = 2;
            } else if (_editModeEncA == 2) {
                int newValue = _pattern->getAttack(_selectedStep) + change;
                _pattern->setAttack(_selectedStep, constrainValue(newValue));
                _displayMode = 4;
            }
        }
        _lastEncoderAPos = encAPos;
        _lastEncoderAMove = now;
    }
    
    long encBPos = _userInput.encoder_b.position;
    if (encBPos != _lastEncoderBPos) {
        int change = (encBPos > _lastEncoderBPos) ? 1 : -1;
        
        if (change != 0) {
            if (_editModeEncB == 0) {
                int newValue = _pattern->getProbability(_selectedStep) + change;
                _pattern->setProbability(_selectedStep, constrainValue(newValue));
                _displayMode = 1;
            } else if (_editModeEncB == 1) {
                int newValue = _pattern->getGateLength(_selectedStep) + change;
                _pattern->setGateLength(_selectedStep, constrainValue(newValue));
                _displayMode = 3;
            } else if (_editModeEncB == 2) {
                int newValue = _pattern->getRatchet(_selectedStep) + change;
                _pattern->setRatchet(_selectedStep, constrainValue(newValue));
                _displayMode = 5;
            } else if (_editModeEncB == 3) {
                int newValue = _pattern->getMicrotiming(_selectedStep) + change;
                _pattern->setMicrotiming(_selectedStep, constrainValue(newValue));
                _displayMode = 7;
            }
        }
        _lastEncoderBPos = encBPos;
        _lastEncoderBMove = now;
    }
}

void StepSequencerUI::handleButtons() {
    // ---- COPY: hold Encoder B > 500ms ----
    if (_userInput.encoder_b_button.press_duration > 500 && !_copyTriggered) {
        // Copy current selected step data
        _copiedStep.on = _pattern->getOn(_selectedStep);
        _copiedStep.cv = _pattern->getCV(_selectedStep);
        _copiedStep.probability = _pattern->getProbability(_selectedStep);
        _copiedStep.gateLength = _pattern->getGateLength(_selectedStep);
        _copiedStep.decay = _pattern->getDecay(_selectedStep);
        _copiedStep.attack = _pattern->getAttack(_selectedStep);
        _copiedStep.ratchet = _pattern->getRatchet(_selectedStep);
        _copiedStep.microtiming = _pattern->getMicrotiming(_selectedStep);
        _copiedStep.hasData = true;
        _copyTriggered = true;
        // Optional: add visual feedback (e.g., flash or draw a small indicator)
    }

    // ---- PASTE or TOGGLE: joystick button ----
    if (_userInput.joystick_button.just_released) {
        if (_copiedStep.hasData) {
            // Paste copied data onto current step (overwrite all parameters)
            _pattern->setOn(_selectedStep, _copiedStep.on);
            _pattern->setCV(_selectedStep, _copiedStep.cv);
            _pattern->setProbability(_selectedStep, _copiedStep.probability);
            _pattern->setGateLength(_selectedStep, _copiedStep.gateLength);
            _pattern->setDecay(_selectedStep, _copiedStep.decay);
            _pattern->setAttack(_selectedStep, _copiedStep.attack);
            _pattern->setRatchet(_selectedStep, _copiedStep.ratchet);
            _pattern->setMicrotiming(_selectedStep, _copiedStep.microtiming);
            // (Keep _copiedStep.hasData = true to allow multiple pastes)
        } else {
            // No copy: toggle ON/OFF
            bool newState = !_pattern->getOn(_selectedStep);
            _pattern->setOn(_selectedStep, newState);
        }
    }
    
    if (_userInput.encoder_a_button.just_pressed) {
        _editModeEncA = (_editModeEncA + 1) % 3;
        if (_editModeEncA == 0) {
            _displayMode = 0;
        } else if (_editModeEncA == 1) {
            _displayMode = 2;
        } else {
            _displayMode = 4;
        }
    }
    if (_userInput.encoder_b_button.just_pressed) {
        _editModeEncB = (_editModeEncB + 1) % 4;
        if (_editModeEncB == 0) {
            _displayMode = 1;
        } else if (_editModeEncB == 1) {
            _displayMode = 3;
        } else if (_editModeEncB == 2) {
            _displayMode = 5;
        } else {
            _displayMode = 7;
        }
    }
}

void StepSequencerUI::blinkCursor() {
    uint64_t now = millis();
    if (now - _lastCursorBlink > CURSOR_BLINK_INTERVAL) {
        _cursorVisible = !_cursorVisible;
        _lastCursorBlink = now;
    }
}

uint8_t StepSequencerUI::constrainValue(int value) {
    if (value < 0) return 0;
    if (value > 99) return 99;
    return (uint8_t)value;
}

void StepSequencerUI::enter() {
    resetEncoderTracking();
    // Clear copy buffer on entry (optional, but good practice)
    _copiedStep.hasData = false;
    _copyTriggered = false;
}

void StepSequencerUI::resetEncoderTracking() {
    _lastEncoderAPos = _userInput.encoder_a.position;
    _lastEncoderBPos = _userInput.encoder_b.position;
}

// ===== CV conversion helpers =====

uint8_t StepSequencerUI::mapRawToPercent(uint16_t raw) const {
    if (!_sequencer) return 0;
    uint16_t min = _sequencer->getMinCV();
    uint16_t max = _sequencer->getMaxCV();
    if (max == min) return 0;
    // Clamp raw to range
    if (raw < min) raw = min;
    if (raw > max) raw = max;
    return map(raw, min, max, 0, 99);
}

uint16_t StepSequencerUI::mapPercentToRaw(uint8_t percent) const {
    if (!_sequencer) return 0;
    uint16_t min = _sequencer->getMinCV();
    uint16_t max = _sequencer->getMaxCV();
    if (max == min) return min;
    return map(percent, 0, 99, min, max);
}

const char* StepSequencerUI::getQuantizedNoteNameOrCV(uint16_t cvValue) {
    static char buffer[6];

    // Clamp first
    uint16_t minCV = _sequencer->getMinCV();
    uint16_t maxCV = _sequencer->getMaxCV();
    if (cvValue < minCV) cvValue = minCV;
    if (cvValue > maxCV) cvValue = maxCV;

    if (!_quantizerEnabled || !_quantizer) {
        uint8_t percent = mapRawToPercent(cvValue);
        sprintf(buffer, "%02d%%", percent);
        return buffer;
    }

    uint8_t noteIndex = _quantizer->getNoteIndex(cvValue);
    if (noteIndex == 0xFF) {
        // No in-scale notes available — fall back to percent display
        uint8_t percent = mapRawToPercent(cvValue);
        sprintf(buffer, "%02d%%", percent);
        return buffer;
    }
    return _quantizer->getNoteName(noteIndex);
}