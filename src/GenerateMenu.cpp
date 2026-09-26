#include "GenerateMenu.h"
#include "Quantizer.h"

GenerateMenu::GenerateMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _minCV(0), _maxCV(4095), _numNotes(61), _exitRequested(false),
      _lastEncPosA(0), _lastEncPosB(0), _lastNavTime(0), _wasCentered(true){
}

GenerateMenu::~GenerateMenu() {
}

void GenerateMenu::enter() {
    _selectedIndex = 0;
    _exitRequested = false;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    _lastNavTime = 0;
    _wasCentered = true;
    loadCurrentValue();
}

void GenerateMenu::loadCurrentValue() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    _minCV = quantizer.getStartDAC();
    _maxCV = quantizer.getEndDAC();
    _numNotes = quantizer.getNumNotesInScale();
}

void GenerateMenu::update() {
    handleNavigation();
    handleEditing();
}

void GenerateMenu::handleNavigation() {
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (_wasCentered || (now - _lastNavTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex <= ITEM_EXIT) {
                _selectedIndex = newIndex;
                _lastNavTime = now;
            }
            _wasCentered = false;
        }
    } else {
        _wasCentered = true;
    }
    _lastNavTime = now;
}

void GenerateMenu::handleEditing() {
    long encPosA = _userInput.encoder_a.position;
    long encPosB = _userInput.encoder_b.position;

    bool encAChanged = (encPosA != _lastEncPosA);
    bool encBChanged = (encPosB != _lastEncPosB);

    if (encAChanged || encBChanged) {
        int deltaA = (encAChanged) ? ((encPosA > _lastEncPosA) ? 1 : -1) : 0;
        int deltaB = (encBChanged) ? ((encPosB > _lastEncPosB) ? 1 : -1) : 0;

        // Only editable items (Min CV, Max CV, Num Notes)
        switch (_selectedIndex) {
            case ITEM_MIN_CV: {
                int newVal = _minCV;
                if (encAChanged) newVal += deltaA * 50;
                if (encBChanged) newVal += deltaB;
                if (newVal < 0) newVal = 0;
                if (newVal > (int)_maxCV - 1) newVal = (int)_maxCV - 1;
                _minCV = (uint16_t)newVal;
                _songSequencer.previewDAC(_track, _minCV);
                break;
            }
            case ITEM_MAX_CV: {
                int newVal = _maxCV;
                if (encAChanged) newVal += deltaA * 50;
                if (encBChanged) newVal += deltaB;
                if (newVal > 4095) newVal = 4095;
                if (newVal < (int)_minCV + 1) newVal = (int)_minCV + 1;
                _maxCV = (uint16_t)newVal;
                _songSequencer.previewDAC(_track, _maxCV);
                break;
            }
            case ITEM_NUM_NOTES: {
                int newVal = _numNotes;
                if (encAChanged) newVal += deltaA * 2;
                if (encBChanged) newVal += deltaB;
                if (newVal < 2) newVal = 2;
                if (newVal > 128) newVal = 128;
                _numNotes = (uint8_t)newVal;
                break;
            }
        }

        _lastEncPosA = encPosA;
        _lastEncPosB = encPosB;
    }

    // Joystick click
    if (_userInput.joystick_button.just_released) {
        if (_selectedIndex == ITEM_GENERATE) {
            generate();
            _exitRequested = true;  // exit after generate
        } else if (_selectedIndex == ITEM_EXIT) {
            _exitRequested = true;
        }
    }

    // Save button: exit
    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void GenerateMenu::generate() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    quantizer.generateChromatic(_minCV, _maxCV, _numNotes);
    // Also set scale active? We'll keep it active by default.
}

void GenerateMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    const char* items[] = {"Min CV", "Max CV", "Num Notes", "Generate", "Exit"};
    char valueBuffer[16];

    for (int i = 0; i <= ITEM_EXIT; i++) {
        int y = i * 10;
        const char* value = "";

        if (i == _selectedIndex) {
            switch (i) {
                case ITEM_MIN_CV:    sprintf(valueBuffer, "%d", _minCV); value = valueBuffer; break;
                case ITEM_MAX_CV:    sprintf(valueBuffer, "%d", _maxCV); value = valueBuffer; break;
                case ITEM_NUM_NOTES: sprintf(valueBuffer, "%d", _numNotes); value = valueBuffer; break;
                default: value = ""; break;
            }
        } else {
            // Show saved values for non-selected editable items (but we don't have saved values here; we show current values)
            switch (i) {
                case ITEM_MIN_CV:    sprintf(valueBuffer, "%d", _minCV); value = valueBuffer; break;
                case ITEM_MAX_CV:    sprintf(valueBuffer, "%d", _maxCV); value = valueBuffer; break;
                case ITEM_NUM_NOTES: sprintf(valueBuffer, "%d", _numNotes); value = valueBuffer; break;
                default: value = ""; break;
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