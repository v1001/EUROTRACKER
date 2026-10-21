#include "SongUI.h"
#include "SongSequencer.h"

SongUI::SongUI(DisplayManager& display, UserInput& userInput, SongData& songData)
    : _display(display), _userInput(userInput), _songData(songData),
      _cursorRow(0), _cursorCol(1), _scrollOffset(0),
      _lastEncoderATime(0), _lastEncoderBTime(0),
      _lastEncoderAPos(0), _lastEncoderBPos(0),
      _lastNavTime(0), _lastJoystickMoveTime(0), _joystickWasCentered(true),
      _trackXPositions{20, 38, 56, 74, 92, 110}, _showPattern(true),
      _currentPlayingStep(-1), _playbackProgress(0), _needsSave(false) {
}

SongUI::~SongUI() {}

void SongUI::begin() {}

void SongUI::update() {
    handleNavigation();
    handleEncoders();
    handleButtons();
    if (!_showPattern) {
        unsigned long lastActivity = max(_lastEncoderATime, _lastEncoderBTime);
        if (millis() - lastActivity >= 3000) _showPattern = true;
    }
}

char SongUI::getEffectChar(uint8_t eff) {
    switch (eff) {
        case 1: return 'C';
        case 2: return 'S';
        case 3: return 'R';
        case 4: return 'D';
        default: return '-';
    }
}

void SongUI::draw(PerformanceState* perf) {
    bool isPerf = (perf && perf->active);
    int rowsPerPage = isPerf ? 5 : ROWS_PER_PAGE;
    
    // ===== Auto-scroll to follow the playing step =====
    if (isPerf && _currentPlayingStep >= 0) {
        int maxOffset = (int)_songData.getLength() - rowsPerPage;
        if (maxOffset < 0) maxOffset = 0;
        
        int desiredOffset = _currentPlayingStep;
        if (desiredOffset > maxOffset) desiredOffset = maxOffset;
        
        _scrollOffset = desiredOffset;
    }

    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    
    // ===== Track headers =====
    int titleY = isPerf ? 2 : 0;
    for (int i = 0; i < NUM_TRACKS; i++) {
        bool muted = (isPerf && perf->trackMuted[i]);
        bool selX = (isPerf && perf->selectedTrackX == i);
        bool selY = (isPerf && perf->selectedTrackY == i);
        
        char label[4];
        if (muted) {
            sprintf(label, "M ");
        } else if (isPerf) {
            char ex = getEffectChar(_songData.getEffectX(perf->activeBank, i));
            char ey = getEffectChar(_songData.getEffectY(perf->activeBank, i));
            if (ex == '-' && ey == '-') {
                sprintf(label, "T%d", i + 1);
            } else {
                sprintf(label, "%c%c", ex, ey);
            }
        } else {
            sprintf(label, "T%d", i + 1);
        }
        
        // Draw character by character to handle individual selection inversion
        for (int c = 0; c < 2; c++) {
            int x = _trackXPositions[i] + 2 + c * 6;
            bool invert = (c == 0 && selX) || (c == 1 && selY);
            
            if (invert) {
                _display.fillRect(x - 1, titleY - 1, 7, 9, _display.colorWhite());
                _display.setTextColor(_display.colorBlack());
            } else {
                _display.setTextColor(_display.colorWhite());
            }
            
            char cbuf[2] = {label[c], '\0'};
            _display.printAt(cbuf, x, titleY, ALIGN_LEFT);
        }
    }
    
    // Reset text color so it doesn't leak into the pattern rows
    _display.setTextColor(_display.colorWhite());
    
    if (!isPerf) {
        _display.drawLine(0, 10, 128, 10, _display.colorWhite());
    }
    
    // ===== Song step rows =====
    for (int row = 0; row < rowsPerPage; row++) {
        int stepNum = _scrollOffset + row;
        if (stepNum >= (int)_songData.getLength()) break;
        
        int y = 14 + row * 10;
        SongData::SongStep* step = _songData.getStep(stepNum);
        
        if (stepNum == _currentPlayingStep && _playbackProgress > 0 && _playbackProgress < 100) {
            drawProgressBar(2, y, 16, 8, _playbackProgress);
        } else {
            char stepText[5];
            if (step->loopTarget != nullptr) {
                sprintf(stepText, "L%d", step->loopCount + 1); 
            } else {
                sprintf(stepText, "%02d", stepNum + 1);
            }
            _display.printAt(stepText, 2, y, ALIGN_LEFT);
        }
        
        for (int col = 0; col < NUM_TRACKS; col++) {
            if (isPerf && perf->trackMuted[col]) continue;
            
            int x = _trackXPositions[col];
            if (_showPattern) {
                drawPatternPreview(_songData.getPattern(col, stepNum), x, y);
            } else {
                const char* divText = _songData.getDivider(col, stepNum).text;
                _display.printAt(divText, x, y - 1, ALIGN_LEFT);
            }
        }
    }
    
    // ===== Loop lines =====
    for (int row = 0; row < rowsPerPage; row++) {
        int stepNum = _scrollOffset + row;
        if (stepNum >= (int)_songData.getLength()) break;
        
        SongData::SongStep* step = _songData.getStep(stepNum);
        if (step->loopTarget != nullptr) {
            int targetIdx = _songData.getStepIndex(step->loopTarget);
            int yCurrent = 14 + row * 10 + 4;
            
            if (targetIdx >= _scrollOffset && targetIdx < _scrollOffset + rowsPerPage) {
                // Target is on screen
                int targetRow = targetIdx - _scrollOffset;
                int yTarget = 14 + targetRow * 10 + 4;
                _display.drawLine(0, yTarget, 0, yCurrent, _display.colorWhite());
                _display.drawLine(0, yTarget, 2, yTarget, _display.colorWhite());
                _display.drawLine(0, yCurrent, 2, yCurrent, _display.colorWhite());
            } else if (targetIdx < _scrollOffset) {
                // Target above view
                _display.drawLine(0, 10, 0, yCurrent, _display.colorWhite());
                _display.drawLine(0, yCurrent, 2, yCurrent, _display.colorWhite());
                _display.drawLine(0, 10, 2, 12, _display.colorWhite());
            } else {
                // Target below view
                _display.drawLine(0, yCurrent, 0, 53, _display.colorWhite());
                _display.drawLine(0, yCurrent, 2, yCurrent, _display.colorWhite());
                _display.drawLine(0, 53, 2, 51, _display.colorWhite());
            }
        }
    }
    
    // ===== Cursor (normal mode only) =====
    if (!isPerf) {
        if (_cursorCol >= 1 && _cursorCol <= NUM_TRACKS) {
            int cursorX = _trackXPositions[_cursorCol - 1] - 1;
            int cursorY = 14 + _cursorRow * 10 - 2;
            _display.drawRect(cursorX, cursorY, 18, 10, _display.colorWhite());
        }
        drawStatusBar();
    }
}

