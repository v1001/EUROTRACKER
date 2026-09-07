#ifndef STEP_SEQUENCER_H
#define STEP_SEQUENCER_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "StepPattern.h"
#include "StepSequencerUI.h"
#include "Quantizer.h"
#include "EventQueue.h"

class StepSequencer {
public:
    StepSequencer(DisplayManager& display, UserInput& userInput);
    ~StepSequencer();
    
    void begin(StepPattern* pattern);
    void resetPosition();
    void update();

    void drawStatusBar() { _ui.drawStatusBar(); }
    void onTimerTick(uint16_t tickCount);
    void onStep();
    void processStepOutput();

    void onEnterUI();
    
    uint16_t getClockDivision() const { return _clockDivision; }
    bool getResetOnStep() const {return _resetOnStep; }
    uint8_t getSwingAmount() const {return _swingAmount; }
    
    uint16_t getCurrentDACValue();
    bool getCurrentGateOutput() const { return _currentGateOutput; }
    bool isGateOutputChanged() const { return _gateOutputChanged; }
    void clearGateOutputChanged() { _gateOutputChanged = false; }
    
    bool hasStepPending() const { return _stepPending; }
    
    void setClockDivision(uint16_t division);

    // Quantizer settings
    void setQuantizerEnabled(bool enabled);
    bool isQuantizerEnabled() const { return _quantizerEnabled; }
    void setQuantizerRef(Quantizer* quantizer) { _quantizer = quantizer; }
    void setMinCV(uint16_t minCV);
    void setMaxCV(uint16_t maxCV);
    uint16_t getMinCV() const { return _minCV; }
    uint16_t getMaxCV() const { return _maxCV; }

    void setResetOnStep(bool enabled) { _resetOnStep = enabled; }
    void setSwingAmount(uint8_t amount) { _swingAmount = amount; }
    void setStepDurationUs(uint64_t stepDurationUs) { _stepDurationUs = stepDurationUs; }
    void setGateQueue(GateQueue* queue) { _gateQueue = queue; }
    void setCVQueue(CVQueue* queue) { _cvQueue = queue; }
    
private:
    static const uint16_t MAX_CLOCK_DIVISION = 192;
    
    DisplayManager& _display;
    UserInput& _userInput;
    StepSequencerUI _ui;
    StepPattern* _pattern;
    
    uint8_t _currentStep;
    uint16_t _clockDivision;
    uint64_t _stepDurationUs;
    uint64_t _lastStepTime;
    uint64_t _lastTickTime;
    
    volatile bool _stepPending;
    
    uint16_t _currentDACValue;

    bool _currentGateOutput;
    bool _gateOutputChanged;
    
    uint64_t _lastEncoderAMove;
    uint64_t _lastEncoderBMove;
    long _lastEncAPos;
    long _lastEncBPos;
    
    void renderToDisplay();
    uint64_t calculateDurationUs(uint8_t gatePercent);
    uint8_t constrainValue(int value);

    // Quantizer
    bool _quantizerEnabled;
    Quantizer* _quantizer;  // Pointer to quantizer in SongData (not owned)
    uint16_t _minCV;   // 0-4095
    uint16_t _maxCV;   // 0-4095

    bool _resetOnStep;
    uint8_t _swingAmount;  // 0-100%

    // Event queue references (passed from SongSequencer)
    GateQueue* _gateQueue;
    CVQueue* _cvQueue;
};

#endif