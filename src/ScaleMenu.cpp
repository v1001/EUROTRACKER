#include "ScaleMenu.h"
#include "Quantizer.h"

ScaleMenu::ScaleMenu(DisplayManager& display, UserInput& userInput, SongSequencer& songSequencer, int track)
    : _display(display), _userInput(userInput), _songSequencer(songSequencer), _track(track),
      _selectedIndex(0), _selectedScaleIndex(0), _selectedRootIndex(0), _exitRequested(false),
      _lastEncPosA(0), _lastEncPosB(0), _lastNavTime(0), _wasCentered(true) {
}

ScaleMenu::~ScaleMenu() {
}

void ScaleMenu::enter() {
    _selectedIndex = 0;
    _exitRequested = false;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    _lastNavTime = 0;
    _wasCentered = true;
    loadCurrentValue();
}

void ScaleMenu::loadCurrentValue() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    _selectedScaleIndex = quantizer.getScaleIndex();
    _selectedRootIndex = quantizer.getRootIndex();
}

void ScaleMenu::update() {
    handleNavigation();
    handleEditing();
}

void ScaleMenu::handleNavigation() {
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
            _selectedScaleIndex = (uint8_t)newIdx;
        } else if (_selectedIndex == ITEM_ROOT) {
            int newIdx = (int)_selectedRootIndex + deltaA;
            if (newIdx < 0) newIdx = 11;
            if (newIdx > 11) newIdx = 0;
            _selectedRootIndex = (uint8_t)newIdx;
        }

        _lastEncPosA = encPosA;
    }

    // Joystick click
    if (_userInput.joystick_button.just_released) {
        if (_selectedIndex == ITEM_APPLY) {
            apply();
            _exitRequested = true;
        } else if (_selectedIndex == ITEM_EXIT) {
            _exitRequested = true;
        }
    }

    // Save button: exit without applying
    if (_userInput.save_button.just_released) {
        _exitRequested = true;
    }
}

void ScaleMenu::apply() {
    Quantizer& quantizer = _songSequencer.getSongData().getQuantizer(_track);
    const ScalePattern* pattern = getScalePattern(_selectedScaleIndex);
    if (!pattern) return;

    quantizer.applyScaleIntervals(pattern->intervals, pattern->numNotes, _selectedRootIndex);
    quantizer.setScaleIndex(_selectedScaleIndex);
    quantizer.setRootIndex(_selectedRootIndex);
}

void ScaleMenu::draw() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    const char* items[] = {"Scale", "Root", "Apply", "Exit"};

    char scaleValue[20];
    const ScalePattern* pattern = getScalePattern(_selectedScaleIndex);
    snprintf(scaleValue, sizeof(scaleValue), "%.12s", pattern ? pattern->name : "?");

    const char* rootValue = Quantizer::NOTE_NAMES[_selectedRootIndex];

    for (int i = 0; i <= ITEM_EXIT; i++) {
        int y = i * 10;
        const char* value = "";

        if (i == ITEM_SCALE) value = scaleValue;
        else if (i == ITEM_ROOT) value = rootValue;

        if (i == _selectedIndex) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 40, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[i], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 40, y, ALIGN_LEFT);
            }
        }
    }
}