#include "StepSequencer.h"
#include "SongSequencer.h"
#include "SongData.h"

StepSequencer::StepSequencer(DisplayManager& display, UserInput& userInput)
    : _display(display), _userInput(userInput), _ui(display, userInput), _pattern(nullptr),
      _currentStep(0), _clockDivision(48), _stepDurationUs(125000), _lastStepTime(0),
      _stepPending(false), _currentDACValue(0), _currentGateOutput(false), _gateOutputChanged(false),
      _lastEncoderAMove(0), _lastEncoderBMove(0), _lastEncAPos(0), _lastEncBPos(0),
      _quantizerEnabled(true), _minCV(0), _maxCV(4095), _quantizer(nullptr), _lastTickTime(0),
      _gateQueue(nullptr), _cvQueue(nullptr), _resetOnStep(false), _swingAmount(0),
      _lastCVEventValue(0), _lastCVEventTimestamp(0) {
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
    _lastCVEventValue = 0;
    _lastCVEventTimestamp = 0;
    _lastPlayedStep.trigger = false;
    _skipCounter = 0;
    _repeatCounter = 0;
}

void StepSequencer::update() {
    if (!_pattern) return;
    _ui.setQuantizer(_quantizerEnabled ? _quantizer : nullptr);
    _ui.setQuantizerEnabled(_quantizerEnabled);

    _ui.update();
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

uint64_t StepSequencer::calculateDurationUs(uint8_t value) const {
    if (value == 0) return 0;

    uint64_t num;
    if (value <= 24) {
        num = (value * 32) / 24;
    } else if (value <= 48) {
        num = 32 + (value - 24) * 224 / 24;
    } else if (value <= 72) {
        num = 256 + (value - 48) * 32;
    } else if (value <= 96) {
        num = 1024 + (value - 72) * 128;
    } else if (value == 97) {
        num = 5120;
    } else if (value == 98) {
        num = 6656;
    } else {
        num = 8192;
    }

    return (_stepDurationUs * num) / 128;
}

uint16_t StepSequencer::getGateDurationMs(uint8_t gatePercent) const {
    uint64_t us = calculateDurationUs(gatePercent);
    return (uint16_t)(us / 1000);
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

void StepSequencer::applyEffect(uint8_t eff, int8_t mag, int joy, StepEffects& effects) {
    if (eff == 0 || mag == 0) return;

    // Scale: -4.0 to +4.0
    float scaled_joy = (joy / 99.0f) * mag;

    if (eff == 1) { // Transpose
        int32_t offset = (int32_t)(scaled_joy * 205);
        int32_t newCV = effects.cv + offset;
        if (newCV < 0) newCV = 0;
        if (newCV > 4095) newCV = 4095;
        effects.cv = (uint16_t)newCV;
    } 
    else if (eff == 2) { // Slide
        int offset = (int)(abs(scaled_joy) * 24);
        if (scaled_joy > 0) {
            effects.attack = constrainValue(effects.attack + offset);
        } else if (scaled_joy < 0) {
            effects.decay = constrainValue(effects.decay + offset);
        }
    } 
    else if (eff == 3) { // Random
        if (scaled_joy > 0) {
            float chance = (scaled_joy / 4.0f) * 100.0f;
            if (random(0, 100) < chance) {
                int32_t offset = random(-205 * abs(mag), 205 * abs(mag) + 1);
                int32_t newCV = effects.cv + offset;
                if (newCV < 0) newCV = 0;
                if (newCV > 4095) newCV = 4095;
                effects.cv = (uint16_t)newCV;
                
                effects.ratchet = random(2, abs(mag) + 2);
            }
        } else if (scaled_joy < 0) {
            float reduction = (abs(scaled_joy) / 4.0f) * 0.75f;
            float newProb = effects.probability * (1.0f - reduction);
            effects.probability = constrain((int)newProb, 1, 100);
        }
    } 
    else if (eff == 4) { // Density
        int n_max = round(1 + 3 * (abs(mag) / 4.0f));
        
        if (scaled_joy > 0) {
            // High Density: Repeat steps, reduce gate time
            int n = map((long)(scaled_joy * 100), 1, 400, n_max, 1);
            n = constrain(n, 1, n_max);
            
            if (effects.trigger) {
                _repeatCounter = 0;
            } else {
                _repeatCounter++;
                if (_lastPlayedStep.trigger && (_repeatCounter % n) == 0) {
                    effects = _lastPlayedStep;
                    effects.trigger = true;
                }
            }
            
            // Reduce gate on time (up to 75% reduction at full push)
            if (effects.trigger) {
                int reduction = (int)((scaled_joy / 4.0f) * 75.0f);
                effects.gatePercent = effects.gatePercent - (effects.gatePercent * reduction / 100);
                if (effects.gatePercent < 1) effects.gatePercent = 1;
            }
            
        } else if (scaled_joy < 0) {
            // Low Density: Skip steps, increase gate time
            int n = map((long)(abs(scaled_joy) * 100), 1, 400, 1, n_max);
            n = constrain(n, 1, n_max);
            
            if (effects.trigger) {
                _skipCounter++;
                if ((_skipCounter % n) != 0) {
                    effects.trigger = false;
                }
            }
            
            // Increase gate on time (up to 75% increase at full pull)
            if (effects.trigger) {
                int increase = (int)((abs(scaled_joy) / 4.0f) * 75.0f);
                effects.gatePercent = effects.gatePercent + (effects.gatePercent * increase / 100);
                if (effects.gatePercent > 99) effects.gatePercent = 99;
            }
        }
    }
}

void StepSequencer::processEffects(StepEffects& effects) {
    if (!_perfState || !_perfState->active) return;

    // Mute check
    if (_perfState->trackMuted[_trackIndex]) {
        effects.trigger = false;
        return;
    }

    // Apply X axis
    if (_songData) {
        uint8_t effX = _songData->getEffectX(_perfState->activeBank, _trackIndex);
        int8_t magX = _songData->getMagX(_perfState->activeBank, _trackIndex);
        applyEffect(effX, magX, _perfState->joyX, effects);
    }

    // Apply Y axis
    if (_songData) {
        uint8_t effY = _songData->getEffectY(_perfState->activeBank, _trackIndex);
        int8_t magY = _songData->getMagY(_perfState->activeBank, _trackIndex);
        applyEffect(effY, magY, _perfState->joyY, effects);
    }
}

void StepSequencer::onStep() {
    if (!_pattern) return;
    
    _stepPending = false;
    
    uint64_t now = micros();
    uint8_t stepIndex = _currentStep;

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
    
    bool stepOn = _pattern->getOn(stepIndex);
    uint8_t probability = _pattern->getProbability(stepIndex);
    bool triggers = stepOn && (random(0, 99) < probability);
    
    // ===== APPLY PERFORMANCE EFFECTS =====
    StepEffects effects;
    effects.cv = _pattern->getCV(stepIndex);
    effects.gatePercent = _pattern->getGateLength(stepIndex);
    effects.probability = probability;
    effects.ratchet = ratchet;
    effects.attack = _pattern->getAttack(stepIndex);
    effects.decay = _pattern->getDecay(stepIndex);
    effects.trigger = triggers;
    uint8_t baseGate = effects.gatePercent;

    processEffects(effects);

    // Update local variables with effects
    triggers = effects.trigger;
    ratchet = effects.ratchet;
    uint8_t gatePercent = effects.gatePercent;

    // Save this step for Step Repeat (before pushing queues)
    if (triggers) {
        _lastPlayedStep = effects;
        _lastPlayedStep.gatePercent = baseGate; // overrides the modified gate
    }
    
    // ===== GATE QUEUE =====
    if (_gateQueue) {
        uint64_t oldGateOffTimestamp = 0;

        if (_resetOnStep) {
            if (gatePercent > 1) {
                while (!_gateQueue->isEmpty()) _gateQueue->pop();
            }
        } else {
            while (!_gateQueue->isEmpty()) {
                const GateEvent& event = _gateQueue->peek();
                if (event.type == GateEvent::GATE_OFF) {
                    if (event.timestamp > oldGateOffTimestamp) oldGateOffTimestamp = event.timestamp;
                }
                _gateQueue->pop();
            }
        }

        if (triggers && gatePercent > 0) {
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
                if (random(0, 99) >= effects.probability) continue;
                uint64_t hitTime = firstHitTime + (hit * ratchetSpacing);
                _gateQueue->push(hitTime, GateEvent::GATE_ON);
                
                uint64_t gateOffTime = hitTime + ratchetSpacing / 2;
                if (gateOffTime > firstHitTime + stepDuration) gateOffTime = firstHitTime + stepDuration;
                
                if (!_resetOnStep && (hit == ratchet - 1)) {
                    if (oldGateOffTimestamp > gateOffTime) gateOffTime = oldGateOffTimestamp;
                }
                _gateQueue->push(gateOffTime, GateEvent::GATE_OFF);
            }
        } else if (!_resetOnStep && oldGateOffTimestamp > 0) {
            _gateQueue->push(oldGateOffTimestamp, GateEvent::GATE_OFF);
        }
    }
    
    // ===== CV QUEUE =====
    if (_cvQueue && triggers) {
        while (!_cvQueue->isEmpty()) _cvQueue->pop();
        
        uint16_t targetDAC = effects.cv;
        if (targetDAC < _minCV) targetDAC = _minCV;
        if (targetDAC > _maxCV) targetDAC = _maxCV;
        if (_quantizerEnabled && _quantizer != nullptr) targetDAC = _quantizer->quantize(targetDAC);
        
        uint64_t attackUs = calculateDurationUs(effects.attack);
        uint64_t decayUs = calculateDurationUs(effects.decay);
        
        if (_resetOnStep) {
            if (effects.attack == 0 && effects.decay == 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            } else if (effects.attack > 0 && effects.decay == 0) {
                _cvQueue->push(stepStartTime, 0, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
            } else if (effects.attack == 0 && effects.decay > 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + decayUs, 0, CVEvent::CV_SMOOTH);
            } else {
                _cvQueue->push(stepStartTime, 0, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
                _cvQueue->push(stepStartTime + attackUs + decayUs, 0, CVEvent::CV_SMOOTH);
            }
        } else {
            if (effects.attack == 0 && effects.decay == 0) {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            } else if (targetDAC > _currentDACValue && effects.attack > 0) {
                _cvQueue->push(stepStartTime, _currentDACValue, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + attackUs, targetDAC, CVEvent::CV_SMOOTH);
            } else if (targetDAC < _currentDACValue && effects.decay > 0) {
                _cvQueue->push(stepStartTime, _currentDACValue, CVEvent::CV_STEP);
                _cvQueue->push(stepStartTime + decayUs, targetDAC, CVEvent::CV_SMOOTH);
            } else {
                _cvQueue->push(stepStartTime, targetDAC, CVEvent::CV_STEP);
            }
        }
    }
    
    _currentStep++;
    if (_currentStep >= _pattern->getNumSteps()) _currentStep = 0;
}

void StepSequencer::processStepOutput() {
    uint64_t now = micros();
    
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
    
    if (_cvQueue) {
        while (!_cvQueue->isEmpty() && _cvQueue->peek().timestamp <= now) {
            const CVEvent& event = _cvQueue->peek();
            _currentDACValue = event.value;
            _lastCVEventValue = event.value;
            _lastCVEventTimestamp = now;
            _cvQueue->pop();
        }
        
        if (!_cvQueue->isEmpty()) {
            const CVEvent& nextEvent = _cvQueue->peek();
            if (nextEvent.transitionMode == CVEvent::CV_SMOOTH && nextEvent.timestamp > now) {
                if (_lastCVEventTimestamp > 0 && _lastCVEventTimestamp < nextEvent.timestamp) {
                    uint64_t totalDuration = nextEvent.timestamp - _lastCVEventTimestamp;
                    uint64_t elapsed = now - _lastCVEventTimestamp;
                    if (elapsed < totalDuration) {
                        float progress = (float)elapsed / (float)totalDuration;
                        uint16_t interpolatedValue = _lastCVEventValue + 
                            (uint16_t)(progress * (nextEvent.value - _lastCVEventValue));
                        if (interpolatedValue != _currentDACValue) _currentDACValue = interpolatedValue;
                    }
                }
            }
        }
    }
}

void StepSequencer::renderToDisplay() { _ui.draw(); }
void StepSequencer::onEnterUI() { _ui.enter(); }

void StepSequencer::setMinCV(uint16_t minCV) {
    if (minCV <= _maxCV) _minCV = minCV;
}

void StepSequencer::setMaxCV(uint16_t maxCV) {
    if (maxCV >= _minCV) _maxCV = maxCV;
}

StepSequencer::CopiedStep StepSequencer::_copiedStep = {
    false, 0, 0, 0, 0, 0, 1, 0, 0, 0, false
};

void StepSequencer::copyStep(const StepPattern* pattern, uint8_t stepIndex, uint8_t trackIndex) {
    if (!pattern) return;
    if (stepIndex >= pattern->getNumSteps()) return;
    _copiedStep.on          = pattern->getOn(stepIndex);
    _copiedStep.cv          = pattern->getCV(stepIndex);
    _copiedStep.probability = pattern->getProbability(stepIndex);
    _copiedStep.gateLength  = pattern->getGateLength(stepIndex);
    _copiedStep.decay       = pattern->getDecay(stepIndex);
    _copiedStep.attack      = pattern->getAttack(stepIndex);
    _copiedStep.ratchet     = pattern->getRatchet(stepIndex);
    _copiedStep.microtiming = pattern->getMicrotiming(stepIndex);
    _copiedStep.sourceStep  = stepIndex;
    _copiedStep.sourceTrack = trackIndex;
    _copiedStep.hasData     = true;
}

void StepSequencer::pasteStep(StepPattern* pattern, uint8_t stepIndex) {
    if (!pattern || !_copiedStep.hasData) return;
    if (stepIndex >= pattern->getNumSteps()) return;
    pattern->setOn(stepIndex,          _copiedStep.on);
    pattern->setCV(stepIndex,          _copiedStep.cv);
    pattern->setProbability(stepIndex, _copiedStep.probability);
    pattern->setGateLength(stepIndex,  _copiedStep.gateLength);
    pattern->setDecay(stepIndex,       _copiedStep.decay);
    pattern->setAttack(stepIndex,      _copiedStep.attack);
    pattern->setRatchet(stepIndex,     _copiedStep.ratchet);
    pattern->setMicrotiming(stepIndex, _copiedStep.microtiming);
}