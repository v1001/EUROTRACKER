#include "StepEditMenu.h"
#include "StepSequencer.h"
#include "Quantizer.h"

StepEditMenu::StepEditMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer),
      _track(0), _songStep(0), _pattern(nullptr),
      _selectedStep(0), _selectedRow(0),
      _exitRequested(false), _lastNavTime(0), _wasCentered(true),
      _lastEncPosA(0), _lastEncPosB(0),
      _popupActive(false), _popupText{0}, _popupTime(0) {
}

StepEditMenu::~StepEditMenu() {}

int StepEditMenu::getRowY(int rowIndex) const {
    return 2 + rowIndex * 11;
}

void StepEditMenu::enter(int track, int songStep) {
    _track = track;
    _songStep = songStep;
    _pattern = &_songSequencer.getSongData().getPattern(_track, _songStep);
    _selectedStep = 0;
    _selectedRow = 0;
    _exitRequested = false;
    _lastNavTime = 0;
    _wasCentered = true;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    _popupActive = false;
}

void StepEditMenu::update() {
    if (_popupActive && (millis() - _popupTime > 800)) {
        _popupActive = false;
    }
    handleNavigation();
    handleEncoders();
    handleButtons();
}

void StepEditMenu::handleNavigation() {
    if (!_pattern) return;
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;

    int dx = 0, dy = 0;
    if (_userInput.joystick.x_position > 30) dx = 1;
    else if (_userInput.joystick.x_position < -30) dx = -1;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dx != 0 || dy != 0) {
        if (_wasCentered || (now - _lastNavTime) > 50) {
            if (dx != 0) navigateStep(dx);
            if (dy != 0) navigateRow(dy);
        }
        _wasCentered = false;
    } else {
        _wasCentered = true;
    }
    _lastNavTime = now;
}

void StepEditMenu::navigateStep(int delta) {
    int numSteps = _pattern ? _pattern->getNumSteps() : 0;
    if (numSteps <= 0) return;
    int newStep = _selectedStep + delta;
    if (newStep < 0) newStep = 0;
    if (newStep >= numSteps) newStep = numSteps - 1;
    _selectedStep = newStep;
}

void StepEditMenu::navigateRow(int delta) {
    int newRow = _selectedRow + delta;
    if (newRow < 0) newRow = 0;
    if (newRow >= ROW_COUNT) newRow = ROW_COUNT - 1;
    _selectedRow = newRow;
}

void StepEditMenu::handleEncoders() {
    long encA = _userInput.encoder_a.position;
    long encB = _userInput.encoder_b.position;

    bool encAChanged = (encA != _lastEncPosA);
    bool encBChanged = (encB != _lastEncPosB);
    if (!encAChanged && !encBChanged) return;

    int deltaA = encAChanged ? ((encA > _lastEncPosA) ? 1 : -1) : 0;
    int deltaB = encBChanged ? ((encB > _lastEncPosB) ? 1 : -1) : 0;

    switch (_selectedRow) {
        case 0:
            if (deltaA != 0) navigateStep(deltaA);
            if (deltaB != 0) editCV(deltaB);
            break;
        case 1:
            if (deltaA != 0) editGateLength(deltaA);
            if (deltaB != 0) editProbability(deltaB);
            break;
        case 2:
            if (deltaA != 0) editAttack(deltaA);
            if (deltaB != 0) editDecay(deltaB);
            break;
        case 3:
            if (deltaA != 0) editRatchet(deltaA);
            if (deltaB != 0) editMicrotiming(deltaB);
            break;
    }

    _lastEncPosA = encA;
    _lastEncPosB = encB;
}

