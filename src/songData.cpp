#include "SongData.h"
#include <cstring>

namespace {

class BufferedWriter {
public:
    explicit BufferedWriter(File& file) : _file(file) {}

    void put(const void* data, size_t len) {
        if (!_ok) return;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        while (len > 0) {
            size_t space = BUFFER_SIZE - _used;
            size_t chunk = (len < space) ? len : space;
            memcpy(_buf + _used, p, chunk);
            _used += chunk;
            p += chunk;
            len -= chunk;
            if (_used == BUFFER_SIZE) flush();
        }
    }

    template <typename T>
    void put(const T& value) {
        put(&value, sizeof(T));
    }

    void flush() {
        if (_used == 0 || !_ok) return;
        if (_file.write(_buf, _used) != _used) _ok = false;
        _used = 0;
    }

    bool finish() {
        flush();
        return _ok;
    }

private:
    static const size_t BUFFER_SIZE = 512;
    File& _file;
    uint8_t _buf[BUFFER_SIZE];
    size_t _used = 0;
    bool _ok = true;
};

} // namespace

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
    if (!LittleFS.exists(filename)) return false;

    File file = LittleFS.open(filename, FILE_READ);
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
    // Size check (unchanged logic, now against LittleFS)
    size_t needed = 0;
    needed += 4 + 1 + 3;
    needed += sizeof(_length);
    needed += _length * NUM_TRACKS;
    needed += _length * NUM_TRACKS * PATTERN_STEPS * (2 + 1 + 1 + 1 + 1 + 1 + 1 + 1);
    needed += _length * NUM_TRACKS;
    for (int track = 0; track < 4; track++) {
        needed += 1 + 1 + _quantizers[track].getNumNotes() * (4 + 2 + 1);
    }
    needed += 4 * 4;
    needed += NUM_TRACKS;
    needed += NUM_TRACKS;
    needed += 4 * (2 + 2 + 1);
    needed += 4 * 2;

    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    size_t free  = total - used;
    if (free < needed + 1024) return false;

    File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) return false;

    BufferedWriter w(file);

    // ---- Header ----
    uint32_t magic = FILE_MAGIC;
    w.put(magic);
    uint8_t version = FILE_VERSION;
    w.put(version);
    uint8_t reserved[3] = {0, 0, 0};
    w.put(reserved, 3);

    // ---- Song length ----
    w.put(_length);

    // ---- Dividers ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            w.put(&_dividerIndices[track][step], 1);
        }
    }

    // ---- Pattern data ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            const StepPattern& pattern = _patterns[track][step];
            for (int s = 0; s < PATTERN_STEPS; s++) {
                uint16_t cv = pattern.getCV(s) & 0x0FFF;
                w.put(cv);
                uint8_t flags = pattern.getOn(s) ? 0x01 : 0x00;
                w.put(flags);
                uint8_t val = pattern.getProbability(s); w.put(val);
                val = pattern.getGateLength(s);           w.put(val);
                val = pattern.getDecay(s);                w.put(val);
                val = pattern.getAttack(s);               w.put(val);
                val = pattern.getRatchet(s);              w.put(val);
                val = pattern.getMicrotiming(s);          w.put(val);
            }
        }
    }

    // ---- Pattern lengths ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            uint8_t numSteps = _patterns[track][step].getNumSteps();
            w.put(numSteps);
        }
    }

    // ---- Quantizers ----
    for (int track = 0; track < 4; track++) {
        uint8_t enabled = sequencers[track]->isQuantizerEnabled() ? 1 : 0;
        w.put(enabled);
        uint8_t numNotes = _quantizers[track].getNumNotes();
        w.put(numNotes);
        for (uint8_t i = 0; i < numNotes; i++) {
            const char* name = _quantizers[track].getNoteName(i);
            w.put(name, 4);
            uint16_t dac = _quantizers[track].getNoteDAC(i);
            w.put(dac);
            uint8_t inScale = _quantizers[track].getNote(i).inScale ? 1 : 0;
            w.put(inScale);
        }
    }

    // ---- CV ranges ----
    for (int track = 0; track < NUM_MELODIC_TRACKS; track++) {
        uint16_t minCV = sequencers[track]->getMinCV();
        uint16_t maxCV = sequencers[track]->getMaxCV();
        w.put(minCV);
        w.put(maxCV);
    }

    // ---- Reset flags ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t flag = sequencers[track]->getResetOnStep() ? 1 : 0;
        w.put(flag);
    }

    // ---- Swing ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        uint8_t swingAmount = sequencers[track]->getSwingAmount();
        w.put(swingAmount);
    }

    // ---- Quantizer range params ----
    for (int track = 0; track < 4; track++) {
        uint16_t startDAC = _quantizers[track].getStartDAC();
        uint16_t endDAC   = _quantizers[track].getEndDAC();
        uint8_t numNotes  = _quantizers[track].getNumNotes();
        w.put(startDAC);
        w.put(endDAC);
        w.put(numNotes);
    }

    // ---- Scale/root ----
    for (int track = 0; track < 4; track++) {
        uint8_t scaleIdx = _quantizers[track].getScaleIndex();
        uint8_t rootIdx  = _quantizers[track].getRootIndex();
        w.put(scaleIdx);
        w.put(rootIdx);
    }

    if (!w.finish()) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

// ----------------------------------------------------------------------
// File utilities (unchanged)
// ----------------------------------------------------------------------
void SongData::deleteFile(const char* filename) {
    if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
    }
}

bool SongData::exists(const char* filename) {
    return LittleFS.exists(filename);
}

// ----------------------------------------------------------------------
// Getters / Setters (all methods used elsewhere)
// ----------------------------------------------------------------------
void SongData::setLength(uint32_t length) {
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