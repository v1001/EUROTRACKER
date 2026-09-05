#include "Quantizer.h"

const char* Quantizer::NOTE_NAMES[12] = {"c", "C", "d", "D", "e", "f", "F", "g", "G", "a", "A", "b"};

Quantizer::Quantizer() {
    generateChromatic(0, 4095, 61);
}

const Quantizer::Note& Quantizer::getNote(uint8_t index) const {
    if (index >= _notes.size()) {
        return _notes[_notes.size() - 1];
    }
    return _notes[index];
}

const char* Quantizer::getNoteName(uint8_t index) const {
    if (index >= _notes.size()) {
        return _notes[_notes.size() - 1].name;
    }
    return _notes[index].name;
}

uint16_t Quantizer::getNoteDAC(uint8_t index) const {
    if (index >= _notes.size()) {
        return _notes[_notes.size() - 1].dacValue;
    }
    return _notes[index].dacValue;
}

void Quantizer::rebuild(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes) {
    generateChromatic(startDAC, endDAC, numNotes);
}

void Quantizer::generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes) {
    _notes.clear();
    _notes.reserve(numNotes);
    
    uint16_t step = (endDAC - startDAC) / (numNotes - 1);
    
    // Calculate starting octave based on note index distribution
    // C4 is note index 0 in our 61-note range (C4 to C9)
    // So octave = 4 + (noteIndex / 12)
    // Note: C4 = index 0, C#4 = index 1, ..., B4 = index 11, C5 = index 12, etc.
    
    for (uint8_t i = 0; i < numNotes; i++) {
        Note note;
        note.dacValue = startDAC + (i * step);
        
        uint8_t noteIndex = i % 12;
        uint8_t octave = 4 + (i / 12);  // i=0-11 -> octave 4, i=12-23 -> octave 5, etc.
        
        // Format: "C4", "C#4", "D4", etc.
        snprintf(note.name, sizeof(note.name), "%s%d", NOTE_NAMES[noteIndex], octave);
        
        _notes.push_back(note);
    }
}

void Quantizer::clearNotes() {
    _notes.clear();
}

void Quantizer::addNote(const char* name, uint16_t dacValue) {
    Note note;
    strncpy(note.name, name, 4);
    note.name[4] = '\0';
    note.dacValue = dacValue;
    _notes.push_back(note);
}