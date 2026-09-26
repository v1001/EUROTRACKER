#include "SongData.h"
#include <SPIFFS.h>

const SongData::Divider SongData::_dividers[NUM_DIVIDERS] = {
    {"x4", 12},     // 0
    {"x3", 16},     // 1
    {"x2", 24},     // 2
    {"x1$", 32},    // 3
    {"x1", 48},     // 4
    {"/1$", 64},    // 5
    {"/2", 96},     // 6
    {"/3", 144},    // 7
    {"/4", 192}     // 8 
};

// ----------------------------------------------------------------------
// Constructor / Destructor / init / clear
// ----------------------------------------------------------------------
SongData::SongData() : _length(0), _hasCopiedData(false) {
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < MAX_SONG_LENGTH; step++) {
            _dividerIndices[track][step] = 4;  // Default to x1
        }
    }
}

SongData::~SongData() {
}

void SongData::init() {
    _length = DEFAULT_SONG_LENGTH;
    _hasCopiedData = false;

    // Reset quantizers to default chromatic scale
    for (int i = 0; i < 4; i++) {
        _quantizers[i].generateChromatic(0, 4095, 61);
    }

    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < MAX_SONG_LENGTH; step++) {
            _patterns[track][step].init(PATTERN_STEPS);
            for (int s = 0; s < PATTERN_STEPS; s++) {
                _patterns[track][step].setOn(s, false);
                _patterns[track][step].setCV(s, 50);
                _patterns[track][step].setProbability(s, 99);
                _patterns[track][step].setGateLength(s, 8);
                _patterns[track][step].setDecay(s, 0);
                _patterns[track][step].setAttack(s, 0);
                _patterns[track][step].setRatchet(s, 1);
            }
            _dividerIndices[track][step] = 4;
        }
    }
}

void SongData::clear() {
    init();
}

