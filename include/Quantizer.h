#ifndef QUANTIZER_H
#define QUANTIZER_H

#include <Arduino.h>
#include <vector>

class Quantizer {
public:
    struct Note {
        char name[4];        // "C", "C#", "D", etc.
        uint16_t dacValue;   // 0-4095
    };
    
    // Constructor - creates chromatic scale C4 to C9 (61 notes)
    Quantizer();
    
    // Get note by index (returns last note if index out of bounds)
    const Note& getNote(uint8_t index) const;
    
    // Get note name by index
    const char* getNoteName(uint8_t index) const;
    
    // Get DAC value by index
    uint16_t getNoteDAC(uint8_t index) const;
    
    // Get number of notes in quantizer
    uint8_t getNumNotes() const { return _notes.size(); }
    
    // Clear and rebuild quantizer with custom range
    void rebuild(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes);

    void clearNotes();
    void addNote(const char* name, uint16_t dacValue);
    
private:
    static const char* NOTE_NAMES[12];
    std::vector<Note> _notes;
    
    void generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes);
};

#endif