void StepEditMenu::handleButtons() {
    if (!_pattern) return;

    if (_userInput.encoder_a_button.just_released) {
        StepSequencer::copyStep(_pattern, (uint8_t)_selectedStep, (uint8_t)_track);
        char buf[24];
        snprintf(buf, sizeof(buf), "COPIED %02d", _selectedStep + 1);
        showPopup(buf);
    }

    if (_userInput.encoder_b_button.just_released) {
        if (StepSequencer::hasCopiedStep()) {
            uint8_t srcStep = StepSequencer::getCopiedSourceStep();
            StepSequencer::pasteStep(_pattern, (uint8_t)_selectedStep);
            char buf[24];
            snprintf(buf, sizeof(buf), "PASTED FROM %02d", srcStep + 1);
            showPopup(buf);
            markDirty();
        } else {
            showPopup("NO COPY");
        }
    }

    if (_userInput.joystick_button.just_released) {
        bool newOn = !_pattern->getOn(_selectedStep);
        _pattern->setOn(_selectedStep, newOn);
        markDirty();
    }

    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void StepEditMenu::editCV(int delta) {
    if (_track >= 4 || !_pattern) return;
    StepSequencer* seq = _songSequencer.getSequencer(_track);
    if (!seq) return;

    uint16_t minCV = seq->getMinCV();
    uint16_t maxCV = seq->getMaxCV();
    uint16_t current = _pattern->getCV(_selectedStep);
    if (current < minCV) current = minCV;
    if (current > maxCV) current = maxCV;

    if (seq->isQuantizerEnabled()) {
        Quantizer& q = _songSequencer.getSongData().getQuantizer(_track);
        uint8_t idx = q.getNoteIndex(current);
        if (idx != 0xFF) {
            int newIdx = idx;
            if (delta > 0) {
                for (int i = idx + 1; i < q.getNumNotes(); i++) {
                    if (q.getNote(i).inScale) { newIdx = i; break; }
                }
            } else {
                for (int i = idx - 1; i >= 0; i--) {
                    if (q.getNote(i).inScale) { newIdx = i; break; }
                }
            }
            _pattern->setCV(_selectedStep, q.getNoteDAC((uint8_t)newIdx));
            markDirty();
            return;
        }
    }

    int newVal = (int)current + delta * 50;
    if (newVal < (int)minCV) newVal = minCV;
    if (newVal > (int)maxCV) newVal = maxCV;
    _pattern->setCV(_selectedStep, (uint16_t)newVal);
    markDirty();
}

void StepEditMenu::editGateLength(int delta) {
    int v = _pattern->getGateLength(_selectedStep) + delta;
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    _pattern->setGateLength(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::editProbability(int delta) {
    int v = _pattern->getProbability(_selectedStep) + delta;
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    _pattern->setProbability(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::editAttack(int delta) {
    int v = _pattern->getAttack(_selectedStep) + delta;
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    _pattern->setAttack(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::editDecay(int delta) {
    int v = _pattern->getDecay(_selectedStep) + delta;
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    _pattern->setDecay(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::editRatchet(int delta) {
    int v = _pattern->getRatchet(_selectedStep) + delta;
    if (v < 1) v = 1;
    if (v > 4) v = 4;
    _pattern->setRatchet(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::editMicrotiming(int delta) {
    int v = _pattern->getMicrotiming(_selectedStep) + delta;
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    _pattern->setMicrotiming(_selectedStep, (uint8_t)v);
    markDirty();
}

void StepEditMenu::markDirty() {
    _songSequencer.markProjectDirty();
}

void StepEditMenu::showPopup(const char* text) {
    snprintf(_popupText, sizeof(_popupText), "%s", text);
    _popupActive = true;
    _popupTime = millis();
}

const char* StepEditMenu::getCVLabel() const {
    if (_track >= 4) return "--";
    StepSequencer* seq = _songSequencer.getSequencer(_track);
    if (!seq) return "CV";
    return seq->isQuantizerEnabled() ? "NOTE" : "CV";
}

const char* StepEditMenu::getCVDisplay(char* buffer, size_t bufSize) {
    if (!_pattern || _track >= 4) {
        snprintf(buffer, bufSize, "--");
        return buffer;
    }

    StepSequencer* seq = _songSequencer.getSequencer(_track);
    if (!seq) {
        snprintf(buffer, bufSize, "%d", _pattern->getCV(_selectedStep));
        return buffer;
    }

    uint16_t minCV = seq->getMinCV();
    uint16_t maxCV = seq->getMaxCV();
    uint16_t cv = _pattern->getCV(_selectedStep);
    if (cv < minCV) cv = minCV;
    if (cv > maxCV) cv = maxCV;

    if (seq->isQuantizerEnabled()) {
        Quantizer& q = _songSequencer.getSongData().getQuantizer(_track);
        uint8_t idx = q.getNoteIndex(cv);
        if (idx != 0xFF) {
            snprintf(buffer, bufSize, "%s", q.getNoteName(idx));
            return buffer;
        }
    }

    int pct = (maxCV == minCV) ? 0 : map(cv, minCV, maxCV, 0, 99);
    snprintf(buffer, bufSize, "%02d%%", pct);
    return buffer;
}

void StepEditMenu::draw() {
    if (!_pattern) return;

    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    for (int i = 0; i < ROW_COUNT; i++) drawRow(i);

    drawStatusBar();

    if (_popupActive) drawPopup();
}

void StepEditMenu::drawRow(int rowIndex) {
    int y = getRowY(rowIndex);

    if (_selectedRow == rowIndex) {
        _display.drawRect(0, y - 2, 128, 11, _display.colorWhite());
    }

    char valBuf[8];

    switch (rowIndex) {
        case 0: {
            _display.printAt("STEP", 2, y, ALIGN_LEFT);

            snprintf(valBuf, sizeof(valBuf), "%02d", _selectedStep + 1);
            if (_pattern->getOn(_selectedStep)) {
                _display.fillRect(30, y - 2, 14, 10, _display.colorWhite());
                _display.setTextColor(_display.colorBlack());
                _display.printAt(valBuf, 32, y, ALIGN_LEFT);
                _display.setTextColor(_display.colorWhite());
            } else {
                _display.printAt(valBuf, 32, y, ALIGN_LEFT);
            }

            _display.printAt(getCVLabel(), 66, y, ALIGN_LEFT);
            char cvBuf[8];
            const char* cvStr = getCVDisplay(cvBuf, sizeof(cvBuf));
            _display.printAt(cvStr, 96, y, ALIGN_LEFT);
            break;
        }
        case 1: {
            _display.printAt("GATE", 2, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getGateLength(_selectedStep));
            _display.printAt(valBuf, 32, y, ALIGN_LEFT);

            _display.printAt("PROB", 66, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getProbability(_selectedStep));
            _display.printAt(valBuf, 96, y, ALIGN_LEFT);
            break;
        }
        case 2: {
            _display.printAt("ATCK", 2, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getAttack(_selectedStep));
            _display.printAt(valBuf, 32, y, ALIGN_LEFT);

            _display.printAt("DECY", 66, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getDecay(_selectedStep));
            _display.printAt(valBuf, 96, y, ALIGN_LEFT);
            break;
        }
        case 3: {
            _display.printAt("RCHT", 2, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getRatchet(_selectedStep));
            _display.printAt(valBuf, 32, y, ALIGN_LEFT);

            _display.printAt("MCRT", 66, y, ALIGN_LEFT);
            snprintf(valBuf, sizeof(valBuf), "%02d", _pattern->getMicrotiming(_selectedStep));
            _display.printAt(valBuf, 96, y, ALIGN_LEFT);
            break;
        }
    }
}

void StepEditMenu::drawStatusBar() {
    _display.setTextColor(_display.colorWhite());
    _display.printAt("A:COPY  B:PASTE", 2, 54, ALIGN_LEFT);
}

void StepEditMenu::drawPopup() {
    const int BOX_W = 108;
    const int BOX_H = 16;
    const int BOX_X = (128 - BOX_W) / 2;
    const int BOX_Y = (64 - BOX_H) / 2;

    _display.fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, _display.colorWhite());
    _display.setTextColor(_display.colorBlack());
    _display.printCentered(_popupText, BOX_Y + 4);
    _display.setTextColor(_display.colorWhite());
}