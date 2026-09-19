#ifndef QUANTIZER_H
#define QUANTIZER_H

#include <Arduino.h>
#include <vector>

class Quantizer {
public:
    struct Note {
        char name[5];          // e.g., "C4", "C#4"
        uint16_t dacValue;     // 0-4095
        bool inScale;          // true if this note is part of the active scale
    };
    
    // Constructor
    Quantizer();
    
    // Generate chromatic scale
    void generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes);
    
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
    
    // Get range parameters (for storage)
    uint16_t getStartDAC() const { return _startDAC; }
    uint16_t getEndDAC() const { return _endDAC; }
    uint8_t getNumNotesInScale() const { return _numNotes; }
    
    // ---- NEW: Scale management ----
    void applyScaleIntervals(const uint8_t* intervals, uint8_t numIntervals, uint8_t rootIndex);
    uint8_t getScaleIndex() const { return _scaleIndex; }
    void setScaleIndex(uint8_t idx) { _scaleIndex = idx; }
    uint8_t getRootIndex() const { return _rootIndex; }
    void setRootIndex(uint8_t idx) { _rootIndex = idx; }
    void setNoteInScale(uint8_t index, bool inScale);
    
    // Enable/disable scale (if disabled, all notes are considered active)
    void setScaleActive(bool active) { _scaleActive = active; }
    bool isScaleActive() const { return _scaleActive; }
    
    // Clear and rebuild
    void clearNotes();
    void addNote(const char* name, uint16_t dacValue);

    void setNoteDAC(uint8_t index, uint16_t dac);
    void toggleNoteInScale(uint8_t index);
    
private:
    static const char* NOTE_NAMES[12];
    std::vector<Note> _notes;
    uint16_t _startDAC;
    uint16_t _endDAC;
    uint8_t _numNotes;
    bool _scaleActive;
    
    // Helper to extract pitch class from a note name (e.g., "c" from "c4")
    void getPitchClass(const char* fullName, char* pitchClass) const;

    // scale relevant parameters
    uint8_t _scaleIndex;
    uint8_t _rootIndex;
};

#endif // QUANTIZER_H