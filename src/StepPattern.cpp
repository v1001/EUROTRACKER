#include "StepPattern.h"
#include <cstring> // for memcpy? not needed.

// Default constructor: empty pattern
StepPattern::StepPattern() : _numSteps(0), _on(nullptr), _cv(nullptr), _probability(nullptr),
                             _gateLength(nullptr), _decay(nullptr), _attack(nullptr), _ratchet(nullptr),
                             _microtiming(nullptr) {
}

// Constructor with number of steps: allocates and clears
StepPattern::StepPattern(uint8_t numSteps) : _numSteps(0), _on(nullptr), _cv(nullptr), _probability(nullptr),
                                             _gateLength(nullptr), _decay(nullptr), _attack(nullptr),
                                             _ratchet(nullptr), _microtiming(nullptr) {
    init(numSteps);
}

// Copy constructor: deep copy
StepPattern::StepPattern(const StepPattern& other) : _numSteps(0), _on(nullptr), _cv(nullptr),
    _probability(nullptr), _gateLength(nullptr), _decay(nullptr), _attack(nullptr),
    _ratchet(nullptr), _microtiming(nullptr) {
    copyFrom(other);
}

// Destructor: free all arrays
StepPattern::~StepPattern() {
    delete[] _on;
    delete[] _cv;
    delete[] _probability;
    delete[] _gateLength;
    delete[] _decay;
    delete[] _attack;
    delete[] _ratchet;
    delete[] _microtiming;
}

// Assignment operator: free existing and copy from other
StepPattern& StepPattern::operator=(const StepPattern& other) {
    if (this != &other) {
        delete[] _on;
        delete[] _cv;
        delete[] _probability;
        delete[] _gateLength;
        delete[] _decay;
        delete[] _attack;
        delete[] _ratchet;
        delete[] _microtiming;
        copyFrom(other);
    }
    return *this;
}

/**
 * copyFrom: helper for copy constructor and assignment operator.
 * Assumes this object's arrays are already freed.
 * Allocates new arrays and copies data from other.
 */
void StepPattern::copyFrom(const StepPattern& other) {
    _numSteps = other._numSteps;

    if (_numSteps > 0) {
        _on = new bool[_numSteps];
        _cv = new uint16_t[_numSteps];          // 16-bit
        _probability = new uint8_t[_numSteps];
        _gateLength = new uint8_t[_numSteps];
        _decay = new uint8_t[_numSteps];
        _attack = new uint8_t[_numSteps];
        _ratchet = new uint8_t[_numSteps];
        _microtiming = new uint8_t[_numSteps];

        for (int i = 0; i < _numSteps; i++) {
            _on[i] = other._on[i];
            _cv[i] = other._cv[i];
            _probability[i] = other._probability[i];
            _gateLength[i] = other._gateLength[i];
            _decay[i] = other._decay[i];
            _attack[i] = other._attack[i];
            _ratchet[i] = other._ratchet[i];
            _microtiming[i] = other._microtiming[i];
        }
    } else {
        _on = nullptr;
        _cv = nullptr;
        _probability = nullptr;
        _gateLength = nullptr;
        _decay = nullptr;
        _attack = nullptr;
        _ratchet = nullptr;
        _microtiming = nullptr;
    }
}

/**
 * init: (re)initialise pattern with given number of steps.
 * Frees any existing arrays, allocates new ones, and resets all values to defaults.
 */
void StepPattern::init(uint8_t numSteps) {
    if (numSteps > MAX_STEPS) numSteps = MAX_STEPS;

    delete[] _on;
    delete[] _cv;
    delete[] _probability;
    delete[] _gateLength;
    delete[] _decay;
    delete[] _attack;
    delete[] _ratchet;
    delete[] _microtiming;

    _numSteps = numSteps;

    if (_numSteps == 0) {
        _on = nullptr;
        _cv = nullptr;
        _probability = nullptr;
        _gateLength = nullptr;
        _decay = nullptr;
        _attack = nullptr;
        _ratchet = nullptr;
        _microtiming = nullptr;
        return;
    }

    _on = new bool[_numSteps];
    _cv = new uint16_t[_numSteps];
    _probability = new uint8_t[_numSteps];
    _gateLength = new uint8_t[_numSteps];
    _decay = new uint8_t[_numSteps];
    _attack = new uint8_t[_numSteps];
    _ratchet = new uint8_t[_numSteps];
    _microtiming = new uint8_t[_numSteps];

    clearAll();
}

