#ifndef SONG_DATA_H
#define SONG_DATA_H

#include "StepPattern.h"
#include "StepSequencer.h"
#include "Quantizer.h"
#include <LittleFS.h>

class SongData {
public:
    static const uint32_t FILE_MAGIC = 0x534F4E47;   // "SONG"
    static const uint8_t FILE_VERSION = 8;           // Bumped for 4 FX banks

    static const uint32_t MAX_SONG_LENGTH = 64;
    static const uint32_t DEFAULT_SONG_LENGTH = 4;
    static const int NUM_TRACKS = 6;
    static const int PATTERN_STEPS = 32;
    static const int NUM_FX_BANKS = 4;

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

    // Performance Effects Management (4 Banks)
    uint8_t getEffectX(int bank, int track) const { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) return _effectX[bank][track]; 
        return 0; 
    }
    void setEffectX(int bank, int track, uint8_t effect) { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) _effectX[bank][track] = effect; 
    }
    
    uint8_t getEffectY(int bank, int track) const { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) return _effectY[bank][track]; 
        return 0; 
    }
    void setEffectY(int bank, int track, uint8_t effect) { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) _effectY[bank][track] = effect; 
    }
    
    int8_t getMagX(int bank, int track) const { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) return _magX[bank][track]; 
        return 4; 
    }
    void setMagX(int bank, int track, int8_t mag) { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) _magX[bank][track] = mag; 
    }
    
    int8_t getMagY(int bank, int track) const { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) return _magY[bank][track]; 
        return 4; 
    }
    void setMagY(int bank, int track, int8_t mag) { 
        if (bank >= 0 && bank < NUM_FX_BANKS && track >= 0 && track < NUM_TRACKS) _magY[bank][track] = mag; 
    }

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

    // Performance Effects (4 Banks)
    uint8_t _effectX[NUM_FX_BANKS][NUM_TRACKS];
    uint8_t _effectY[NUM_FX_BANKS][NUM_TRACKS];
    int8_t _magX[NUM_FX_BANKS][NUM_TRACKS];
    int8_t _magY[NUM_FX_BANKS][NUM_TRACKS];

    void rebuildStepPointers();
    SongStep* getFreeNode();
};

#endif