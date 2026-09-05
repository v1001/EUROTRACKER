#ifndef STEP_PATTERN_H
#define STEP_PATTERN_H

#include <Arduino.h>

/**
 * StepPattern stores a single pattern of up to 32 steps.
 * Each step contains:
 * - on/off state (bool)
 * - CV value (16-bit raw DAC 0-4095)
 * - probability (0-99)
 * - gate length (0-99)
 * - decay (0-99)
 * - attack (0-99)
 * - ratchet (1-4)
 * - microtiming (0-99)
 */
class StepPattern {
public:
    static const uint8_t MAX_STEPS = 32;

    // Default constructor: empty pattern (0 steps)
    StepPattern();

    // Constructor with initial number of steps (allocates arrays)
    StepPattern(uint8_t numSteps);

    // Copy constructor (deep copy)
    StepPattern(const StepPattern& other);

    // Assignment operator (deep copy)
    StepPattern& operator=(const StepPattern& other);

    // Destructor (frees arrays)
    ~StepPattern();

    // Initialise pattern with given number of steps (resets all values)
    void init(uint8_t numSteps = 32);

    // Get number of steps
    uint8_t getNumSteps() const { return _numSteps; }

    // Returns true if pattern has zero steps
    bool isEmpty() const { return _numSteps == 0; }

    // Set number of steps (preserves existing data, new steps are cleared)
    void setNumSteps(uint8_t numSteps);

    // Step state getters/setters
    bool getOn(uint8_t step) const;
    void setOn(uint8_t step, bool on);

    // CV: raw 12-bit DAC value (0-4095)
    uint16_t getCV(uint8_t step) const;
    void setCV(uint8_t step, uint16_t cv);

    uint8_t getProbability(uint8_t step) const;
    void setProbability(uint8_t step, uint8_t prob);

    uint8_t getGateLength(uint8_t step) const;
    void setGateLength(uint8_t step, uint8_t length);

    uint8_t getDecay(uint8_t step) const;
    void setDecay(uint8_t step, uint8_t decay);

    uint8_t getAttack(uint8_t step) const;
    void setAttack(uint8_t step, uint8_t attack);

    uint8_t getRatchet(uint8_t step) const;
    void setRatchet(uint8_t step, uint8_t ratchet);

    uint8_t getMicrotiming(uint8_t step) const;
    void setMicrotiming(uint8_t step, uint8_t value);

    // Clear all steps (off, CV=0, etc.)
    void clearAll();

    // Fill all steps with default values (on, CV=2048, etc.)
    void fillAll();

    // Randomize all step parameters
    void randomize();

    // Rotate all steps by increment (positive = forward, negative = backward)
    void rotate(int8_t increment);

private:
    // Deep copy helper used by copy constructor and assignment
    void copyFrom(const StepPattern& other);

    uint8_t _numSteps;
    bool* _on;
    uint16_t* _cv;           // 16‑bit raw DAC value (0-4095)
    uint8_t* _probability;
    uint8_t* _gateLength;
    uint8_t* _decay;
    uint8_t* _attack;
    uint8_t* _ratchet;
    uint8_t* _microtiming;
};

#endif