#include "MainMenu.h"
#include "TrackerApp.h"

MainMenu::MainMenu(DisplayManager& display, UserInput& userInput, OutputHandler& outputHandler, TrackerApp* trackerApp)
    : _display(display), _userInput(userInput), _outputHandler(outputHandler), _trackerApp(trackerApp),
      _selectedIndex(0), _scrollOffset(0), _editing(false), _editValue(0),
      _lastNavTime(0), _lastJoystickMoveTime(0), _joystickWasCentered(true),
      _lastEncPosA(0), _lastEncPosB(0), _exitRequested(false), _saveSlot(0), _loadSlot(0),
      _showWarning(false), _pendingSlot(0), _pendingIsSave(false), _pendingNewSong(false) {
}

MainMenu::~MainMenu() {
}

void MainMenu::enter() {
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
    loadCurrentValue();
}

void MainMenu::loadCurrentValue() {
    switch (_selectedIndex) {
        case MENU_CLOCK_SOURCE:
            _editValue = (GlobalSettings::clockSource == GlobalSettings::SOURCE_EXTERNAL) ? 1 : 0;
            break;
        case MENU_INTERNAL_BPM:
            _editValue = GlobalSettings::internalBPM;   // scaled
            break;
        case MENU_EXTERNAL_PPQN:
            _editValue = GlobalSettings::externalPPQN;
            break;
        case MENU_AUTOPLAY:
            _editValue = GlobalSettings::autoplay ? 1 : 0;
            break;
        case MENU_SYNC_START:
            _editValue = GlobalSettings::syncStart ? 1 : 0;
            break;
        case MENU_JOYSTICK_SPEED:
            _editValue = GlobalSettings::joystickSpeed;
            break;
        case MENU_BRIGHTNESS:
            _editValue = GlobalSettings::brightness;
            break;
        case MENU_SONG_LENGTH:
            _editValue = _trackerApp->getSongLength();
            break;
        case MENU_NEW_SONG:
        case MENU_SAVE_SONG:
        case MENU_LOAD_SONG:
        case MENU_SAVE_AND_EXIT:
        case MENU_EXIT_WO_SAVE:
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
            if (newIndex >= 0 && newIndex <= MENU_EXIT_WO_SAVE) {
                applySetting();
                
                _selectedIndex = newIndex;
                if (_selectedIndex < _scrollOffset) {
                    _scrollOffset = _selectedIndex;
                } else if (_selectedIndex >= _scrollOffset + 6) {
                    _scrollOffset = _selectedIndex - 5;
                }
                
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

        // For BPM: encoder A = 1, encoder B = 0.1
        // For other items: encoder A = 1, encoder B is ignored (or can be used for fine adjustments if needed)
        switch (_selectedIndex) {
            case MENU_INTERNAL_BPM:
                if (encAChanged) {
                    _editValue += deltaA * 10;
                }
                if (encBChanged) {
                    _editValue += deltaB;
                }
                if (_editValue < 300) _editValue = 300;
                if (_editValue > 3000) _editValue = 3000;
                _trackerApp->setInternalBPM(_editValue);   // no division
                break;

            // For other numeric items: use encoder A only (or both with different steps if needed)
            case MENU_JOYSTICK_SPEED:
                if (encAChanged) {
                    _editValue += deltaA;
                    if (_editValue < 1) _editValue = 1;
                    if (_editValue > 10) _editValue = 10;
                    _trackerApp->setJoystickSpeed(_editValue);
                }
                break;
            case MENU_BRIGHTNESS:
                if (encAChanged) {
                    _editValue += deltaA;
                    if (_editValue < 0) _editValue = 0;
                    if (_editValue > 255) _editValue = 255;
                    _trackerApp->setBrightness(_editValue);
                }
                break;
            case MENU_SONG_LENGTH:
                if (encAChanged) {
                    _editValue += deltaA;
                    if (_editValue < 1) _editValue = 1;
                    if (_editValue > 64) _editValue = 64;
                    _trackerApp->setSongLength(_editValue);
                }
                break;
            // Clock source, PPQN, autoplay, sync start: toggle with encoder A only
            case MENU_CLOCK_SOURCE:
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 2;
                    if (_editValue < 0) _editValue = 1;
                    _trackerApp->setClockSource(_editValue == 1 ? GlobalSettings::SOURCE_EXTERNAL : GlobalSettings::SOURCE_INTERNAL);
                }
                break;
            case MENU_EXTERNAL_PPQN:
                if (encAChanged) {
                    _editValue = (_editValue + deltaA) % 5;
                    if (_editValue < 0) _editValue = 4;
                    _trackerApp->setExternalPPQN(_editValue);
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
            // Save/Load slots: use encoder A only
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
        if (_selectedIndex == MENU_NEW_SONG) {
            _pendingNewSong = true;
            _showWarning = true;
        }else if (_selectedIndex == MENU_SAVE_SONG) {
            _pendingIsSave = true;
            _pendingSlot = _saveSlot;
            _pendingNewSong = false;   // clear any stale new song flag
            
            char filename[32];
            sprintf(filename, "/song%d.song", _pendingSlot + 1);
            
            if (_trackerApp->songExists(filename)) {
                _showWarning = true;
            } else {
                _trackerApp->saveCurrentProjectToSlot(_pendingSlot + 1);
                _exitRequested = true;
            }
        } else if (_selectedIndex == MENU_LOAD_SONG) {
            _pendingIsSave = false;
            _pendingSlot = _loadSlot;
            _pendingNewSong = false;   // clear any stale new song flag
            _showWarning = true;
        } else if (_selectedIndex == MENU_SAVE_AND_EXIT) {
            GlobalSettings::save();
            _exitRequested = true;
        } else if (_selectedIndex == MENU_EXIT_WO_SAVE) {
            _exitRequested = true;
        } else {
            applySetting();
        }
    }
}

void MainMenu::handleWarning() {
    // Check for encoder A button press (YES)
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
        // Clear all pending flags
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }
    
    // Check for encoder B button press (NO)
    if (_userInput.encoder_b_button.just_released) {
        _showWarning = false;
        // Clear all pending flags when dismissing
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }
    
    // Save button also cancels
    if (_userInput.save_button.just_released) {
        _showWarning = false;
        // Clear all pending flags when dismissing
        _pendingNewSong = false;
        _pendingIsSave = false;
        _pendingSlot = 0;
        return;
    }
}

void MainMenu::applySetting() {
    switch (_selectedIndex) {
        case MENU_CLOCK_SOURCE:
            _trackerApp->setClockSource(_editValue == 1 ? 
                GlobalSettings::SOURCE_EXTERNAL : GlobalSettings::SOURCE_INTERNAL);
            break;
        case MENU_INTERNAL_BPM:
            _trackerApp->setInternalBPM(_editValue);
            break;
        case MENU_EXTERNAL_PPQN:
            _trackerApp->setExternalPPQN(_editValue);
            break;
        case MENU_AUTOPLAY:
            _trackerApp->setAutoplay(_editValue == 1);
            break;
        case MENU_SYNC_START:
            _trackerApp->setSyncStart(_editValue == 1);
            break;
        case MENU_JOYSTICK_SPEED:
            _trackerApp->setJoystickSpeed(_editValue);
            break;
        case MENU_BRIGHTNESS:
            // Already applied live
            break;
        case MENU_SONG_LENGTH:
            // Already applied live
            break;
        case MENU_NEW_SONG:
        case MENU_SAVE_SONG:
        case MENU_LOAD_SONG:
        case MENU_SAVE_AND_EXIT:
        case MENU_EXIT_WO_SAVE:
            // Handled elsewhere
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
    
    const char* items[] = {
        "Clock Source",
        "Internal BPM",
        "External PPQN",
        "Autoplay",
        "Sync Start",
        "Joystick Speed",
        "Brightness",
        "Song Length",
        "New Song",
        "Save Song",
        "Load Song",
        "Save and Exit",
        "Exit w/o Saving"
    };
    
    char valueBuffer[16];
    for (int i = 0; i < 6 && (_scrollOffset + i) <= MENU_EXIT_WO_SAVE; i++) {
        int idx = _scrollOffset + i;
        int y = i * 10;
        
        const char* value = "";
        
        // Determine display value based on item type
        if (idx == MENU_SAVE_SONG) {
            sprintf(valueBuffer, "Song%d", _saveSlot + 1);
            value = valueBuffer;
        } else if (idx == MENU_LOAD_SONG) {
            sprintf(valueBuffer, "Song%d", _loadSlot + 1);
            value = valueBuffer;
        } else if (idx == _selectedIndex) {
            // Selected editable item - show edit buffer
            switch (idx) {
                case MENU_CLOCK_SOURCE:
                    value = (_editValue == 1) ? "Ext" : "Int";
                    break;
                case MENU_INTERNAL_BPM:
                    // Show float value with one decimal
                    sprintf(valueBuffer, "%.1f", _editValue / 10.0f);
                    value = valueBuffer;
                    break;
                case MENU_EXTERNAL_PPQN: {
                    const char* ppqnValues[] = {"1", "2", "4", "24", "48"};
                    value = ppqnValues[_editValue];
                    break;
                }
                case MENU_AUTOPLAY:
                    value = _editValue ? "ON" : "OFF";
                    break;
                case MENU_SYNC_START:
                    value = _editValue ? "ON" : "OFF";
                    break;
                case MENU_JOYSTICK_SPEED:
                    sprintf(valueBuffer, "%d", _editValue);
                    value = valueBuffer;
                    break;
                case MENU_BRIGHTNESS:
                    sprintf(valueBuffer, "%d", _editValue);
                    value = valueBuffer;
                    break;
                case MENU_SONG_LENGTH:
                    sprintf(valueBuffer, "%d", _editValue);
                    value = valueBuffer;
                    break;
                default:
                    value = "";
                    break;
            }
        } else {
            // Non-selected item - show actual saved value
            switch (idx) {
                case MENU_CLOCK_SOURCE:
                    value = (GlobalSettings::clockSource == GlobalSettings::SOURCE_INTERNAL) ? "Int" : "Ext";
                    break;
                case MENU_INTERNAL_BPM:
                    sprintf(valueBuffer, "%.1f", TrackerApp::getInternalBPMFloat());
                    value = valueBuffer;
                    break;
                case MENU_EXTERNAL_PPQN: {
                    const char* ppqnValues[] = {"1", "2", "4", "24", "48"};
                    value = ppqnValues[GlobalSettings::externalPPQN];
                    break;
                }
                case MENU_AUTOPLAY:
                    value = GlobalSettings::autoplay ? "ON" : "OFF";
                    break;
                case MENU_SYNC_START:
                    value = GlobalSettings::syncStart ? "ON" : "OFF";
                    break;
                case MENU_JOYSTICK_SPEED:
                    sprintf(valueBuffer, "%d", GlobalSettings::joystickSpeed);
                    value = valueBuffer;
                    break;
                case MENU_BRIGHTNESS:
                    sprintf(valueBuffer, "%d", GlobalSettings::brightness);
                    value = valueBuffer;
                    break;
                case MENU_SONG_LENGTH:
                    sprintf(valueBuffer, "%d", _trackerApp->getSongLength());
                    value = valueBuffer;
                    break;
                default:
                    value = "";
                    break;
            }
        }
        
        if (idx == _selectedIndex) {
            _display.fillRect(0, y - 2, 128, 10, _display.colorWhite());
            _display.setTextColor(_display.colorBlack());
            _display.printAt(items[idx], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
            _display.setTextColor(_display.colorWhite());
        } else {
            _display.printAt(items[idx], 2, y, ALIGN_LEFT);
            if (strlen(value) > 0) {
                _display.printAt(value, 96, y, ALIGN_LEFT);
            }
        }
    }
}

void MainMenu::drawWarning() {
    _display.setTextSize(TEXT_SMALL);
    _display.setTextColor(_display.colorWhite());
    
    // Warning text on the left side
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
    
    // Encoder A (YES) - circle with "A"
    _display.drawCircle(112, 15, 6, _display.colorWhite());
    _display.printAt("A", 110, 11, ALIGN_LEFT);
    _display.printAt("YES", 104, 23, ALIGN_LEFT);
    
    // Encoder B (NO) - circle with "B"
    _display.drawCircle(112, 40, 6, _display.colorWhite());
    _display.printAt("B", 110, 37, ALIGN_LEFT);
    _display.printAt("NO", 107, 48, ALIGN_LEFT);
}

void MainMenu::saveAndExit() {
    GlobalSettings::save();
}