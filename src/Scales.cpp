#include "Scales.h"
#include <cstring>

// Note names (project convention)
static const char* NOTE_NAMES[12] = {"c", "C", "d", "D", "e", "f", "F", "g", "G", "a", "A", "b"};

// ---- Scale intervals (semitones from root) ----

// Chromatic
static const uint8_t CHROMATIC[] = {0,1,2,3,4,5,6,7,8,9,10,11};

// Major scale (Ionian) - explicit
static const uint8_t MAJOR[] = {0,2,4,5,7,9,11};

// Natural minor (Aeolian)
static const uint8_t NATURAL_MINOR[] = {0,2,3,5,7,8,10};

// Other minor scales
static const uint8_t HARMONIC_MINOR[] = {0,2,3,5,7,8,11};
static const uint8_t MELODIC_MINOR[] = {0,2,3,5,7,9,11};

// Pentatonics
static const uint8_t PENTATONIC_MAJOR[] = {0,2,4,7,9};
static const uint8_t PENTATONIC_MINOR[] = {0,3,5,7,10};

// Blues
static const uint8_t BLUES[] = {0,3,5,6,7,10};

// Modes of the major scale (excluding Ionian, which is already MAJOR)
static const uint8_t DORIAN[] = {0,2,3,5,7,9,10};
static const uint8_t PHRYGIAN[] = {0,1,3,5,7,8,10};
static const uint8_t LYDIAN[] = {0,2,4,6,7,9,11};
static const uint8_t MIXOLYDIAN[] = {0,2,4,5,7,9,10};
static const uint8_t LOCRIAN[] = {0,1,3,5,6,8,10};

// Exotic scales
static const uint8_t WHOLE_TONE[] = {0,2,4,6,8,10};
static const uint8_t DIMINISHED_OCTATONIC[] = {0,1,3,4,6,7,9,10}; // half-whole
static const uint8_t AUGMENTED[] = {0,3,4,7,8,11};                // augmented scale
static const uint8_t HUNGARIAN_MINOR[] = {0,2,3,6,7,8,11};

// ---- Master scale patterns array ----
static const ScalePattern SCALE_PATTERNS[] = {
    {"Chromatic", CHROMATIC, 12},
    {"Major", MAJOR, 7},
    {"Nat. Minor", NATURAL_MINOR, 7},
    {"Pent Major", PENTATONIC_MAJOR, 5},
    {"Pent Minor", PENTATONIC_MINOR, 5},
    {"Blues", BLUES, 6},
    {"Harm Minor", HARMONIC_MINOR, 7},
    {"Melo Minor", MELODIC_MINOR, 7},
    {"Dorian", DORIAN, 7},
    {"Phrygian", PHRYGIAN, 7},
    {"Lydian", LYDIAN, 7},
    {"Mixolydian", MIXOLYDIAN, 7},
    {"Locrian", LOCRIAN, 7},
    {"Whole Tone", WHOLE_TONE, 6},
    {"Diminished", DIMINISHED_OCTATONIC, 8},
    {"Augmented", AUGMENTED, 6},
    {"Hung. Minor", HUNGARIAN_MINOR, 7},
};

static const uint8_t NUM_SCALES = sizeof(SCALE_PATTERNS) / sizeof(ScalePattern);

// ---- Helper: get root index from character ----
static uint8_t getRootIndex(char root) {
    for (uint8_t i = 0; i < 12; i++) {
        if (NOTE_NAMES[i][0] == root) {
            return i;
        }
    }
    return 0; // default to C
}

// ---- Public functions ----

uint8_t getNumScalePatterns() {
    return NUM_SCALES;
}

const ScalePattern* getScalePattern(uint8_t index) {
    if (index >= NUM_SCALES) return nullptr;
    return &SCALE_PATTERNS[index];
}

const ScalePattern* getScalePatternByName(const char* name) {
    for (uint8_t i = 0; i < NUM_SCALES; i++) {
        if (strcmp(SCALE_PATTERNS[i].name, name) == 0) {
            return &SCALE_PATTERNS[i];
        }
    }
    return nullptr;
}

bool applyScaleToQuantizer(Quantizer& quantizer, char root, const ScalePattern* scalePattern) {
    if (!scalePattern) return false;

    // Generate note names for this scale
    const char* noteNames[12];  // max 12 notes
    uint8_t numNotes = scalePattern->numNotes;
    uint8_t rootIndex = getRootIndex(root);

    for (uint8_t i = 0; i < numNotes; i++) {
        uint8_t noteIndex = (rootIndex + scalePattern->intervals[i]) % 12;
        noteNames[i] = NOTE_NAMES[noteIndex];
    }

    // Apply the scale to the quantizer
    quantizer.applyScale(noteNames, numNotes);
    return true;
}

bool applyScaleToQuantizer(Quantizer& quantizer, char root, uint8_t index) {
    const ScalePattern* pattern = getScalePattern(index);
    return applyScaleToQuantizer(quantizer, root, pattern);
}