// ----------------------------------------------------------------------
// File operations (streaming, version 5)
// ----------------------------------------------------------------------
bool SongData::load(const char* filename, StepSequencer** sequencers) {
    if (!SPIFFS.exists(filename)) return false;

    File file = SPIFFS.open(filename, FILE_READ);
    if (!file) return false;

    // ---- Header ----
    uint32_t magic;
    if (file.read((uint8_t*)&magic, 4) != 4) { file.close(); return false; }
    if (magic != FILE_MAGIC) { file.close(); return false; }

    uint8_t version;
    if (file.read(&version, 1) != 1) { file.close(); return false; }
    if (version != FILE_VERSION) { file.close(); return false; }

    file.seek(file.position() + 3);   // reserved

    // ---- Song length ----
    if (file.read((uint8_t*)&_length, sizeof(_length)) != sizeof(_length)) {
        file.close(); return false;
    }
    if (_length > MAX_SONG_LENGTH) _length = MAX_SONG_LENGTH;

    // ---- Dividers ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            if (file.read(&_dividerIndices[track][step], 1) != 1) {
                file.close(); return false;
            }
        }
    }

    // ---- Pattern data ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            _patterns[track][step].init(PATTERN_STEPS);
            for (int s = 0; s < PATTERN_STEPS; s++) {
                uint16_t cv;
                if (file.read((uint8_t*)&cv, 2) != 2) { file.close(); return false; }
                uint8_t flags;
                if (file.read(&flags, 1) != 1) { file.close(); return false; }
                _patterns[track][step].setOn(s, (flags & 0x01) != 0);
                _patterns[track][step].setCV(s, cv);

                uint8_t prob, gate, decay, attack, ratchet, micro;
                if (file.read(&prob, 1) != 1 || file.read(&gate, 1) != 1 ||
                    file.read(&decay, 1) != 1 || file.read(&attack, 1) != 1 ||
                    file.read(&ratchet, 1) != 1 || file.read(&micro, 1) != 1) {
                    file.close(); return false;
                }
                _patterns[track][step].setProbability(s, prob);
                _patterns[track][step].setGateLength(s, gate);
                _patterns[track][step].setDecay(s, decay);
                _patterns[track][step].setAttack(s, attack);
                _patterns[track][step].setRatchet(s, ratchet);
                _patterns[track][step].setMicrotiming(s, micro);
            }
        }
    }

    // ---- Pattern lengths ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            uint8_t numSteps;
            if (file.read(&numSteps, 1) != 1) { file.close(); return false; }
            if (numSteps > StepPattern::MAX_STEPS) numSteps = StepPattern::MAX_STEPS;
            _patterns[track][step].setNumSteps(numSteps);
        }
    }

    // ---- Quantizers: enabled + notes (name, dac, inScale) ----
    for (int track = 0; track < 4; track++) {
        uint8_t enabled;
        if (file.read(&enabled, 1) != 1) { file.close(); return false; }
        sequencers[track]->setQuantizerEnabled(enabled != 0);

        uint8_t numNotes;
        if (file.read(&numNotes, 1) != 1) { file.close(); return false; }

        _quantizers[track].clearNotes();
        for (uint8_t i = 0; i < numNotes; i++) {
            char name[5];
            if (file.read((uint8_t*)name, 4) != 4) { file.close(); return false; }
            name[4] = '\0';
            uint16_t dac;
            if (file.read((uint8_t*)&dac, 2) != 2) { file.close(); return false; }
            _quantizers[track].addNote(name, dac);

            uint8_t inScale;
            if (file.read(&inScale, 1) != 1) { file.close(); return false; }
            _quantizers[track].setNoteInScale(i, inScale != 0);
        }
    }

    // ---- CV ranges ----
    for (int track = 0; track < NUM_MELODIC_TRACKS; track++) {
        uint16_t minCV, maxCV;
        if (file.read((uint8_t*)&minCV, 2) != 2 ||
            file.read((uint8_t*)&maxCV, 2) != 2) { file.close(); return false; }
        sequencers[track]->setMinCV(minCV);
        sequencers[track]->setMaxCV(maxCV);
    }

    // ---- Reset flags ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t flag;
        if (file.read(&flag, 1) != 1) { file.close(); return false; }
        sequencers[track]->setResetOnStep(flag != 0);
    }

    // ---- Swing amounts ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t swing;
        if (file.read(&swing, 1) != 1) { file.close(); return false; }
        sequencers[track]->setSwingAmount(swing);
    }

    // ---- Quantizer range params ----
    for (int track = 0; track < 4; track++) {
        uint16_t startDAC, endDAC;
        uint8_t numNotes;
        if (file.read((uint8_t*)&startDAC, 2) != 2 ||
            file.read((uint8_t*)&endDAC, 2) != 2 ||
            file.read(&numNotes, 1) != 1) { file.close(); return false; }
        _quantizers[track].setRangeParams(startDAC, endDAC, numNotes);
    }

    // ---- Quantizer scale index and root index ----
    for (int track = 0; track < 4; track++) {
        uint8_t scaleIdx, rootIdx;
        if (file.read(&scaleIdx, 1) != 1 ||
            file.read(&rootIdx, 1) != 1) { file.close(); return false; }
        _quantizers[track].setScaleIndex(scaleIdx);
        _quantizers[track].setRootIndex(rootIdx);
    }

    file.close();
    return true;
}

