#ifndef STEP_SEQUENCER_UI_H
#define STEP_SEQUENCER_UI_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "StepPattern.h"
#include "Quantizer.h"

// Forward declaration to avoid circular include
class StepSequencer;

class StepSequencerUI {
public:
    StepSequencerUI(DisplayManager& display, UserInput& userInput);
    ~StepSequencerUI();
    
    // Now takes both pattern and the owning StepSequencer
    void begin(StepPattern* pattern, StepSequencer* sequencer);
    void update();
    void draw();
    void drawStatusBar();
    
    void setCurrentStep(uint8_t step) { _currentStep = step; }
    void setQuantizer(Quantizer* quantizer) { _quantizer = quantizer; }
    void setQuantizerEnabled(bool enabled) { _quantizerEnabled = enabled; }
    
    uint8_t getSelectedStep() const { return _selectedStep; }
    bool isCursorVisible() const { return _cursorVisible; }
    uint8_t getDisplayMode() const { return _displayMode; }
    uint8_t getEditModeEncA() const { return _editModeEncA; }
    uint8_t getEditModeEncB() const { return _editModeEncB; }
    
    void setDisplayMode(uint8_t mode) { _displayMode = mode; }
    void setEditModeEncA(uint8_t mode) { _editModeEncA = mode; }
    void setEditModeEncB(uint8_t mode) { _editModeEncB = mode; }
    
    void enter();
    
private:
    static const int ROWS = 4;
    static const int COLS = 8;
    static const int TOTAL_STEPS = 32;
    static const int RECT_WIDTH = 14;
    static const int RECT_HEIGHT = 10;
    static const int CELL_SPACING_X = 16;
    static const int CELL_SPACING_Y = 12;
    static const int STATUS_BAR_Y = 54;
    static const int MOVE_DELAY = 200;
    static const int CURSOR_BLINK_INTERVAL = 300;

    struct CopiedStep {
        bool on;
        uint16_t cv;
        uint8_t probability;
        uint8_t gateLength;
        uint8_t decay;
        uint8_t attack;
        uint8_t ratchet;
        uint8_t microtiming;
        bool hasData;
    };
    
    DisplayManager& _display;
    UserInput& _userInput;
    StepPattern* _pattern;
    StepSequencer* _sequencer;        // Added to access min/max CV
    
    uint8_t _selectedStep;
    uint8_t _currentStep;
    uint8_t _displayMode;   // 0=CV, 2=DECAY, 4=ATTACK, 1=PROB, 3=GATE, 5=RATCHET, 7=MICROTIMING
    uint8_t _editModeEncA;  // 0=CV, 1=DECAY, 2=ATTACK
    uint8_t _editModeEncB;  // 0=PROB, 1=GATE, 2=RATCHET, 3=MICROTIMING
    bool _cursorVisible;

    // Copy‑paste state
    CopiedStep _copiedStep;
    bool _copyTriggered;
    
    uint64_t _lastCursorBlink;
    uint64_t _lastMoveTime;
    uint64_t _lastNavTime;
    uint64_t _lastJoystickMoveTime;
    bool _joystickWasCentered;
    const uint64_t NAV_DELAY_MS = 100;
    const uint64_t JOYSTICK_CONTINUOUS_DELAY = 50;
    
    long _lastEncoderAPos;
    long _lastEncoderBPos;
    uint64_t _lastEncoderAMove;
    uint64_t _lastEncoderBMove;
    
    void getStepPosition(uint8_t step, int& x, int& y);
    void drawStep(uint8_t step, int x, int y);
    void handleJoystick();
    void handleEncoders();
    void handleButtons();
    void blinkCursor();
    uint8_t constrainValue(int value);

    // CV conversion helpers
    uint8_t mapRawToPercent(uint16_t raw) const;
    uint16_t mapPercentToRaw(uint8_t percent) const;

    Quantizer* _quantizer;
    bool _quantizerEnabled;

    // Returns note name if quantizer ON, else percentage string
    const char* getQuantizedNoteNameOrCV(uint16_t cvValue);

    void resetEncoderTracking();

    // Pop-up overlay
    bool     _popupActive;
    uint8_t  _popupParam;
    uint16_t _popupValue;
    uint64_t _popupLastEdit;

    void drawPopup();
    void setPopup(uint8_t param, uint16_t value);
};

#endif