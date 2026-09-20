#include "Scales.h"

// ---- Scale intervals (semitones from root) ----

// Basic scales
static const uint8_t CHROMATIC[] = {0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8_t MAJOR[] = {0,2,4,5,7,9,11};               // Ionian
static const uint8_t NATURAL_MINOR[] = {0,2,3,5,7,8,10};      // Aeolian
static const uint8_t PENTATONIC_MAJOR[] = {0,2,4,7,9};
static const uint8_t BLUES[] = {0,3,5,6,7,10};

// Different minors
static const uint8_t HARMONIC_MINOR[] = {0,2,3,5,7,8,11};
static const uint8_t MELODIC_MINOR[] = {0,2,3,5,7,9,11};

// Modes of the major scale
static const uint8_t DORIAN[] = {0,2,3,5,7,9,10};
static const uint8_t PHRYGIAN[] = {0,1,3,5,7,8,10};
static const uint8_t LYDIAN[] = {0,2,4,6,7,9,11};
static const uint8_t MIXOLYDIAN[] = {0,2,4,5,7,9,10};
static const uint8_t LOCRIAN[] = {0,1,3,5,6,8,10};

// Exotic scales
static const uint8_t WHOLE_TONE[] = {0,2,4,6,8,10};
static const uint8_t DIMINISHED[] = {0,1,3,4,6,7,9,10};       // half-whole octatonic
static const uint8_t AUGMENTED[] = {0,3,4,7,8,11};
static const uint8_t HUNGARIAN_MINOR[] = {0,2,3,6,7,8,11};
static const uint8_t LYDIAN_DOMINANT[] = {0,2,4,6,7,9,10};    // 4th mode of melodic minor
static const uint8_t SUPERLOCRIAN[] = {0,1,3,5,6,8,10};        // 7th mode of melodic minor

// ---- Master scale patterns array (in the requested order) ----
static const ScalePattern SCALE_PATTERNS[] = {
    {"Chromatic", CHROMATIC, 12},
    {"Major", MAJOR, 7},
    {"Nat. Min", NATURAL_MINOR, 7},
    {"Pent. Maj", PENTATONIC_MAJOR, 5},
    {"Blues", BLUES, 6},
    {"Harm. Min", HARMONIC_MINOR, 7},
    {"Melod. Min", MELODIC_MINOR, 7},
    {"Dorian", DORIAN, 7},
    {"Phrygian", PHRYGIAN, 7},
    {"Lydian", LYDIAN, 7},
    {"Mixolydian", MIXOLYDIAN, 7},
    {"Locrian", LOCRIAN, 7},
    {"Whole Tone", WHOLE_TONE, 6},
    {"Diminished", DIMINISHED, 8},
    {"Augmented", AUGMENTED, 6},
    {"Hungr. Min", HUNGARIAN_MINOR, 7},
    {"Lydian Dom", LYDIAN_DOMINANT, 7},
    {"Super Locr", SUPERLOCRIAN, 7},
};

static const uint8_t NUM_SCALES = sizeof(SCALE_PATTERNS) / sizeof(ScalePattern);

// ---- Public functions ----

uint8_t getNumScalePatterns() {
    return NUM_SCALES;
}

const ScalePattern* getScalePattern(uint8_t index) {
    if (index >= NUM_SCALES) return nullptr;
    return &SCALE_PATTERNS[index];
}