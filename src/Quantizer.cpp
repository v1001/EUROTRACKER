#include "Quantizer.h"
#include <cstring>

// Natural notes: lowercase, sharp notes: uppercase (one character each)
const char* Quantizer::NOTE_NAMES[12] = {"c", "C", "d", "D", "e", "f", "F", "g", "G", "a", "A", "b"};

Quantizer::Quantizer() 
    : _startDAC(0), _endDAC(4095), _numNotes(61) {
    generateChromatic(0, 4095, 61);
    _scaleActive = true;
}

void Quantizer::generateChromatic(uint16_t startDAC, uint16_t endDAC, uint8_t numNotes) {
    _notes.clear();
    _notes.reserve(numNotes);
    _startDAC = startDAC;
    _endDAC = endDAC;
    _numNotes = numNotes;
    
    if (numNotes < 2) return;
    
    uint16_t step = (endDAC - startDAC) / (numNotes - 1);
    
    for (uint8_t i = 0; i < numNotes; i++) {
        Note note;
        note.dacValue = startDAC + (i * step);
        note.inScale = true;
        
        uint8_t noteIndex = i % 12;
        uint8_t octave = 4 + (i / 12);
        
        // Format: note name (one char) + octave (one digit)
        // e.g., "c4", "C4", "d4", "D4", "e4", "f4", "F4", ...
        snprintf(note.name, sizeof(note.name), "%s%d", NOTE_NAMES[noteIndex], octave);
        
        _notes.push_back(note);
    }
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
        return rawCV;
    }
    
    // If scale is inactive, quantize to nearest note (chromatic)
    if (!_scaleActive) {
        uint8_t idx = getNoteIndex(rawCV);
        return _notes[idx].dacValue;
    }
    
    // Scale is active: find nearest note that is in scale
    // First, check if there is at least one active note
    bool hasActiveNote = false;
    for (const auto& note : _notes) {
        if (note.inScale) {
            hasActiveNote = true;
            break;
        }
    }
    if (!hasActiveNote) {
        return rawCV;  // No active notes – return unquantized
    }
    
    uint8_t bestIdx = 0;
    uint16_t bestDiff = 0xFFFF;
    
    for (uint8_t i = 0; i < _notes.size(); i++) {
        if (!_notes[i].inScale) continue;  // skip inactive notes
        
        uint16_t noteDac = _notes[i].dacValue;
        uint16_t diff = (rawCV > noteDac) ? (rawCV - noteDac) : (noteDac - rawCV);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    
    return _notes[bestIdx].dacValue;
}

uint8_t Quantizer::getNoteIndex(uint16_t rawCV) const {
    if (_notes.empty()) {
        return 0;
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
    
    return bestIdx;
}

void Quantizer::clearNotes() {
    _notes.clear();
}

void Quantizer::addNote(const char* name, uint16_t dacValue) {
    Note note;
    strncpy(note.name, name, sizeof(note.name) - 1);
    note.name[sizeof(note.name) - 1] = '\0';
    note.dacValue = dacValue;
    note.inScale = true;
    _notes.push_back(note);
}