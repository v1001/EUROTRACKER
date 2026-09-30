#ifndef QUANTIZER_H
#define QUANTIZER_H

#include <Arduino.h>
#include <vector>

class Quantizer {
public:
    // Natural notes: lowercase, sharp notes: uppercase (one character each).
    // Shared by all modules that display or match pitch classes.
    static const char* NOTE_NAMES[12];

    struct Note {
        char name[5];          // e.g., "C4", "C#4"
        uint16_t dacValue;     // 0-4095
        bool inScale;          // true if this note is part of the active scale
    };

    // Constructor
    Quantizer();
    
    // Generate chromatic scale
    void generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes,
                       uint8_t startNoteIndex = 0, uint8_t startOctave = 4);
    
    // Get note by index
    const Note& getNote(uint8_t index) const;
    
    // Get note name by index
    const char* getNoteName(uint8_t index) const;
    
    // Get DAC value by index
    uint16_t getNoteDAC(uint8_t index) const;
    
    // Get number of notes in quantizer
    uint8_t getNumNotes() const { return _notes.size(); }
    
    // Quantize a raw CV value to the nearest note's DAC value
    uint16_t quantize(uint16_t rawCV) const;
    
    // Get the nearest note index for a raw CV value
    uint8_t getNoteIndex(uint16_t rawCV) const;
    
    // range parameters (for storage)
    uint16_t getStartDAC() const { return _startDAC; }
    uint16_t getEndDAC() const { return _endDAC; }
    uint8_t getNumNotesInScale() const { return _numNotes; }
    void setRangeParams(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes);
    
    // ---- NEW: Scale management ----
    void applyScaleIntervals(const uint8_t* intervals, uint8_t numIntervals, uint8_t rootIndex);
    uint8_t getScaleIndex() const { return _scaleIndex; }
    void setScaleIndex(uint8_t idx) { _scaleIndex = idx; }
    uint8_t getRootIndex() const { return _rootIndex; }
    void setRootIndex(uint8_t idx) { _rootIndex = idx; }
    void setNoteInScale(uint8_t index, bool inScale);
    bool inScaleMatchesApplied() const;
    
    // Clear and rebuild
    void clearNotes();
    void addNote(const char* name, uint16_t dacValue);

    void setNoteDAC(uint8_t index, uint16_t dac);
    void toggleNoteInScale(uint8_t index);
    static bool parseNoteName(const char* name, uint8_t& pitchClass, uint8_t& octave);
    
private:
    std::vector<Note> _notes;
    uint16_t _startDAC;
    uint16_t _endDAC;
    uint8_t _numNotes;
    
    // scale relevant parameters
    uint8_t _scaleIndex;
    uint8_t _rootIndex;
};

#endif // QUANTIZER_H