bool SongData::save(const char* filename, StepSequencer** sequencers) {
    // Calculate required size (version 3: added 4 tracks * (2+2+1) bytes)
    size_t needed = 0;
    needed += 4 + 1 + 3;                     // magic + version + reserved
    needed += sizeof(_length);
    needed += _length * NUM_TRACKS;           // dividers
    needed += _length * NUM_TRACKS * PATTERN_STEPS * (2 + 1 + 1 + 1 + 1 + 1 + 1 + 1); // cv + flags + 6 others = 9
    needed += _length * NUM_TRACKS;           // pattern lengths
    for (int track = 0; track < 4; track++) {
        needed += 1 + 1 + _quantizers[track].getNumNotes() * (4 + 2 + 1); // enabled + numNotes + notes (4-byte name + 2-byte DAC + 1-byte inScale)
    }
    needed += 4 * 4;                          // CV ranges (min/max)
    needed += NUM_TRACKS;                     // reset flags
    needed += NUM_TRACKS;                     // swing amounts
    needed += 4 * (2 + 2 + 1);                // quantizer range parameters per track
    needed += 4 * 2;                          // scale index + root index per track

    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    size_t free = total - used;
    if (free < needed + 1024) {               // leave 1KB margin
        return false;
    }

    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        return false;
    }

    // ---- Header ----
    uint32_t magic = FILE_MAGIC;
    if (file.write((uint8_t*)&magic, 4) != 4) {
        file.close();
        return false;
    }
    uint8_t version = FILE_VERSION;
    if (file.write(&version, 1) != 1) {
        file.close();
        return false;
    }
    uint8_t reserved[3] = {0, 0, 0};
    if (file.write(reserved, 3) != 3) {
        file.close();
        return false;
    }

    // ---- Song length ----
    if (file.write((uint8_t*)&_length, sizeof(_length)) != sizeof(_length)) {
        file.close();
        return false;
    }

    // ---- Dividers ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            if (file.write(&_dividerIndices[track][step], 1) != 1) {
                file.close();
                return false;
            }
        }
    }

    // ---- Pattern data (version 2: CV + flags + 6 bytes) ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            const StepPattern& pattern = _patterns[track][step];
            for (int s = 0; s < PATTERN_STEPS; s++) {
                uint16_t cv = pattern.getCV(s) & 0x0FFF;
                if (file.write((uint8_t*)&cv, 2) != 2) {
                    file.close();
                    return false;
                }
                uint8_t flags = pattern.getOn(s) ? 0x01 : 0x00;   // bit 0 = ON
                if (file.write(&flags, 1) != 1) {
                    file.close();
                    return false;
                }
                uint8_t val = pattern.getProbability(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
                val = pattern.getGateLength(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
                val = pattern.getDecay(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
                val = pattern.getAttack(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
                val = pattern.getRatchet(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
                val = pattern.getMicrotiming(s);
                if (file.write(&val, 1) != 1) {
                    file.close();
                    return false;
                }
            }
        }
    }

    // ---- Pattern lengths ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int step = 0; step < _length; step++) {
            uint8_t numSteps = _patterns[track][step].getNumSteps();
            if (file.write(&numSteps, 1) != 1) {
                file.close();
                return false;
            }
        }
    }

    // ---- Quantizers (enabled + notes) ----
    for (int track = 0; track < 4; track++) {
        uint8_t enabled = sequencers[track]->isQuantizerEnabled() ? 1 : 0;
        if (file.write(&enabled, 1) != 1) { file.close(); return false; }

        uint8_t numNotes = _quantizers[track].getNumNotes();
        if (file.write(&numNotes, 1) != 1) { file.close(); return false; }

        for (uint8_t i = 0; i < numNotes; i++) {
            const char* name = _quantizers[track].getNoteName(i);
            if (file.write((const uint8_t*)name, 4) != 4) {
                file.close();
                return false;
            }
            uint16_t dac = _quantizers[track].getNoteDAC(i);
            if (file.write((uint8_t*)&dac, 2) != 2) {
                file.close();
                return false;
            }
            // inScale, version 5
            uint8_t inScale = _quantizers[track].getNote(i).inScale ? 1 : 0;
            if (file.write(&inScale, 1) != 1) {
                file.close();
                return false;
            }
        }
    }

    // ---- CV ranges (for editing) ----
    for (int track = 0; track < NUM_MELODIC_TRACKS; track++) {
        uint16_t minCV = sequencers[track]->getMinCV();
        uint16_t maxCV = sequencers[track]->getMaxCV();
        if (file.write((uint8_t*)&minCV, 2) != 2 ||
            file.write((uint8_t*)&maxCV, 2) != 2) {
            file.close();
            return false;
        }
    }

    // ---- Reset flags ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t flag = sequencers[track]->getResetOnStep() ? 1 : 0;
        if (file.write(&flag, 1) != 1) {
            file.close();
            return false;
        }
    }

    // ---- Swing amounts ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t swingAmount = sequencers[track]->getSwingAmount();
        if (file.write(&swingAmount, 1) != 1) {
            file.close();
            return false;
        }
    }

    // ---- NEW: Quantizer range parameters (startDAC, endDAC, numNotes) ----
    for (int track = 0; track < 4; track++) {
        uint16_t startDAC = _quantizers[track].getStartDAC();
        uint16_t endDAC = _quantizers[track].getEndDAC();
        uint8_t numNotes = _quantizers[track].getNumNotes();
        if (file.write((uint8_t*)&startDAC, 2) != 2 ||
            file.write((uint8_t*)&endDAC, 2) != 2 ||
            file.write((uint8_t*)&numNotes, 1) != 1) {
            file.close();
            return false;
        }
    }

    // ---- Quantizer scale index and root note (version 4) ----
    for (int track = 0; track < 4; track++) {
        uint8_t scaleIdx = _quantizers[track].getScaleIndex();
        uint8_t rootIdx  = _quantizers[track].getRootIndex();
        if (file.write(&scaleIdx, 1) != 1 ||
            file.write(&rootIdx, 1) != 1) {
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}

// ----------------------------------------------------------------------
// File utilities (unchanged)
// ----------------------------------------------------------------------
void SongData::deleteFile(const char* filename) {
    if (SPIFFS.exists(filename)) {
        SPIFFS.remove(filename);
    }
}

bool SongData::exists(const char* filename) {
    return SPIFFS.exists(filename);
}

// ----------------------------------------------------------------------
// Getters / Setters (all methods used elsewhere)
// ----------------------------------------------------------------------
void SongData::setLength(int length) {
    if (length >= 1 && length <= MAX_SONG_LENGTH) {
        _length = length;
    }
}

void SongData::setDividerIndex(int track, int step, uint8_t index) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        if (index >= 0 && index < NUM_DIVIDERS) {
            _dividerIndices[track][step] = index;
        }
    }
}