void SongUI::drawPatternPreview(StepPattern& pattern, int x, int y) {
    int lineY = y;
    uint8_t numSteps = pattern.getNumSteps();
    
    for (int row = 0; row < 4; row++) {
        for (int i = 0; i < 8; i++) {
            int stepIndex = row * 8 + i;
            if (stepIndex < numSteps) {
                int dashX = x + (i * 2);
                _display.drawPixel(dashX, lineY, _display.colorWhite());
            }
        }
        
        for (int col = 0; col < 8; col++) {
            int stepIndex = row * 8 + col;
            if (stepIndex < numSteps && pattern.getOn(stepIndex)) {
                int squareX = x + (col * 2);
                _display.fillRect(squareX, lineY - 1, 2, 2, _display.colorWhite());
            }
        }
        lineY += 2;
    }
}

void SongUI::drawStatusBar() {
    char status[32];
    sprintf(status, "A:ROT B:MOD");
    _display.printAt(status, 48, 54, ALIGN_LEFT);
}

void SongUI::handleNavigation() {
    uint64_t now = millis();
    if (now - _lastNavTime < MOVE_DELAY) return;
    
    int dx = 0, dy = 0;
    if (_userInput.joystick.x_position > 30) dx = 1;
    else if (_userInput.joystick.x_position < -30) dx = -1;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;
    
    if (dx != 0 || dy != 0) {
        if (dx != 0) {
            int newCol = _cursorCol + dx;
            if (newCol >= 1 && newCol <= NUM_TRACKS) {
                _cursorCol = newCol;
                _lastNavTime = now;
            }
        }
        if (dy != 0) {
            int newRow = _cursorRow + dy;
            if (newRow >= 0 && newRow < ROWS_PER_PAGE) {
                _cursorRow = newRow;
                _lastNavTime = now;
            } else if (newRow < 0 && _scrollOffset > 0) {
                _scrollOffset--;
                _cursorRow = 0;
                _lastNavTime = now;
            } else if (newRow >= ROWS_PER_PAGE) {
                int nextStep = _scrollOffset + ROWS_PER_PAGE;
                if (nextStep < _songData.getLength()) {
                    _scrollOffset++;
                    _cursorRow = ROWS_PER_PAGE - 1;
                    _lastNavTime = now;
                }
            }
        }
    }
}

