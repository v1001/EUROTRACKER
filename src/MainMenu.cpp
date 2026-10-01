#include "MainMenu.h"
#include "TrackerApp.h"
#include "SongData.h"

MainMenu::MainMenu(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler, TrackerApp* trackerApp)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler), _trackerApp(trackerApp),
      _visibleCount(0),
      _selectedIndex(0), _scrollOffset(0), _editing(false), _editValue(0),
      _lastNavTime(0), _lastJoystickMoveTime(0), _joystickWasCentered(true),
      _lastEncPosA(0), _lastEncPosB(0), _exitRequested(false), _saveSlot(0), _loadSlot(0),
      _showWarning(false), _pendingSlot(0), _pendingIsSave(false), _pendingNewSong(false) {
}

MainMenu::~MainMenu() {
}

void MainMenu::rebuildVisibleItems() {
    _visibleCount = 0;
    bool isExternal = (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL);

    _visibleItems[_visibleCount++] = MENU_CLOCK_SOURCE;

    // Internal BPM only makes sense when the internal clock drives the sequencer.
    if (!isExternal) {
        _visibleItems[_visibleCount++] = MENU_INTERNAL_BPM;
    }

    _visibleItems[_visibleCount++] = MENU_CLOCK_IO;

    // Sync Start only applies to incoming clock edges, so it is only
    // meaningful when the external clock is selected.
    if (isExternal) {
        _visibleItems[_visibleCount++] = MENU_SYNC_START;
    }

    _visibleItems[_visibleCount++] = MENU_AUTOPLAY;
    _visibleItems[_visibleCount++] = MENU_SONG_LENGTH;
    _visibleItems[_visibleCount++] = MENU_NEW_SONG;
    _visibleItems[_visibleCount++] = MENU_SAVE_SONG;
    _visibleItems[_visibleCount++] = MENU_LOAD_SONG;
    _visibleItems[_visibleCount++] = MENU_SAVE_AND_EXIT;
    _visibleItems[_visibleCount++] = MENU_EXIT_WO_SAVE;
}

void MainMenu::clampSelectionToVisible() {
    if (_visibleCount <= 0) {
        _selectedIndex = 0;
        _scrollOffset = 0;
        return;
    }
    if (_selectedIndex < 0) _selectedIndex = 0;
    if (_selectedIndex >= _visibleCount) _selectedIndex = _visibleCount - 1;

    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex >= _scrollOffset + VISIBLE_ROWS) {
        _scrollOffset = _selectedIndex - (VISIBLE_ROWS - 1);
    }
    if (_scrollOffset < 0) _scrollOffset = 0;
    int maxOffset = _visibleCount - VISIBLE_ROWS;
    if (maxOffset < 0) maxOffset = 0;
    if (_scrollOffset > maxOffset) _scrollOffset = maxOffset;
}

MainMenu::MenuItem MainMenu::currentItem() const {
    if (_visibleCount <= 0) return MENU_CLOCK_SOURCE;
    if (_selectedIndex < 0) return _visibleItems[0];
    if (_selectedIndex >= _visibleCount) return _visibleItems[_visibleCount - 1];
    return _visibleItems[_selectedIndex];
}

const char* MainMenu::itemLabel(MenuItem item) const {
    switch (item) {
        case MENU_CLOCK_SOURCE:  return "Clock Source";
        case MENU_INTERNAL_BPM:  return "Internal BPM";
        case MENU_CLOCK_IO:
            return (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL)
                ? "External PPQN"
                : "CLK out mod";
        case MENU_AUTOPLAY:      return "Autoplay";
        case MENU_SYNC_START:    return "Sync Start";
        case MENU_SONG_LENGTH:   return "Song Length";
        case MENU_NEW_SONG:      return "New Song";
        case MENU_SAVE_SONG:     return "Save Song";
        case MENU_LOAD_SONG:     return "Load Song";
        case MENU_SAVE_AND_EXIT: return "Save and Exit";
        case MENU_EXIT_WO_SAVE:  return "Exit w/o Saving";
        default:                 return "";
    }
}

