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

uint16_t Quantizer::quantize(uint16_t rawCV) const {
    if (_notes.empty()) {
        return rawCV;  // No notes to quantize to
    }
    
    uint8_t bestIdx = 0;
    uint16_t bestDiff = 0xFFFF;
    
    for (uint8_t i = 0; i < _notes.size(); i++) {
        uint16_t noteDac = _notes[i].dacValue;
        uint16_t diff = (rawCV > noteDac) ? (rawCV - noteDac) : (noteDac - rawCV);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    
    return _notes[bestIdx].dacValue;
}

void Quantizer::rebuild(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes) {
    generateChromatic(startDAC, endDAC, numNotes);
}

void Quantizer::generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes) {
    _notes.clear();
    _notes.reserve(numNotes);
    
    uint16_t step = (endDAC - startDAC) / (numNotes - 1);
    
    for (uint8_t i = 0; i < numNotes; i++) {
        Note note;
        note.dacValue = startDAC + (i * step);
        
        uint8_t noteIndex = i % 12;
        uint8_t octave = 4 + (i / 12);
        
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