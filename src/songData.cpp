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
// Constructor / Destructor / Linked List Helpers
// ----------------------------------------------------------------------
SongData::SongData() : _length(0), _head(nullptr), _tail(nullptr), _hasCopiedData(false) {
}

SongData::~SongData() {
}

void SongData::rebuildStepPointers() {
    SongStep* curr = _head;
    for (uint32_t i = 0; i < _length; i++) {
        _steps[i] = curr;
        curr = curr->next;
    }
}

SongData::SongStep* SongData::getFreeNode() {
    // Find a node in the pool that is not currently in the active _steps array
    for (int i = 0; i < MAX_SONG_LENGTH; i++) {
        bool isUsed = false;
        for (uint32_t j = 0; j < _length; j++) {
            if (_steps[j] == &_nodePool[i]) {
                isUsed = true;
                break;
            }
        }
        if (!isUsed) return &_nodePool[i];
    }
    return nullptr;
}

void SongData::init() {
    _length = DEFAULT_SONG_LENGTH;
    _hasCopiedData = false;

    // Reset quantizers to default chromatic scale
    for (int i = 0; i < 4; i++) {
        _quantizers[i].generateChromatic(0, 4095, 61);
    }

    // 1. Link the ENTIRE pool sequentially for initialization
    for (int i = 0; i < MAX_SONG_LENGTH - 1; i++) {
        _nodePool[i].next = &_nodePool[i + 1];
    }
    _nodePool[MAX_SONG_LENGTH - 1].next = nullptr;

    // 2. Initialize the data for all nodes in the pool
    SongStep* curr = &_nodePool[0];
    for (int i = 0; i < MAX_SONG_LENGTH; i++) {
        for (int track = 0; track < NUM_TRACKS; track++) {
            curr->dividers[track] = 4; // Default x1
            curr->patterns[track].init(PATTERN_STEPS);
            for (int s = 0; s < PATTERN_STEPS; s++) {
                curr->patterns[track].setOn(s, false);
                curr->patterns[track].setCV(s, 50);
                curr->patterns[track].setProbability(s, 99);
                curr->patterns[track].setGateLength(s, 8);
                curr->patterns[track].setDecay(s, 0);
                curr->patterns[track].setAttack(s, 0);
                curr->patterns[track].setRatchet(s, 1);
                curr->patterns[track].setMicrotiming(s, 0);
            }
        }
        curr = curr->next;
    }

    // 3. Set up the active linked list boundaries (truncate to default length)
    _head = &_nodePool[0];
    _tail = &_nodePool[DEFAULT_SONG_LENGTH - 1];
    _tail->next = nullptr; 

    rebuildStepPointers();
}

void SongData::clear() {
    init();
}

// ----------------------------------------------------------------------
// Structural Edits
// ----------------------------------------------------------------------
bool SongData::insertStep(int index) {
    if (_length >= MAX_SONG_LENGTH) return false;
    SongStep* newNode = getFreeNode();
    if (!newNode) return false;

    // Initialize the new node
    for (int t = 0; t < NUM_TRACKS; t++) {
        newNode->dividers[t] = 4;
        newNode->patterns[t].init(PATTERN_STEPS);
    }

    if (index < 0) {
        // Insert at head
        newNode->next = _head;
        _head = newNode;
        if (_length == 0) _tail = newNode;
    } else if (index >= (int)_length - 1) {
        // Insert at tail
        newNode->next = nullptr;
        if (_tail) _tail->next = newNode;
        _tail = newNode;
        if (_length == 0) _head = newNode;
    } else {
        // Insert in middle
        newNode->next = _steps[index]->next;
        _steps[index]->next = newNode;
    }

    _length++;
    rebuildStepPointers();
    return true;
}

bool SongData::deleteStep(int index) {
    if (_length <= 1 || index < 0 || index >= (int)_length) return false;

    SongStep* toDelete = _steps[index];

    if (index == 0) {
        _head = _head->next;
    } else {
        _steps[index - 1]->next = toDelete->next;
    }

    if (toDelete == _tail) {
        _tail = _steps[index - 1];
    }

    _length--;
    rebuildStepPointers();
    return true;
}

