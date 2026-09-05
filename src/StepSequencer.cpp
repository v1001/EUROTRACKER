#include "StepSequencer.h"

StepSequencer::StepSequencer(DisplayManager& display, UserInput& userInput)
    : _display(display), _userInput(userInput), _ui(display, userInput), _pattern(nullptr),
      _currentStep(0), _clockDivision(48), _stepDurationUs(125000), _lastStepTime(0),
      _stepPending(false), _currentDACValue(0), _currentGateOutput(false), _gateOutputChanged(false),
      _lastEncoderAMove(0), _lastEncoderBMove(0), _lastEncAPos(0), _lastEncBPos(0),
      _quantizerEnabled(true), _minCV(0), _maxCV(4095), _quantizer(nullptr), _lastTickTime(0),
      _gateQueue(nullptr), _cvQueue(nullptr), _resetOnStep(true), _swingAmount(0) {
}

StepSequencer::~StepSequencer() {
}

void StepSequencer::begin(StepPattern* pattern) {
    _pattern = pattern;
    _ui.begin(pattern, this);
}

void StepSequencer::setQuantizerEnabled(bool enabled) {
    _quantizerEnabled = enabled;
}

void StepSequencer::resetPosition() {
    _currentStep = 0;
}

void StepSequencer::update() {
    if (!_pattern) return;
    // Pass quantizer info to UI (by pointer)
    _ui.setQuantizer(_quantizerEnabled ? _quantizer : nullptr);
    _ui.setQuantizerEnabled(_quantizerEnabled);

    _ui.update();
    
    // Update UI with current step and duration for display
    _ui.setCurrentStep(_currentStep);
    
    renderToDisplay();
    _ui.drawStatusBar();
}

void StepSequencer::setClockDivision(uint16_t division) {
    if (division >= 1 && division <= MAX_CLOCK_DIVISION) {
        _clockDivision = division;
    }
}

uint8_t StepSequencer::constrainValue(int value) {
    if (value < 0) return 0;
    if (value > 99) return 99;
    return (uint8_t)value;
}

uint64_t StepSequencer::calculateDurationUs(uint8_t gatePercent) {
    const uint64_t MIN_GATE = 120;
    uint64_t halfStep = _stepDurationUs / 2;

    if (gatePercent == 0) return MIN_GATE;
    if (gatePercent <= 32) return ((uint64_t)gatePercent * halfStep) / 32;
    if (gatePercent <= 96) return halfStep + ((uint64_t)(gatePercent - 32) * halfStep);
    if (gatePercent == 97) return 128 * halfStep;
    if (gatePercent == 98) return 256 * halfStep;
    return 512 * halfStep;
}

uint16_t StepSequencer::getCurrentDACValue() {
    return _currentDACValue;
}

void StepSequencer::onTimerTick(uint16_t tickCount) {
    uint64_t now = micros();
    
    if (_lastTickTime != 0) {
        _stepDurationUs = (now - _lastTickTime) * _clockDivision;
    }
    _lastTickTime = now;
    if (tickCount % _clockDivision == 0) {
        _stepPending = true;
    }
}