// ---- Getters / Setters ----

bool StepPattern::getOn(uint8_t step) const {
    if (step >= _numSteps) return false;
    return _on[step];
}

void StepPattern::setOn(uint8_t step, bool on) {
    if (step < _numSteps) _on[step] = on;
}

uint16_t StepPattern::getCV(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _cv[step];
}

void StepPattern::setCV(uint8_t step, uint16_t cv) {
    if (step < _numSteps) _cv[step] = cv;   // 12-bit value, stored in uint16_t
}

uint8_t StepPattern::getProbability(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _probability[step];
}

void StepPattern::setProbability(uint8_t step, uint8_t prob) {
    if (step < _numSteps) _probability[step] = prob;
}

uint8_t StepPattern::getGateLength(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _gateLength[step];
}

void StepPattern::setGateLength(uint8_t step, uint8_t length) {
    if (step < _numSteps) _gateLength[step] = length;
}

uint8_t StepPattern::getDecay(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _decay[step];
}

void StepPattern::setDecay(uint8_t step, uint8_t decay) {
    if (step < _numSteps) _decay[step] = decay;
}

uint8_t StepPattern::getAttack(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _attack[step];
}

void StepPattern::setAttack(uint8_t step, uint8_t attack) {
    if (step < _numSteps) _attack[step] = attack;
}

uint8_t StepPattern::getRatchet(uint8_t step) const {
    if (step >= _numSteps) return 1;
    return _ratchet[step];
}

void StepPattern::setRatchet(uint8_t step, uint8_t ratchet) {
    if (step < _numSteps) {
        if (ratchet < 1) ratchet = 1;
        if (ratchet > 4) ratchet = 4;
        _ratchet[step] = ratchet;
    }
}

uint8_t StepPattern::getMicrotiming(uint8_t step) const {
    if (step >= _numSteps) return 0;
    return _microtiming[step];
}

void StepPattern::setMicrotiming(uint8_t step, uint8_t value) {
    if (step < _numSteps) _microtiming[step] = value;
}

// ---- Bulk operations ----

void StepPattern::clearAll() {
    for (uint8_t i = 0; i < _numSteps; i++) {
        _on[i] = false;
        _cv[i] = 0;
        _probability[i] = 99;
        _gateLength[i] = 0;
        _decay[i] = 0;
        _attack[i] = 0;
        _ratchet[i] = 1;
        _microtiming[i] = 0;
    }
}

void StepPattern::fillAll() {
    for (uint8_t i = 0; i < _numSteps; i++) {
        _on[i] = true;
        _cv[i] = 2048;          // mid‑scale DAC
        _probability[i] = 99;
        _gateLength[i] = 50;
        _decay[i] = 0;
        _attack[i] = 0;
        _ratchet[i] = 1;
        _microtiming[i] = 0;
    }
}

void StepPattern::randomize() {
    for (uint8_t i = 0; i < _numSteps; i++) {
        _on[i] = random(0, 2);
        _cv[i] = random(0, 4096);
        _probability[i] = random(0, 100);
        _gateLength[i] = random(0, 100);
        _decay[i] = random(0, 100);
        _attack[i] = random(0, 100);
        _ratchet[i] = random(1, 5);
        _microtiming[i] = random(0, 100);
    }
}

/**
 * rotate: shifts all step data by 'increment' positions.
 * Positive increment moves data forward (toward higher step indices).
 * Negative moves backward.
 * Data wraps around (circular shift).
 */
