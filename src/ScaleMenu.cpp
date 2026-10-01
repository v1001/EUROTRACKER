#include "ScaleMenu.h"
#include "Quantizer.h"

ScaleMenu::ScaleMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _selectedScaleIndex(0), _selectedRootIndex(0),
      _noteCursor(0), _exitRequested(false),
      _lastEncPosA(0), _lastEncPosB(0), _lastNavTime(0), _wasCentered(true) {
    for (int i = 0; i < PITCH_COUNT; i++) _workingNotes[i] = false;
}

ScaleMenu::~ScaleMenu() {
}

void ScaleMenu::enter() {
    _selectedIndex = 0;
    _exitRequested = false;
    _noteCursor = 0;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    _lastNavTime = 0;
    _wasCentered = true;
    loadCurrentValue();
    loadMaskFromQuantizer();
}

void ScaleMenu::loadCurrentValue() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    _selectedScaleIndex = quantizer.getScaleIndex();
    _selectedRootIndex = quantizer.getRootIndex();
}

void ScaleMenu::loadMaskFromQuantizer() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    for (int i = 0; i < PITCH_COUNT; i++) {
        _workingNotes[i] = quantizer.isPitchClassInScale((uint8_t)i);
    }
}

void ScaleMenu::rebuildMaskFromScale() {
    const ScalePattern* pat = getScalePattern(_selectedScaleIndex);
    if (!pat) return;

    for (int i = 0; i < PITCH_COUNT; i++) _workingNotes[i] = false;
    for (uint8_t i = 0; i < pat->numNotes; i++) {
        uint8_t pc = (uint8_t)((pat->intervals[i] + _selectedRootIndex) % 12);
        _workingNotes[pc] = true;
    }
}

uint8_t ScaleMenu::getDisplayPitchClass(int displayPos) const {
    return (uint8_t)((_selectedRootIndex + displayPos) % 12);
}

void ScaleMenu::update() {
    handleNavigation();
    handleEditing();
}

