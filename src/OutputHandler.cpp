#include "OutputHandler.h"

const int OutputHandler::DIGITAL_PINS[] = {12, 15, 2, 4, 13, 17};

OutputHandler::OutputHandler() : _dacChanged(false) {
    for (int i = 0; i < NUM_DIGITAL_OUTPUTS; i++) {
        digital_states[i] = LOW;
        pinMode(DIGITAL_PINS[i], OUTPUT);
        digitalWrite(DIGITAL_PINS[i], LOW);
    }
    for (int i = 0; i < 4; i++) {
        dac_values[i] = 2048;   // mid‑scale
    }
}

void OutputHandler::setAllDigitalOutputs(bool state) {
    for (int i = 0; i < NUM_DIGITAL_OUTPUTS; i++) {
        digital_states[i] = state;
        digitalWrite(DIGITAL_PINS[i], state ? HIGH : LOW);
    }
}

void OutputHandler::setAllDigitalOutputs() {
    for (int i = 0; i < NUM_DIGITAL_OUTPUTS; i++) {
        digitalWrite(DIGITAL_PINS[i], digital_states[i] ? HIGH : LOW);
    }
}

bool OutputHandler::setDigitalOutput(uint8_t pin_index, bool state) {
    if (pin_index >= NUM_DIGITAL_OUTPUTS) return false;
    digital_states[pin_index] = state;
    return true;
}

void OutputHandler::setDACChannel(uint8_t channel, uint16_t value) {
    if (channel >= 4) return;
    uint16_t clamped = constrain(value, 0, 4095);
    if (dac_values[channel] != clamped) {
        dac_values[channel] = clamped;
        _dacChanged = true;
    }
}

void OutputHandler::setDACChannelVoltage(uint8_t channel, float voltage) {
    uint16_t value = (uint16_t)((voltage * 4095.0f) / 3.3f);
    setDACChannel(channel, value);
}

uint16_t OutputHandler::getDACChannelValue(uint8_t channel) {
    return (channel < 4) ? dac_values[channel] : 0;
}

float OutputHandler::getDACChannelVoltage(uint8_t channel) {
    float v = (getDACChannelValue(channel) * 3.3f) / 4095.0f;
    return v;
}