void StepPattern::rotate(int8_t increment) {
    if (_numSteps == 0 || increment == 0) return;

    int8_t shift = increment % _numSteps;
    if (shift < 0) shift += _numSteps;
    if (shift == 0) return;

    bool* rotated_on = new bool[_numSteps];
    uint16_t* rotated_cv = new uint16_t[_numSteps];
    uint8_t* rotated_probability = new uint8_t[_numSteps];
    uint8_t* rotated_gateLength = new uint8_t[_numSteps];
    uint8_t* rotated_decay = new uint8_t[_numSteps];
    uint8_t* rotated_attack = new uint8_t[_numSteps];
    uint8_t* rotated_ratchet = new uint8_t[_numSteps];
    uint8_t* rotated_microtiming = new uint8_t[_numSteps];

    for (int i = 0; i < _numSteps; i++) {
        int newIndex = (i + shift) % _numSteps;
        rotated_on[newIndex] = _on[i];
        rotated_cv[newIndex] = _cv[i];
        rotated_probability[newIndex] = _probability[i];
        rotated_gateLength[newIndex] = _gateLength[i];
        rotated_decay[newIndex] = _decay[i];
        rotated_attack[newIndex] = _attack[i];
        rotated_ratchet[newIndex] = _ratchet[i];
        rotated_microtiming[newIndex] = _microtiming[i];
    }

    for (int i = 0; i < _numSteps; i++) {
        _on[i] = rotated_on[i];
        _cv[i] = rotated_cv[i];
        _probability[i] = rotated_probability[i];
        _gateLength[i] = rotated_gateLength[i];
        _decay[i] = rotated_decay[i];
        _attack[i] = rotated_attack[i];
        _ratchet[i] = rotated_ratchet[i];
        _microtiming[i] = rotated_microtiming[i];
    }

    delete[] rotated_on;
    delete[] rotated_cv;
    delete[] rotated_probability;
    delete[] rotated_gateLength;
    delete[] rotated_decay;
    delete[] rotated_attack;
    delete[] rotated_ratchet;
    delete[] rotated_microtiming;
}

/**
 * setNumSteps: changes the number of steps.
 * If new size is smaller, data is truncated.
 * If larger, new steps are initialised to defaults.
 * Existing data is preserved.
 */
void StepPattern::setNumSteps(uint8_t numSteps) {
    if (numSteps == _numSteps) return;
    if (numSteps > MAX_STEPS) numSteps = MAX_STEPS;

    bool* new_on = new bool[numSteps];
    uint16_t* new_cv = new uint16_t[numSteps];
    uint8_t* new_probability = new uint8_t[numSteps];
    uint8_t* new_gateLength = new uint8_t[numSteps];
    uint8_t* new_decay = new uint8_t[numSteps];
    uint8_t* new_attack = new uint8_t[numSteps];
    uint8_t* new_ratchet = new uint8_t[numSteps];
    uint8_t* new_microtiming = new uint8_t[numSteps];

    uint8_t minSteps = (_numSteps < numSteps) ? _numSteps : numSteps;

    for (uint8_t i = 0; i < minSteps; i++) {
        new_on[i] = _on[i];
        new_cv[i] = _cv[i];
        new_probability[i] = _probability[i];
        new_gateLength[i] = _gateLength[i];
        new_decay[i] = _decay[i];
        new_attack[i] = _attack[i];
        new_ratchet[i] = _ratchet[i];
        new_microtiming[i] = _microtiming[i];
    }

    // Initialize new steps (if any)
    for (uint8_t i = minSteps; i < numSteps; i++) {
        new_on[i] = false;
        new_cv[i] = 0;
        new_probability[i] = 99;
        new_gateLength[i] = 0;
        new_decay[i] = 0;
        new_attack[i] = 0;
        new_ratchet[i] = 1;
        new_microtiming[i] = 0;
    }

    delete[] _on;
    delete[] _cv;
    delete[] _probability;
    delete[] _gateLength;
    delete[] _decay;
    delete[] _attack;
    delete[] _ratchet;
    delete[] _microtiming;

    _on = new_on;
    _cv = new_cv;
    _probability = new_probability;
    _gateLength = new_gateLength;
    _decay = new_decay;
    _attack = new_attack;
    _ratchet = new_ratchet;
    _microtiming = new_microtiming;
    _numSteps = numSteps;
}