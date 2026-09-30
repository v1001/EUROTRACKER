#include "GenerateMenu.h"
#include "Quantizer.h"

GenerateMenu::GenerateMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _minCV(0), _maxCV(4095),
      _minNoteIndex(0), _minNoteOctave(4),
      _maxNoteIndex(0), _maxNoteOctave(9),
      _exitRequested(false),
      _lastEncPosA(0), _lastEncPosB(0), _lastNavTime(0), _wasCentered(true) {
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

    uint8_t numNotes = quantizer.getNumNotes();
    if (numNotes > 0) {
        Quantizer::parseNoteName(quantizer.getNote(0).name,
                                 _minNoteIndex, _minNoteOctave);
        Quantizer::parseNoteName(quantizer.getNote(numNotes - 1).name,
                                 _maxNoteIndex, _maxNoteOctave);
    }
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
            case ITEM_MIN_NOTE: {
                uint8_t newPitch = _minNoteIndex;
                uint8_t newOct   = _minNoteOctave;

                if (encAChanged) {
                    int idx = (int)_minNoteIndex + deltaA;
                    while (idx < 0)   idx += 12;
                    while (idx >= 12) idx -= 12;
                    newPitch = (uint8_t)idx;
                }
                if (encBChanged) {
                    int oct = (int)_minNoteOctave + deltaB;
                    if (oct < 0) oct = 0;
                    if (oct > 9) oct = 9;
                    newOct = (uint8_t)oct;
                }

                // Reject if the new min would meet or pass the current max.
                int newMinAbs = (int)newOct * 12 + (int)newPitch;
                int maxAbs    = (int)_maxNoteOctave * 12 + (int)_maxNoteIndex;
                if (newMinAbs < maxAbs) {
                    _minNoteIndex  = newPitch;
                    _minNoteOctave = newOct;
                }
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
            case ITEM_MAX_NOTE: {
                uint8_t newPitch = _maxNoteIndex;
                uint8_t newOct   = _maxNoteOctave;

                if (encAChanged) {
                    int idx = (int)_maxNoteIndex + deltaA;
                    while (idx < 0)   idx += 12;
                    while (idx >= 12) idx -= 12;
                    newPitch = (uint8_t)idx;
                }
                if (encBChanged) {
                    int oct = (int)_maxNoteOctave + deltaB;
                    if (oct < 0) oct = 0;
                    if (oct > 9) oct = 9;
                    newOct = (uint8_t)oct;
                }

                // Reject if the new max would meet or fall below the current min.
                int newMaxAbs = (int)newOct * 12 + (int)newPitch;
                int minAbs    = (int)_minNoteOctave * 12 + (int)_minNoteIndex;
                if (newMaxAbs > minAbs) {
                    _maxNoteIndex  = newPitch;
                    _maxNoteOctave = newOct;
                }
                break;
            }
            default:
                break;
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
    int minAbs = (int)_minNoteOctave * 12 + (int)_minNoteIndex;
    int maxAbs = (int)_maxNoteOctave * 12 + (int)_maxNoteIndex;

    // Defensive guard. handleEditing() blocks invalid states, so this
    // should never trigger from normal UI use. It protects against a
    // corrupted or externally-modified saved file.
    if (maxAbs <= minAbs) return;

    int interval = maxAbs - minAbs;
    uint8_t numNotes = (uint8_t)(interval + 1);

    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    quantizer.generateChromatic(_minCV, _maxCV, numNotes,
                                _minNoteIndex, _minNoteOctave);
}

void GenerateMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    const char* items[] = {"Min CV", "Min Note/Oct", "Max CV", "Max Note/Oct", "Generate", "Exit"};
    char valueBuffer[16];

    for (int i = 0; i <= ITEM_EXIT; i++) {
        int y = i * 10;
        const char* value = "";

        switch (i) {
            case ITEM_MIN_CV:
                snprintf(valueBuffer, sizeof(valueBuffer), "%d", _minCV);
                value = valueBuffer;
                break;
            case ITEM_MIN_NOTE:
                snprintf(valueBuffer, sizeof(valueBuffer), "%s %d",
                         Quantizer::NOTE_NAMES[_minNoteIndex], _minNoteOctave);
                value = valueBuffer;
                break;
            case ITEM_MAX_CV:
                snprintf(valueBuffer, sizeof(valueBuffer), "%d", _maxCV);
                value = valueBuffer;
                break;
            case ITEM_MAX_NOTE:
                snprintf(valueBuffer, sizeof(valueBuffer), "%s %d",
                         Quantizer::NOTE_NAMES[_maxNoteIndex], _maxNoteOctave);
                value = valueBuffer;
                break;
            default:
                value = "";
                break;
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