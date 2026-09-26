#ifndef SONG_DATA_H
#define SONG_DATA_H

#include "StepPattern.h"
#include "StepSequencer.h"
#include "Quantizer.h"
#include <SPIFFS.h>

class SongData {
public:
    // Constants for file format
    static const uint32_t FILE_MAGIC = 0x534F4E47;   // "SONG"
    // Version encoded as major*10 + minor (e.g. 5 = v0.5, 10 = v1.0, 11 = v1.1, 20 = v2.0).
    // Minor bumps append fields at the end of the file.
    // Major bumps change layout and break compatibility.
    static const uint8_t FILE_VERSION = 5;

    // Constants
    static const int MAX_SONG_LENGTH = 64;
    static const int DEFAULT_SONG_LENGTH = 4;
    static const int NUM_TRACKS = 6;
    static const int PATTERN_STEPS = 32;
    
    // Divider structure
    struct Divider {
        const char* text;
        uint16_t value;
    };
    
    SongData();
    ~SongData();
    
    // Initialization
    void init();
    void clear();
    
    // File operations
    bool load(const char* filename, StepSequencer** sequencers);
    bool save(const char* filename, StepSequencer** sequencers);
    void deleteFile(const char* filename);
    bool exists(const char* filename);
    
    // Getters
    int getLength() const { return _length; }
    StepPattern& getPattern(int track, int step) { return _patterns[track][step]; }
    const StepPattern& getPattern(int track, int step) const { return _patterns[track][step]; }
    uint8_t getDividerIndex(int track, int step) const { return _dividerIndices[track][step]; }
    const Divider& getDivider(int track, int step) const { return _dividers[_dividerIndices[track][step]]; }
    uint16_t getDividerValue(int track, int step) const { return _dividers[_dividerIndices[track][step]].value; }
    
    // Setters
    void setLength(int length);
    void setDividerIndex(int track, int step, uint8_t index);
    
    // Pattern operations
    void rotatePattern(int track, int step, int increment);
    void randomizePattern(int track, int step);
    void clearPattern(int track, int step);
    
    // Copy/Paste
    void copyPattern(int track, int step);
    void pastePattern(int track, int step);
    bool hasCopiedData() const { return _hasCopiedData; }
    
    // Divider utilities
    static const Divider& getDividerByIndex(int index) { return _dividers[index]; }
    static int getNumDividers() { return NUM_DIVIDERS; }
    static uint16_t getDividerValueByIndex(int index) { return _dividers[index].value; }
    static const char* getDividerTextByIndex(int index) { return _dividers[index].text; }

    // Step analysis
    uint32_t getStepLengthInTicks(int step) const;
    int getStepLongestPatternIndex(int step) const;

    // Quantizer management
    Quantizer& getQuantizer(int track);
    const Quantizer& getQuantizer(int track) const;
    void setQuantizer(int track, const Quantizer& quantizer);
   
private:
    static const int NUM_MELODIC_TRACKS = 4;  // Tracks 0-3 have CV output
    static const int NUM_DIVIDERS = 9;
    static const Divider _dividers[NUM_DIVIDERS];
    
    StepPattern _patterns[NUM_TRACKS][MAX_SONG_LENGTH];
    uint8_t _dividerIndices[NUM_TRACKS][MAX_SONG_LENGTH];
    int _length;
    
    // Copy/Paste buffers
    StepPattern _copiedPattern;
    uint8_t _copiedDivider;
    bool _hasCopiedData;
    
    // Quantizers for melodic tracks (0-3) only
    Quantizer _quantizers[4];
};

#endif