void SongData::rotatePattern(int track, int step, int increment) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        _patterns[track][step].rotate(increment);
    }
}

void SongData::copyPattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        _copiedPattern = _patterns[track][step];
        _copiedDivider = _dividerIndices[track][step];
        _hasCopiedData = true;
    }
}

void SongData::pastePattern(int track, int step) {
    if (_hasCopiedData && track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        _patterns[track][step] = _copiedPattern;
        _dividerIndices[track][step] = _copiedDivider;
    }
}

void SongData::randomizePattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        _patterns[track][step].randomize();
    }
}

void SongData::clearPattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < _length) {
        for (int s = 0; s < PATTERN_STEPS; s++) {
            _patterns[track][step].setOn(s, false);
            _patterns[track][step].setCV(s, 50);
            _patterns[track][step].setProbability(s, 99);
            _patterns[track][step].setGateLength(s, 8);
            _patterns[track][step].setDecay(s, 0);
        }
    }
}

uint32_t SongData::getStepLengthInTicks(int step) const {
    if (step < 0 || step >= _length) return 0;

    uint32_t maxTicks = 0;
    for (int track = 0; track < NUM_TRACKS; track++) {
        const StepPattern& pattern = _patterns[track][step];
        int patternLength = pattern.getNumSteps();
        uint16_t dividerValue = _dividers[_dividerIndices[track][step]].value;
        uint32_t ticks = patternLength * dividerValue;
        if (ticks > maxTicks) maxTicks = ticks;
    }
    return maxTicks;
}

int SongData::getStepLongestPatternIndex(int step) const {
    if (step < 0 || step >= _length) return -1;
    uint32_t maxTicks = 0;
    int longestTrack = -1;
    for (int track = 0; track < NUM_TRACKS; track++) {
        const StepPattern& pattern = _patterns[track][step];
        int patternLength = pattern.getNumSteps();
        uint16_t dividerValue = _dividers[_dividerIndices[track][step]].value;
        uint32_t ticks = patternLength * dividerValue;
        if (ticks > maxTicks) {
            maxTicks = ticks;
            longestTrack = track;
        }
    }
    return longestTrack;
}

Quantizer& SongData::getQuantizer(int track) {
    if (track >= 0 && track < 4) {
        return _quantizers[track];
    }
    static Quantizer defaultQuantizer;
    return defaultQuantizer;
}

const Quantizer& SongData::getQuantizer(int track) const {
    if (track >= 0 && track < 4) {
        return _quantizers[track];
    }
    static const Quantizer defaultQuantizer;
    return defaultQuantizer;
}

void SongData::setQuantizer(int track, const Quantizer& quantizer) {
    if (track >= 0 && track < 4) {
        _quantizers[track] = quantizer;
    }
}