void MainMenu::enter() {
    rebuildVisibleItems();
    _selectedIndex = 0;
    _scrollOffset = 0;
    _editing = true;
    _lastNavTime = 0;
    _lastJoystickMoveTime = 0;
    _joystickWasCentered = true;
    _lastEncPosA = _userInput.encoder_a.position;
    _lastEncPosB = _userInput.encoder_b.position;
    _exitRequested = false;
    _showWarning = false;
    _pendingNewSong = false;
    clampSelectionToVisible();
    loadCurrentValue();
}

void MainMenu::loadCurrentValue() {
    switch (currentItem()) {
        case MENU_CLOCK_SOURCE:
            _editValue = (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) ? 1 : 0;
            break;
        case MENU_INTERNAL_BPM:
            _editValue = GlobalSettings::internalBPM;
            break;
        case MENU_CLOCK_IO:
            _editValue = (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL)
                ? GlobalSettings::externalPPQN
                : GlobalSettings::clockOutDivider;
            break;
        case MENU_AUTOPLAY:
            _editValue = GlobalSettings::autoplay ? 1 : 0;
            break;
        case MENU_SYNC_START:
            _editValue = GlobalSettings::syncStart ? 1 : 0;
            break;
        case MENU_SONG_LENGTH:
            _editValue = _trackerApp->getSongLength();
            break;
        default:
            break;
    }
}

void MainMenu::update() {
    if (_showWarning) {
        handleWarning();
    } else {
        handleNavigation();
        handleEditing();
    }
}

