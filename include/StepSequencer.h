#ifndef STEP_SEQUENCER_H
#define STEP_SEQUENCER_H

#include "DisplayManager.h"
#include "UserInput.h"
#include "StepPattern.h"
#include "StepSequencerUI.h"
#include "Quantizer.h"
#include "EventQueue.h"

// Forward declarations
struct PerformanceState;
class SongData;

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

    uint64_t getStepDurationUs() const { return _stepDurationUs; }
    uint64_t calculateDurationUs(uint8_t value) const;
    uint16_t getGateDurationMs(uint8_t gatePercent) const;
    
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

    // Performance Mode
    void setPerformanceState(PerformanceState* state, SongData* songData, int trackIndex) {
        _perfState = state;
        _songData = songData;
        _trackIndex = trackIndex;
    }

    struct CopiedStep {
        bool on;
        uint16_t cv;
        uint8_t probability;
        uint8_t gateLength;
        uint8_t decay;
        uint8_t attack;
        uint8_t ratchet;
        uint8_t microtiming;
        uint8_t sourceStep;
        uint8_t sourceTrack;
        bool hasData;
    };

    static void copyStep(const StepPattern* pattern, uint8_t stepIndex, uint8_t trackIndex);
    static void pasteStep(StepPattern* pattern, uint8_t stepIndex);
    static bool hasCopiedStep() { return _copiedStep.hasData; }
    static uint8_t getCopiedSourceStep() { return _copiedStep.sourceStep; }
    static uint8_t getCopiedSourceTrack() { return _copiedStep.sourceTrack; }
    static void clearClipboard() { _copiedStep.hasData = false; }
    
private:
    static const uint16_t MAX_CLOCK_DIVISION = 192;
    static CopiedStep _copiedStep;
    
    DisplayManager& _display;
    UserInput& _userInput;
    StepSequencerUI _ui;
    StepPattern* _pattern;
    
    uint8_t _currentStep;
    uint16_t _clockDivision;
    uint64_t _stepDurationUs;
    uint64_t _lastStepTime;
    uint64_t _lastTickTime;

    uint16_t _lastCVEventValue;
    uint64_t _lastCVEventTimestamp;
    
    volatile bool _stepPending;
    
    uint16_t _currentDACValue;
    bool _currentGateOutput;
    bool _gateOutputChanged;
    
    uint64_t _lastEncoderAMove;
    uint64_t _lastEncoderBMove;
    long _lastEncAPos;
    long _lastEncBPos;
    
    void renderToDisplay();
    uint8_t constrainValue(int value);

    bool _quantizerEnabled;
    Quantizer* _quantizer;
    uint16_t _minCV;
    uint16_t _maxCV;

    bool _resetOnStep;
    uint8_t _swingAmount;

    GateQueue* _gateQueue;
    CVQueue* _cvQueue;

    // Performance Mode
    PerformanceState* _perfState = nullptr;
    SongData* _songData = nullptr;
    int _trackIndex = 0;

    struct StepEffects {
        uint16_t cv;
        uint8_t gatePercent;
        uint8_t probability;
        uint8_t ratchet;
        uint8_t attack;
        uint8_t decay;
        bool trigger;
    };

    StepEffects _lastPlayedStep = {0, 0, 0, 1, 0, 0, false};
    uint8_t _skipCounter = 0;
    uint8_t _repeatCounter = 0;

    void processEffects(StepEffects& effects);
    void applyEffect(uint8_t eff, int8_t mag, int joy, StepEffects& effects);
};

#endif