void SongUI::handleEncoders() {
    int step = _scrollOffset + _cursorRow;
    int track = _cursorCol - 1;
    
    if (track >= 0 && track < NUM_TRACKS && step < _songData.getLength()) {
        long currentEncAPos = _userInput.encoder_a.position;
        if (currentEncAPos != _lastEncoderAPos) {
            int delta = (currentEncAPos > _lastEncoderAPos) ? 1 : -1;
            int rotations = abs(delta);
            for (int i = 0; i < rotations; i++) {
                _songData.rotatePattern(track, step, delta > 0 ? 1 : -1);
            }
            _lastEncoderAPos = currentEncAPos;
            _lastEncoderATime = millis();
            _showPattern = true;
            _needsSave = true;
        }
        
        long currentEncBPos = _userInput.encoder_b.position;
        if (currentEncBPos != _lastEncoderBPos) {
            int delta = (currentEncBPos > _lastEncoderBPos) ? -1 : 1;
            int currentIdx = _songData.getDividerIndex(track, step);
            int newIdx = currentIdx + delta;
            
            if (newIdx < 0) newIdx = 0;
            if (newIdx >= _songData.getNumDividers()) newIdx = _songData.getNumDividers() - 1;
            
            if (newIdx != currentIdx) {
                _songData.setDividerIndex(track, step, newIdx);
                _lastEncoderBTime = millis();
                _needsSave = true;
            }
            _lastEncoderBPos = currentEncBPos;
            _showPattern = false;
        }
    }
}

void SongUI::handleButtons() {
    if (_userInput.encoder_a_button.just_released) {
        int currentStep = _scrollOffset + _cursorRow;
        int currentTrack = _cursorCol - 1;
        if (currentTrack >= 0 && currentTrack < NUM_TRACKS && currentStep < _songData.getLength()) {
            _songData.copyPattern(currentTrack, currentStep);
        }
    }
    
    if (_userInput.encoder_b_button.just_released && _songData.hasCopiedData()) {
        int currentStep = _scrollOffset + _cursorRow;
        int currentTrack = _cursorCol - 1;
        if (currentTrack >= 0 && currentTrack < NUM_TRACKS && currentStep < _songData.getLength()) {
            _songData.pastePattern(currentTrack, currentStep);
            _needsSave = true;
        }
    }
}

bool SongUI::needsSave() const { return _needsSave; }
void SongUI::clearSaveFlag() { _needsSave = false; }
void SongUI::setSaveFlag() { _needsSave = true; }
void SongUI::resetEncoderTracking() {
    _lastEncoderAPos = _userInput.encoder_a.position;
    _lastEncoderBPos = _userInput.encoder_b.position;
}

void SongUI::drawProgressBar(int x, int y, int width, int height, int percent) {
    if (percent > 100) percent = 100;
    _display.drawRect(x, y, width, height, _display.colorWhite());
    int fillWidth = (width - 2) * percent / 100;
    if (fillWidth > 0) _display.fillRect(x + 1, y + 1, fillWidth, height - 2, _display.colorWhite());
}

void SongUI::handlePerformanceTimeout() {
    if (!_showPattern) {
        unsigned long lastActivity = max(_lastEncoderATime, _lastEncoderBTime);
        if (millis() - lastActivity >= 3000) {
            _showPattern = true;
        }
    }
}