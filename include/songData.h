#ifndef SONG_DATA_H
#define SONG_DATA_H

#include "StepPattern.h"
#include "StepSequencer.h"
#include "Quantizer.h"
#include <LittleFS.h>

class SongData {
public:
    static const uint32_t FILE_MAGIC = 0x534F4E47;   // "SONG"
    static const uint8_t FILE_VERSION = 7;           // Bumped for performance effects

    static const uint32_t MAX_SONG_LENGTH = 64;
    static const uint32_t DEFAULT_SONG_LENGTH = 4;
    static const int NUM_TRACKS = 6;
    static const int PATTERN_STEPS = 32;

    struct Divider {
        const char* text;
        uint16_t value;
    };

    struct SongStep {
        StepPattern patterns[NUM_TRACKS];
        uint8_t dividers[NUM_TRACKS];
        
        SongStep* next;
        SongStep* loopTarget;
        uint8_t loopCount;
    };

    SongData();
    ~SongData();

    void init();
    void clear();

    bool load(const char* filename, StepSequencer** sequencers);
    bool save(const char* filename, StepSequencer** sequencers);
    void deleteFile(const char* filename);
    bool exists(const char* filename);

    bool insertStep(int index);
    bool deleteStep(int index);
    bool cloneStep(int index);

    uint32_t getLength() const { return _length; }
    StepPattern& getPattern(int track, int step) { return _steps[step]->patterns[track]; }
    const StepPattern& getPattern(int track, int step) const { return _steps[step]->patterns[track]; }
    uint8_t getDividerIndex(int track, int step) const { return _steps[step]->dividers[track]; }
    const Divider& getDivider(int track, int step) const { return _dividers[_steps[step]->dividers[track]]; }
    uint16_t getDividerValue(int track, int step) const { return _dividers[_steps[step]->dividers[track]].value; }

    void setLength(uint32_t length);
    void setDividerIndex(int track, int step, uint8_t index);

    void rotatePattern(int track, int step, int increment);
    void randomizePattern(int track, int step);
    void clearPattern(int track, int step);

    void copyPattern(int track, int step);
    void pastePattern(int track, int step);
    bool hasCopiedData() const { return _hasCopiedData; }

    static const Divider& getDividerByIndex(int index) { return _dividers[index]; }
    static int getNumDividers() { return NUM_DIVIDERS; }
    static uint16_t getDividerValueByIndex(int index) { return _dividers[index].value; }
    static const char* getDividerTextByIndex(int index) { return _dividers[index].text; }

    uint32_t getStepLengthInTicks(int step) const;
    int getStepLongestPatternIndex(int step) const;

    Quantizer& getQuantizer(int track);
    const Quantizer& getQuantizer(int track) const;
    void setQuantizer(int track, const Quantizer& quantizer);

    // Loop management
    SongStep* getStep(int index);
    int getStepIndex(SongStep* target) const;
    bool setStepLoop(int stepIndex, int targetIndex, uint8_t count);
    void clearStepLoop(int stepIndex);

    // Performance Effects Management
    uint8_t getEffectX(int track) const { return (track >= 0 && track < 6) ? _effectX[track] : 0; }
    void setEffectX(int track, uint8_t effect) { if (track >= 0 && track < 6) _effectX[track] = effect; }
    
    uint8_t getEffectY(int track) const { return (track >= 0 && track < 6) ? _effectY[track] : 0; }
    void setEffectY(int track, uint8_t effect) { if (track >= 0 && track < 6) _effectY[track] = effect; }
    
    int8_t getMagX(int track) const { return (track >= 0 && track < 6) ? _magX[track] : 0; }
    void setMagX(int track, int8_t mag) { if (track >= 0 && track < 6) _magX[track] = mag; }
    
    int8_t getMagY(int track) const { return (track >= 0 && track < 6) ? _magY[track] : 0; }
    void setMagY(int track, int8_t mag) { if (track >= 0 && track < 6) _magY[track] = mag; }

private:
    static const int NUM_MELODIC_TRACKS = 4;
    static const int NUM_DIVIDERS = 9;
    static const Divider _dividers[NUM_DIVIDERS];

    SongStep _nodePool[MAX_SONG_LENGTH];
    SongStep* _head;
    SongStep* _tail;
    SongStep* _steps[MAX_SONG_LENGTH];
    uint32_t _length;

    StepPattern _copiedPattern;
    uint8_t _copiedDivider;
    bool _hasCopiedData;

    Quantizer _quantizers[4];

    // Performance Effects
    uint8_t _effectX[6]; // 0=None, 1=Transpose, 2=Slide, 3=Random, 4=Density
    uint8_t _effectY[6];
    int8_t _magX[6];     // -4 to +4
    int8_t _magY[6];

    void rebuildStepPointers();
    SongStep* getFreeNode();
};

#endif