void StepSequencer::onStep() {
    if (!_pattern) return;
    
    _stepPending = false;
    
    // Process current step (starts at 0)
    uint64_t now = micros();
    uint8_t stepIndex = _currentStep;

    // Apply swing delay to odd-numbered steps (1,3,5,7...)
    uint64_t stepStartTime = now;
    uint8_t microtimingValue = _pattern->getMicrotiming(stepIndex);
    uint8_t ratchet = _pattern->getRatchet(stepIndex);
    if (microtimingValue > 0) {
        uint64_t microtimingOffset = _stepDurationUs * microtimingValue / 100;
        stepStartTime += microtimingOffset / ratchet;
    } else if ((stepIndex % 2) == 1 && _swingAmount > 0) {
        uint64_t swingDelay = (_stepDurationUs * _swingAmount) / 200;
        stepStartTime += swingDelay;
    }
    
    // Determine if step triggers (single probability check)
    bool stepOn = _pattern->getOn(stepIndex);
    uint8_t probability = _pattern->getProbability(stepIndex);
    bool triggers = stepOn && (random(0, 99) < probability);
    
    // ===== GATE QUEUE =====
    if (_gateQueue) {
        uint64_t oldGateOffTimestamp = 0;
        
        if (_resetOnStep) {
            while (!_gateQueue->isEmpty()) {
                _gateQueue->pop();
            }
        } else {
            while (!_gateQueue->isEmpty()) {
                const GateEvent& event = _gateQueue->peek();
                if (event.type == GateEvent::GATE_OFF) {
                    if (event.timestamp > oldGateOffTimestamp) {
                        oldGateOffTimestamp = event.timestamp;
                    }
                }
                _gateQueue->pop();
            }
        }
        
        if (triggers) {
            uint8_t gatePercent = _pattern->getGateLength(stepIndex);
            uint64_t gateDuration = calculateDurationUs(gatePercent);
            
            _gateQueue->push(stepStartTime, GateEvent::GATE_ON);
            
            uint64_t newGateOffTime = stepStartTime + gateDuration;
            
            if (!_resetOnStep && (oldGateOffTimestamp > newGateOffTime)) {
                _gateQueue->push(oldGateOffTimestamp, GateEvent::GATE_OFF);
            } else {
                _gateQueue->push(newGateOffTime, GateEvent::GATE_OFF);
            }

            uint64_t stepDuration = _stepDurationUs;
            if (stepDuration == 0) stepDuration = 250000;
            uint64_t firstHitTime = stepStartTime;
            uint64_t ratchetSpacing = stepDuration / ratchet;

            for (uint8_t hit = 1; hit < ratchet; hit++) {
                if (random(0, 99) >= probability) continue;
                
                uint64_t hitTime = firstHitTime + (hit * ratchetSpacing);
                
                _gateQueue->push(hitTime, GateEvent::GATE_ON);
                
                uint64_t gateOffTime = hitTime + ratchetSpacing / 2;
                if (gateOffTime > firstHitTime + stepDuration) {
                    gateOffTime = firstHitTime + stepDuration;
                }
                
                if (!_resetOnStep && (hit == ratchet - 1)) {
                    if (oldGateOffTimestamp > gateOffTime) {
                        gateOffTime = oldGateOffTimestamp;
                    }
                }
                _gateQueue->push(gateOffTime, GateEvent::GATE_OFF);
            }
        } else if (!_resetOnStep && oldGateOffTimestamp > 0) {
            _gateQueue->push(oldGateOffTimestamp, GateEvent::GATE_OFF);
        }
    }
    
    // ===== CV QUEUE ===== (tracks 0-3 only)
    if (_cvQueue && triggers) {
        while (!_cvQueue->isEmpty()) {
            _cvQueue->pop();
        }
        
        // CV is already a raw 16-bit DAC value (0-4095)
        // Quantization happens during editing, not here
        uint16_t targetDAC = _pattern->getCV(stepIndex);
        uint8_t attack = _pattern->getAttack(stepIndex);
        uint8_t decay = _pattern->getDecay(stepIndex);
        
        uint64_t attackUs = calculateDurationUs(attack);
        uint64_t decayUs = calculateDurationUs(decay);
        
        if (_resetOnStep) {
            if (attack == 0 && decay == 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            } else if (attack > 0 && decay == 0) {
                _cvQueue->push(stepStartTime, 0, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
            } else if (attack == 0 && decay > 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + decayUs, 0, CVEvent::CV_SMOOTH);
            } else {
                _cvQueue->push(stepStartTime, 0, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
                _cvQueue->push(stepStartTime + attackUs + decayUs, 0, CVEvent::CV_SMOOTH);
            }
        } else {
            if (attack == 0 && decay == 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            } else if (targetDAC > _currentDACValue && attack > 0) {
                _cvQueue->push(stepStartTime, _currentDACValue, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
            } else if (targetDAC < _currentDACValue && decay > 0) {
                _cvQueue->push(stepStartTime, _currentDACValue, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + decayUs, targetDAC, CVEvent::CV_SMOOTH);
            } else {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            }
        }
    }
    
    // Advance to next step
    _currentStep++;
    if (_currentStep >= _pattern->getNumSteps()) {
        _currentStep = 0;
    }
}

void StepSequencer::processStepOutput() {
    uint64_t now = micros();
    static uint16_t lastValue = 0;
    static uint64_t lastTimestamp = 0;
    
    // ===== GATE OUTPUT =====
    if (_gateQueue) {
        while (!_gateQueue->isEmpty() && _gateQueue->peek().timestamp <= now) {
            const GateEvent& event = _gateQueue->peek();
            bool newGateState = (event.type == GateEvent::GATE_ON);
            
            if (newGateState != _currentGateOutput) {
                _currentGateOutput = newGateState;
                _gateOutputChanged = true;
            }
            
            _gateQueue->pop();
        }
    }
    
    // ===== CV OUTPUT =====
    if (_cvQueue) {
        while (!_cvQueue->isEmpty() && _cvQueue->peek().timestamp <= now) {
            const CVEvent& event = _cvQueue->peek();
            _currentDACValue = event.value;
            lastValue = event.value;
            lastTimestamp = now;
            _cvQueue->pop();
        }
        
        if (!_cvQueue->isEmpty()) {
            const CVEvent& nextEvent = _cvQueue->peek();
            
            if (nextEvent.transitionMode == CVEvent::CV_SMOOTH && nextEvent.timestamp > now) {
                if (lastTimestamp > 0 && lastTimestamp < nextEvent.timestamp) {
                    uint64_t totalDuration = nextEvent.timestamp - lastTimestamp;
                    uint64_t elapsed = now - lastTimestamp;
                    
                    if (elapsed < totalDuration) {
                        float progress = (float)elapsed / (float)totalDuration;
                        uint16_t interpolatedValue = lastValue + 
                            (uint16_t)(progress * (nextEvent.value - lastValue));
                        
                        if (interpolatedValue != _currentDACValue) {
                            _currentDACValue = interpolatedValue;
                        }
                    }
                }
            }
        }
    }
}

void StepSequencer::renderToDisplay() {
    _ui.draw();
}

void StepSequencer::onEnterUI() {
    _ui.enter();
}

void StepSequencer::setMinCV(uint16_t minCV) {
    if (minCV <= _maxCV) {
        _minCV = minCV;
    }
}

void StepSequencer::setMaxCV(uint16_t maxCV) {
    if (maxCV >= _minCV) {
        _maxCV = maxCV;
    }
}