#include "SongUI.h"

SongUI::SongUI(DisplayManager& display, UserInput& userInput, SongData& songData)
    : _display(display), _userInput(userInput), _songData(songData),
      _cursorRow(0), _cursorCol(1), _scrollOffset(0),
      _lastEncoderATime(0), _lastEncoderBTime(0),
      _lastEncoderAPos(0), _lastEncoderBPos(0),
      _lastNavTime(0), _lastJoystickMoveTime(0), _joystickWasCentered(true),
      _trackXPositions{20, 38, 56, 74, 92, 110}, _showPattern(true),
      _currentPlayingStep(-1), _playbackProgress(0), _needsSave(false) {
}

SongUI::~SongUI() {
}

void SongUI::begin() {
}

void SongUI::update() {
    handleNavigation();
    handleEncoders();
    handleButtons();
    // Auto‑revert to pattern preview after 3 seconds of encoder inactivity
    if (!_showPattern) {
        unsigned long lastActivity = max(_lastEncoderATime, _lastEncoderBTime);
        if (millis() - lastActivity >= 3000) {
            _showPattern = true;
        }
    }
}

void SongUI::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    
    // Header - track titles (show Q instead of T for quantized tracks)
    for (int i = 0; i < NUM_TRACKS; i++) {
        char label[4];
        if (i < 4 && _songData.isQuantizerEnabled(i)) {
            // Quantized track - show "Q1", "Q2", etc.
            sprintf(label, "Q%d", i + 1);
        } else {
            // Normal track - show "T1", "T2", etc.
            sprintf(label, "T%d", i + 1);
        }
        _display.printAt(label, _trackXPositions[i] + 4, 0, ALIGN_LEFT);
    }
    _display.drawLine(0, 10, 128, 10, _display.colorWhite());
    
    // Song steps
    for (int row = 0; row < ROWS_PER_PAGE; row++) {
        int stepNum = _scrollOffset + row;
        if (stepNum >= _songData.getLength()) break;
        
        int y = 14 + row * 10;
        
        // Show progress bar for currently playing step, otherwise show step number
        if (stepNum == _currentPlayingStep && _playbackProgress > 0 && _playbackProgress < 100) {
            drawProgressBar(2, y, 16, 8, _playbackProgress);
        } else {
            char stepText[5];
            sprintf(stepText, "%02d", stepNum + 1);
            _display.printAt(stepText, 2, y, ALIGN_LEFT);
        }
        
        for (int col = 0; col < NUM_TRACKS; col++) {
            int x = _trackXPositions[col];
            
            if (_showPattern) {
                drawPatternPreview(_songData.getPattern(col, stepNum), x, y);
            } else {
                const char* divText = _songData.getDivider(col, stepNum).text;
                _display.printAt(divText, x, y - 1, ALIGN_LEFT);
            }
        }
    }
    
    // Cursor
    if (_cursorCol >= 1 && _cursorCol <= NUM_TRACKS) {
        int cursorX = _trackXPositions[_cursorCol - 1] - 1;
        int cursorY = 14 + _cursorRow * 10 - 2;
        _display.drawRect(cursorX, cursorY, 18, 10, _display.colorWhite());
    }
    
    // Status bar
    drawStatusBar();
    // No display.update() here
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
        // Encoder A: Rotate pattern based on position change
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
            // Signal that project needs saving
            _needsSave = true;
        }
        
        // Encoder B: Cycle through divider indices based on position change
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
    // Copy
    if (_userInput.encoder_a_button.just_released) {
        int currentStep = _scrollOffset + _cursorRow;
        int currentTrack = _cursorCol - 1;
        
        if (currentTrack >= 0 && currentTrack < NUM_TRACKS && currentStep < _songData.getLength()) {
            _songData.copyPattern(currentTrack, currentStep);
        }
    }
    
    // Paste
    if (_userInput.encoder_b_button.just_released && _songData.hasCopiedData()) {
        int currentStep = _scrollOffset + _cursorRow;
        int currentTrack = _cursorCol - 1;
        
        if (currentTrack >= 0 && currentTrack < NUM_TRACKS && currentStep < _songData.getLength()) {
            _songData.pastePattern(currentTrack, currentStep);
            _needsSave = true;
        }
    }
}

bool SongUI::needsSave() const {
    return _needsSave;
}

void SongUI::clearSaveFlag() {
    _needsSave = false;
}

void SongUI::setSaveFlag() {
    _needsSave = true;
}

void SongUI::resetEncoderTracking() {
    _lastEncoderAPos = _userInput.encoder_a.position;
    _lastEncoderBPos = _userInput.encoder_b.position;
}

void SongUI::drawProgressBar(int x, int y, int width, int height, int percent) {
    if (percent > 100) percent = 100;
    _display.drawRect(x, y, width, height, _display.colorWhite());
    int fillWidth = (width - 2) * percent / 100;
    if (fillWidth > 0) {
        _display.fillRect(x + 1, y + 1, fillWidth, height - 2, _display.colorWhite());
    }
}