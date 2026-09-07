#ifndef SCALES_H
#define SCALES_H

#include <Arduino.h>

struct ScalePattern {
    const char* name;
    const uint8_t* intervals;   // semitone offsets from root
    uint8_t numNotes;
};

// Get the number of available scale patterns
uint8_t getNumScalePatterns();

// Get a scale pattern by index (returns nullptr if out of range)
const ScalePattern* getScalePattern(uint8_t index);

#endif