void MainMenu::handleNavigation() {
    unsigned long now = millis();

    if (now - _lastNavTime < 100) return;

    int dy = 0;
    if (_userInput.joystick.y_position > 30) dy = -1;
    else if (_userInput.joystick.y_position < -30) dy = 1;

    if (dy != 0) {
        if (_joystickWasCentered || (now - _lastJoystickMoveTime) > 50) {
            int newIndex = _selectedIndex + dy;
            if (newIndex >= 0 && newIndex < _visibleCount) {
                applySetting();

                _selectedIndex = newIndex;
                clampSelectionToVisible();
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

void MainMenu::handleEditing() {
    long encPosA = _userInput.encoder_a.position;
    long encPosB = _userInput.encoder_b.position;
    bool encAChanged = (encPosA != _lastEncPosA);
    bool encBChanged = (encPosB != _lastEncPosB);

    if (encAChanged || encBChanged) {
        int deltaA = (encAChanged) ? ((encPosA > _lastEncPosA) ? 1 : -1) : 0;
        int deltaB = (encBChanged) ? ((encPosB > _lastEncPosB) ? 1 : -1) : 0;

        switch (currentItem()) {
            case MENU_INTERNAL_BPM:
                if (encAChanged) _editValue += deltaA * 10;
                if (encBChanged) _editValue += deltaB;
                if (_editValue < 300) _editValue = 300;
                if (_editValue > 3000) _editValue = 3000;
                _trackerApp->setInternalBPM(_editValue);
                break;

            case MENU_SONG_LENGTH:
                if (encAChanged) {
                    _editValue += deltaA;
                    if (_editValue < 1) _editValue = 1;
                    if (_editValue > 64) _editValue = 64;
                    _trackerApp->setSongLength(_editValue);
                }
                break;

            case MENU_CLOCK_SOURCE:
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    _trackerApp->setClockSource(_editValue == 1
                        ? GlobalSettings::SOURCE_EXTERNAL
                        : GlobalSettings::SOURCE_INTERNAL);

                    // The visible item list may change (Internal BPM appears
                    // or disappears). Rebuild and keep the cursor on this row.
                    MenuItem on = currentItem();
                    rebuildVisibleItems();
                    for (int i = 0; i < _visibleCount; i++) {
                        if (_visibleItems[i] == on) { _selectedIndex = i; break; }
                    }
                    clampSelectionToVisible();
                    loadCurrentValue();
                }
                break;

            case MENU_CLOCK_IO:
                if (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) {
                    if (encAChanged) {
                        _editValue = (_editValue + deltaA) % 5;
                        if (_editValue < 0) _editValue = 4;
                        _trackerApp->setExternalPPQN(_editValue);
                    }
                } else {
                    if (encAChanged) {
                        int n = SongData::getNumDividers();
                        _editValue = (_editValue + deltaA) % n;
                        if (_editValue < 0) _editValue = n - 1;
                        _trackerApp->setClockOutDivider(_editValue);
                    }
                }
                break;

            case MENU_AUTOPLAY:
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    _trackerApp->setAutoplay(_editValue == 1);
                }
                break;

            case MENU_SYNC_START:
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    _trackerApp->setSyncStart(_editValue == 1);
                }
                break;

            case MENU_SAVE_SONG:
                if (encAChanged) {
                    _saveSlot += deltaA;
                    if (_saveSlot < 0) _saveSlot = 7;
                    if (_saveSlot > 7) _saveSlot = 0;
                }
                break;

            case MENU_LOAD_SONG:
                if (encAChanged) {
                    _loadSlot += deltaA;
                    if (_loadSlot < 0) _loadSlot = 7;
                    if (_loadSlot > 7) _loadSlot = 0;
                }
                break;

            default:
                break;
        }

        _lastEncPosA = encPosA;
        _lastEncPosB = encPosB;
    }

    // Joystick click triggers actions
    if (_userInput.joystick_button.just_released) {
        MenuItem item = currentItem();
        if (item == MENU_NEW_SONG) {
            _pendingNewSong = true;
            _showWarning = true;
        } else if (item == MENU_SAVE_SONG) {
            _pendingIsSave = true;
            _pendingSlot = _saveSlot;
            _pendingNewSong = false;

            char filename[32];
            snprintf(filename, sizeof(filename), "/song%d.song", _pendingSlot + 1);

            if (_trackerApp->songExists(filename)) {
                _showWarning = true;
            } else {
                _trackerApp->saveCurrentProjectToSlot(_pendingSlot + 1);
                _exitRequested = true;
            }
        } else if (item == MENU_LOAD_SONG) {
            _pendingIsSave = false;
            _pendingSlot = _loadSlot;
            _pendingNewSong = false;
            _showWarning = true;
        } else if (item == MENU_SAVE_AND_EXIT) {
            GlobalSettings::save();
            _exitRequested = true;
        } else if (item == MENU_EXIT_WO_SAVE) {
            _exitRequested = true;
        } else {
            applySetting();
        }
    }
}

void MainMenu::handleWarning() {
    if (_userInput.encoder_a_button.just_released) {
        if (_pendingNewSong) {
            _trackerApp->newCurrentProject();
        } else if (_pendingIsSave) {
            _trackerApp->saveCurrentProjectToSlot(_pendingSlot + 1);
        } else {
            _trackerApp->loadCurrentProjectFromSlot(_pendingSlot + 1);
        }
        _showWarning = false;
        _exitRequested = true;
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }

    if (_userInput.encoder_b_button.just_released) {
        _showWarning = false;
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }

    if (_userInput.save_button.just_released) {
        _showWarning = false;
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }
}

void MainMenu::applySetting() {
    switch (currentItem()) {
        case MENU_CLOCK_SOURCE:
            _trackerApp->setClockSource(_editValue == 1
                ? GlobalSettings::SOURCE_EXTERNAL
                : GlobalSettings::SOURCE_INTERNAL);
            break;
        case MENU_INTERNAL_BPM:
            _trackerApp->setInternalBPM(_editValue);
            break;
        case MENU_CLOCK_IO:
            if (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) {
                _trackerApp->setExternalPPQN(_editValue);
            } else {
                _trackerApp->setClockOutDivider(_editValue);
            }
            break;
        case MENU_AUTOPLAY:
            _trackerApp->setAutoplay(_editValue == 1);
            break;
        case MENU_SYNC_START:
            _trackerApp->setSyncStart(_editValue == 1);
            break;
        default:
            break;
    }
}

void MainMenu::draw() {
    if (_showWarning) {
        drawWarning();
        return;
    }

    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    char valueBuffer[16];

    for (int i = 0; i < VISIBLE_ROWS && (_scrollOffset + i) < _visibleCount; i++) {
        int pos = _scrollOffset + i;
        MenuItem item = _visibleItems[pos];
        int y = i * 10;
        const char* label = itemLabel(item);
        const char* value = "";
        bool selected = (pos == _selectedIndex);

        if (item == MENU_SAVE_SONG) {
            snprintf(valueBuffer, sizeof(valueBuffer), "Song%d", _saveSlot + 1);
            value = valueBuffer;
        } else if (item == MENU_LOAD_SONG) {
            snprintf(valueBuffer, sizeof(valueBuffer), "Song%d", _loadSlot + 1);
            value = valueBuffer;
        } else if (selected) {
            switch (item) {
                case MENU_CLOCK_SOURCE:
                    value = (_editValue == 1) ? "Ext" : "Int";
                    break;
                case MENU_INTERNAL_BPM:
                    snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", _editValue / 10.0f);
                    value = valueBuffer;
                    break;
                case MENU_CLOCK_IO:
                    if (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) {
                        const char* ppqnValues[] = {"1", "2", "4", "24", "48"};
                        value = ppqnValues[_editValue];
                    } else {
                        value = SongData::getDividerTextByIndex(_editValue);
                    }
                    break;
                case MENU_AUTOPLAY:
                    value = _editValue ? "ON" : "OFF";
                    break;
                case MENU_SYNC_START:
                    value = _editValue ? "ON" : "OFF";
                    break;
                case MENU_SONG_LENGTH:
                    snprintf(valueBuffer, sizeof(valueBuffer), "%u", _editValue);
                    value = valueBuffer;
                    break;
                default:
                    value = "";
                    break;
            }
        } else {
            switch (item) {
                case MENU_CLOCK_SOURCE:
                    value = (GlobalSettings::clockSource == GlobalSettings::SOURCE_INTERNAL) ? "Int" : "Ext";
                    break;
                case MENU_INTERNAL_BPM:
                    snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", TrackerApp::getInternalBPMFloat());
                    value = valueBuffer;
                    break;
                case MENU_CLOCK_IO:
                    if (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) {
                        const char* ppqnValues[] = {"1", "2", "4", "24", "48"};
                        value = ppqnValues[GlobalSettings::externalPPQN];
                    } else {
                        value = SongData::getDividerTextByIndex(GlobalSettings::clockOutDivider);
                    }
                    break;
                case MENU_AUTOPLAY:
                    value = GlobalSettings::autoplay ? "ON" : "OFF";
                    break;
                case MENU_SYNC_START:
                    value = GlobalSettings::syncStart ? "ON" : "OFF";
                    break;
                case MENU_SONG_LENGTH:
                    snprintf(valueBuffer, sizeof(valueBuffer), "%u", _trackerApp->getSongLength());
                    value = valueBuffer;
                    break;
                default:
                    value = "";
                    break;
            }
        }

        if (selected) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(label, 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(label, 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
        }
    }
}

void MainMenu::drawWarning() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());

    if (_pendingNewSong) {
        _display.printAt("NEW SONG?", 2, 10, ALIGN_LEFT);
        _display.printAt("Current project", 2, 20, ALIGN_LEFT);
        _display.printAt("will be lost.", 2, 30, ALIGN_LEFT);
        _display.printAt("Continue?", 2, 40, ALIGN_LEFT);
    } else if (_pendingIsSave) {
        _display.printAt("OVERWRITE?", 2, 10, ALIGN_LEFT);
        _display.printAt("Song will be", 2, 20, ALIGN_LEFT);
        _display.printAt("overwritten.", 2, 30, ALIGN_LEFT);
        _display.printAt("Continue?", 2, 40, ALIGN_LEFT);
    } else {
        _display.printAt("WARNING!", 2, 10, ALIGN_LEFT);
        _display.printAt("Current project", 2, 20, ALIGN_LEFT);
        _display.printAt("will lose", 2, 30, ALIGN_LEFT);
        _display.printAt("unsaved data.", 2, 40, ALIGN_LEFT);
        _display.printAt("Continue?", 2, 50, ALIGN_LEFT);
    }

    _display.drawCircle(112, 15, 6, _display.colorWhite());
    _display.printAt("A", 110, 11, ALIGN_LEFT);
    _display.printAt("YES", 104, 23, ALIGN_LEFT);

    _display.drawCircle(112, 40, 6, _display.colorWhite());
    _display.printAt("B", 110, 37, ALIGN_LEFT);
    _display.printAt("NO", 107, 48, ALIGN_LEFT);
}

void MainMenu::saveAndExit() {
    GlobalSettings::save();
}