void ScaleMenu::handleNavigation() {
    unsigned long now = millis();
    if (now - _lastNavTime < 100) return;

    int dx = 0, dy = 0;
    if (_userInput.joystick.x_position > 30) dx = 1;
    else if (_userInput.joystick.x_position < -30) dx = -1;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (_selectedIndex == ITEM_NOTES && dy == 0 && dx != 0) {
        int newCursor = _noteCursor + dx;
        if (newCursor < 0) newCursor = 0;
        if (newCursor >= PITCH_COUNT) newCursor = PITCH_COUNT - 1;
        if (newCursor != _noteCursor) {
            _noteCursor = newCursor;
            _lastNavTime = now;
        }
        _wasCentered = false;
        return;
    }

    if (dy != 0) {
        if (_wasCentered || (now - _lastNavTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex < MENU_ITEM_COUNT) {
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

void ScaleMenu::handleEditing() {
    long encPosA = _userInput.encoder_a.position;
    bool encAChanged = (encPosA != _lastEncPosA);

    if (encAChanged) {
        int deltaA = (encPosA > _lastEncPosA) ? 1 : -1;

        if (_selectedIndex == ITEM_SCALE) {
            int numScales = getNumScalePatterns();
            int newIdx = (int)_selectedScaleIndex + deltaA;
            if (newIdx < 0) newIdx = 0;
            if (newIdx >= numScales) newIdx = numScales - 1;
            if (newIdx != _selectedScaleIndex) {
                _selectedScaleIndex = (uint8_t)newIdx;
                rebuildMaskFromScale();
            }
        } else if (_selectedIndex == ITEM_ROOT) {
            int newIdx = (int)_selectedRootIndex + deltaA;
            if (newIdx < 0) newIdx = 11;
            if (newIdx > 11) newIdx = 0;
            if (newIdx != _selectedRootIndex) {
                _selectedRootIndex = (uint8_t)newIdx;
                rebuildMaskFromScale();
            }
        } else if (_selectedIndex == ITEM_NOTES) {
            int newCursor = _noteCursor + deltaA;
            if (newCursor < 0) newCursor = 0;
            if (newCursor >= PITCH_COUNT) newCursor = PITCH_COUNT - 1;
            _noteCursor = newCursor;
        }

        _lastEncPosA = encPosA;
    }

    if (_userInput.joystick_button.just_released) {
        if (_selectedIndex == ITEM_NOTES) {
            toggleCurrentNote();
        } else if (_selectedIndex == ITEM_APPLY) {
            apply();
            _exitRequested = true;
        } else if (_selectedIndex == ITEM_EXIT) {
            _exitRequested = true;
        }
    }

    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void ScaleMenu::toggleCurrentNote() {
    uint8_t pc = getDisplayPitchClass(_noteCursor);
    _workingNotes[pc] = !_workingNotes[pc];
}

void ScaleMenu::apply() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    for (int i = 0; i < PITCH_COUNT; i++) {
        quantizer.setPitchClassInScale((uint8_t)i, _workingNotes[i]);
    }
    quantizer.setScaleIndex(_selectedScaleIndex);
    quantizer.setRootIndex(_selectedRootIndex);
}

void ScaleMenu::drawNotesRow(int y) {
    for (int i = 0; i < PITCH_COUNT; i++) {
        int x = NOTE_ROW_X + i * NOTE_ROW_SPACING;
        uint8_t pc = getDisplayPitchClass(i);
        const char* name = Quantizer::NOTE_NAMES[pc];
        bool inScale = _workingNotes[pc];

        if (inScale) {
            _display.fillRect(x - 1, y - 1, 7, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack(), _display.colorBlack());
        } else {
            _display.setTextColor(_display.colorWhite(), _display.colorWhite());
        }
        _display.printAt(name, x, y, ALIGN_LEFT);
    }

    if (_selectedIndex == ITEM_NOTES) {
        int x = NOTE_ROW_X + _noteCursor * NOTE_ROW_SPACING;
        _display.setTextColor(_display.colorWhite(), _display.colorWhite());
        _display.drawRect(x - 2, y - 2, 9, 12, _display.colorWhite());
    }

    _display.setTextColor(_display.colorWhite(), _display.colorWhite());
}

void ScaleMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    const ScalePattern* pattern = getScalePattern(_selectedScaleIndex);
    char scaleValue[20];
    snprintf(scaleValue, sizeof(scaleValue), "%.12s", pattern ? pattern->name : "?");
    const char* rootValue = Quantizer::NOTE_NAMES[_selectedRootIndex];

    // Row 0: Scale
    {
        int y = 0;
        bool selected = (_selectedIndex == ITEM_SCALE);
        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
        }
        _display.printAt("Scale", 2, y, ALIGN_LEFT);
        _display.printAt(scaleValue, 40, y, ALIGN_LEFT);
        if (selected) _display.setTextColor(_display.colorWhite());
    }

    // Row 1: Root
    {
        int y = 10;
        bool selected = (_selectedIndex == ITEM_ROOT);
        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
        }
        _display.printAt("Root", 2, y, ALIGN_LEFT);
        _display.printAt(rootValue, 40, y, ALIGN_LEFT);
        if (selected) _display.setTextColor(_display.colorWhite());
    }

    // Row 2: Notes label
    {
        int y = 20;
        bool selected = (_selectedIndex == ITEM_NOTES);
        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
        }
        _display.printAt("Notes", 2, y, ALIGN_LEFT);
        if (selected) _display.setTextColor(_display.colorWhite());
    }

    // Row 3: notes grid
    drawNotesRow(NOTE_ROW_Y);

    // Row 4: Apply
    {
        int y = 42;
        bool selected = (_selectedIndex == ITEM_APPLY);
        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
        }
        _display.printAt("Apply", 2, y, ALIGN_LEFT);
        if (selected) _display.setTextColor(_display.colorWhite());
    }

    // Row 5: Exit
    {
        int y = 52;
        bool selected = (_selectedIndex == ITEM_EXIT);
        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
        }
        _display.printAt("Exit", 2, y, ALIGN_LEFT);
        if (selected) _display.setTextColor(_display.colorWhite());
    }
}