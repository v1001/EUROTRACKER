#include "NotesMenu.h"

NotesMenu::NotesMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _scrollOffset(0), _exitRequested(false) {
}

NotesMenu::~NotesMenu() {
}

void NotesMenu::enter() {
    _selectedIndex = 0;
    _scrollOffset = 0;
    _exitRequested = false;
    // Ensure the quantizer has notes
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    if (quantizer.getNumNotes() == 0) {
        quantizer.generateChromatic(0, 4095, 61);
    }
}

void NotesMenu::update() {
    handleNavigation();
    handleEditing();
}

void NotesMenu::handleNavigation() {
    unsigned long now = millis();
    static unsigned long lastNavTime = 0;
    static bool wasCentered = true;
    if (now - lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (wasCentered || (now - lastNavTime) > 50) {
            Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
            int numNotes = quantizer.getNumNotes();
            if (numNotes == 0) return;
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex < numNotes) {
                _selectedIndex = newIndex;
                if (_selectedIndex < _scrollOffset) {
                    _scrollOffset = _selectedIndex;
                } else if (_selectedIndex >= _scrollOffset + 6) {
                    _scrollOffset = _selectedIndex - 5;
                }
                lastNavTime = now;
            }
            wasCentered = false;
        }
    } else {
        wasCentered = true;
    }
    lastNavTime = now;
}

void NotesMenu::handleEditing() {
    long encPosA = _userInput.encoder_a.position;
    long encPosB = _userInput.encoder_b.position;
    static long lastEncPosA = 0, lastEncPosB = 0;

    bool encAChanged = (encPosA != lastEncPosA);
    bool encBChanged = (encPosB != lastEncPosB);

    if (encAChanged || encBChanged) {
        int deltaA = (encAChanged) ? ((encPosA > lastEncPosA) ? 1 : -1) : 0;
        int deltaB = (encBChanged) ? ((encPosB > lastEncPosB) ? 1 : -1) : 0;

        Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
        if (_selectedIndex < quantizer.getNumNotes()) {
            uint16_t current = quantizer.getNoteDAC(_selectedIndex);
            int newVal = current;
            if (encAChanged) newVal += deltaA * 50;  // coarse
            if (encBChanged) newVal += deltaB;       // fine

            // Determine bounds from adjacent notes
            int minVal = 0;
            int maxVal = 4095;

            if (_selectedIndex > 0) {
                minVal = quantizer.getNoteDAC(_selectedIndex - 1);
            }
            if (_selectedIndex < quantizer.getNumNotes() - 1) {
                maxVal = quantizer.getNoteDAC(_selectedIndex + 1);
            }

            // Clamp to bounds
            if (newVal < minVal) newVal = minVal;
            if (newVal > maxVal) newVal = maxVal;

            if ((uint16_t)newVal != current) {
                quantizer.setNoteDAC(_selectedIndex, (uint16_t)newVal);
            }
        }
        lastEncPosA = encPosA;
        lastEncPosB = encPosB;
    }

    // Joystick click toggles inScale
    if (_userInput.joystick_button.just_released) {
        Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
        if (_selectedIndex < quantizer.getNumNotes()) {
            quantizer.toggleNoteInScale(_selectedIndex);
        }
    }

    // Save button: exit
    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void NotesMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    int numNotes = quantizer.getNumNotes();
    if (numNotes == 0) {
        _display.printCentered("No notes", 32);
        return;
    }

    int displayCount = (numNotes - _scrollOffset > 6) ? 6 : numNotes - _scrollOffset;
    for (int i = 0; i < displayCount; i++) {
        int idx = _scrollOffset + i;
        int y = i * 10 + 2;  // top margin
        const Quantizer::Note& note = quantizer.getNote(idx);

        // Circle: filled if active, else empty
        int circleX = 4;
        int circleY = y + 3;  // center of the row
        if (note.inScale) {
            _display.fillCircle(circleX, circleY, 3, _display.colorWhite());
        } else {
            _display.drawCircle(circleX, circleY, 3, _display.colorWhite());
        }

        // Note name (2 characters)
        _display.printAt(note.name, 14, y, ALIGN_LEFT);

        // CV value (right‑aligned)
        char valStr[6];
        sprintf(valStr, "%d", note.dacValue);
        _display.printAt(valStr, 80, y, ALIGN_LEFT);

        // Highlight selected row with a border
        if (idx == _selectedIndex) {
            _display.drawRect(0, y - 2, 128, 10, _display.colorWhite());
        }
    }
}