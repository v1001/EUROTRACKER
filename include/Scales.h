#ifndef SCALES_H
#define SCALES_H

#include <Arduino.h>
#include "Quantizer.h"

// Scale pattern: name and intervals (semitone offsets from root)
struct ScalePattern {
    const char* name;
    const uint8_t* intervals;
    uint8_t numNotes;
};

// Get the number of available scale patterns
uint8_t getNumScalePatterns();

// Get a scale pattern by index (returns nullptr if out of range)
const ScalePattern* getScalePattern(uint8_t index);

// Get a scale pattern by name (returns nullptr if not found)
const ScalePattern* getScalePatternByName(const char* name);

// Apply a scale pattern to a quantizer given a root note character
// root: single character from NOTE_NAMES (e.g., 'c', 'C', 'd', ...)
// scalePattern: pointer to the ScalePattern to apply
// Returns true if successful
bool applyScaleToQuantizer(Quantizer& quantizer, char root, const ScalePattern* scalePattern);

// Convenience: apply by index and root
bool applyScaleToQuantizer(Quantizer& quantizer, char root, uint8_t index);

#endif