bool SongData::cloneStep(int index) {
    if (_length >= MAX_SONG_LENGTH || index < 0 || index >= (int)_length) return false;
    SongStep* newNode = getFreeNode();
    if (!newNode) return false;

    // Copy data
    for (int t = 0; t < NUM_TRACKS; t++) {
        newNode->dividers[t] = _steps[index]->dividers[t];
        newNode->patterns[t] = _steps[index]->patterns[t];
    }

    // Link it after the specified index
    if (index >= (int)_length - 1) {
        newNode->next = nullptr;
        if (_tail) _tail->next = newNode;
        _tail = newNode;
    } else {
        newNode->next = _steps[index]->next;
        _steps[index]->next = newNode;
    }

    _length++;
    rebuildStepPointers();
    return true;
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
    if (_length == 0) _length = 1; // Safety fallback

    // Setup the linked list based on the loaded length
    _head = &_nodePool[0];
    for (uint32_t i = 0; i < _length - 1; i++) {
        _nodePool[i].next = &_nodePool[i + 1];
    }
    _tail = &_nodePool[_length - 1];
    _tail->next = nullptr;
    rebuildStepPointers();

    // ---- Dividers ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            if (file.read(&_steps[step]->dividers[track], 1) != 1) {
                file.close(); return false;
            }
        }
    }

    // ---- Pattern data ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            _steps[step]->patterns[track].init(PATTERN_STEPS);
            for (int s = 0; s < PATTERN_STEPS; s++) {
                uint16_t cv;
                if (file.read((uint8_t*)&cv, 2) != 2) { file.close(); return false; }
                uint8_t flags;
                if (file.read(&flags, 1) != 1) { file.close(); return false; }
                _steps[step]->patterns[track].setOn(s, (flags & 0x01) != 0);
                _steps[step]->patterns[track].setCV(s, cv);

                uint8_t prob, gate, decay, attack, ratchet, micro;
                if (file.read(&prob, 1) != 1 || file.read(&gate, 1) != 1 ||
                    file.read(&decay, 1) != 1 || file.read(&attack, 1) != 1 ||
                    file.read(&ratchet, 1) != 1 || file.read(&micro, 1) != 1) {
                    file.close(); return false;
                }
                _steps[step]->patterns[track].setProbability(s, prob);
                _steps[step]->patterns[track].setGateLength(s, gate);
                _steps[step]->patterns[track].setDecay(s, decay);
                _steps[step]->patterns[track].setAttack(s, attack);
                _steps[step]->patterns[track].setRatchet(s, ratchet);
                _steps[step]->patterns[track].setMicrotiming(s, micro);
            }
        }
    }

    // ---- Pattern lengths ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            uint8_t numSteps;
            if (file.read(&numSteps, 1) != 1) { file.close(); return false; }
            if (numSteps > StepPattern::MAX_STEPS) numSteps = StepPattern::MAX_STEPS;
            _steps[step]->patterns[track].setNumSteps(numSteps);
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
            w.put(&_steps[step]->dividers[track], 1);
        }
    }

    // ---- Pattern data ----
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (uint32_t step = 0; step < _length; step++) {
            const StepPattern& pattern = _steps[step]->patterns[track];
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
            uint8_t numSteps = _steps[step]->patterns[track].getNumSteps();
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

void SongData::deleteFile(const char* filename) {
    if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
    }
}

bool SongData::exists(const char* filename) {
    return LittleFS.exists(filename);
}

void SongData::setLength(uint32_t length) {
    if (length >= 1 && length <= MAX_SONG_LENGTH) {
        if (length > _length) {
            // Growing the song length. Just extend the tail.
            for (uint32_t i = _length; i < length; i++) {
                SongStep* newNode = getFreeNode();
                if (newNode) {
                    for (int t = 0; t < NUM_TRACKS; t++) {
                        newNode->dividers[t] = 4;
                        newNode->patterns[t].init(PATTERN_STEPS);
                    }
                    newNode->next = nullptr;
                    _tail->next = newNode;
                    _tail = newNode;
                }
            }
        } else if (length < _length) {
            // Shrinking the song length. Truncate the list.
            _tail = _steps[length - 1];
            _tail->next = nullptr;
        }
        _length = length;
        rebuildStepPointers();
    }
}

void SongData::setDividerIndex(int track, int step, uint8_t index) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        if (index < NUM_DIVIDERS) {
            _steps[step]->dividers[track] = index;
        }
    }
}

void SongData::rotatePattern(int track, int step, int increment) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        _steps[step]->patterns[track].rotate(increment);
    }
}

void SongData::copyPattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        _copiedPattern = _steps[step]->patterns[track];
        _copiedDivider = _steps[step]->dividers[track];
        _hasCopiedData = true;
    }
}

void SongData::pastePattern(int track, int step) {
    if (_hasCopiedData && track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        _steps[step]->patterns[track] = _copiedPattern;
        _steps[step]->dividers[track] = _copiedDivider;
    }
}

void SongData::randomizePattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        _steps[step]->patterns[track].randomize();
    }
}

void SongData::clearPattern(int track, int step) {
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < (int)_length) {
        for (int s = 0; s < PATTERN_STEPS; s++) {
            _steps[step]->patterns[track].setOn(s, false);
            _steps[step]->patterns[track].setCV(s, 50);
            _steps[step]->patterns[track].setProbability(s, 99);
            _steps[step]->patterns[track].setGateLength(s, 8);
            _steps[step]->patterns[track].setDecay(s, 0);
        }
    }
}

uint32_t SongData::getStepLengthInTicks(int step) const {
    if (step < 0 || step >= (int)_length) return 0;

    uint32_t maxTicks = 0;
    for (int track = 0; track < NUM_TRACKS; track++) {
        const StepPattern& pattern = _steps[step]->patterns[track];
        int patternLength = pattern.getNumSteps();
        uint16_t dividerValue = _dividers[_steps[step]->dividers[track]].value;
        uint32_t ticks = patternLength * dividerValue;
        if (ticks > maxTicks) maxTicks = ticks;
    }
    return maxTicks;
}

int SongData::getStepLongestPatternIndex(int step) const {
    if (step < 0 || step >= (int)_length) return -1;
    uint32_t maxTicks = 0;
    int longestTrack = -1;
    for (int track = 0; track < NUM_TRACKS; track++) {
        const StepPattern& pattern = _steps[step]->patterns[track];
        int patternLength = pattern.getNumSteps();
        uint16_t dividerValue = _dividers[_steps[step]->dividers[track]].value;
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