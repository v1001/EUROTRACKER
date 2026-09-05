#ifndef OUTPUT_HANDLER_H
#define OUTPUT_HANDLER_H

#include <Arduino.h>

class OutputHandler {
public:
    OutputHandler();

    // Digital outputs (just set pins directly)
    bool setDigitalOutput(uint8_t pin_index, bool state);
    void setAllDigitalOutputs(bool state);
    void setAllDigitalOutputs();

    // DAC outputs – only store values, no I2C
    void setDACChannel(uint8_t channel, uint16_t value);
    void setDACChannelVoltage(uint8_t channel, float voltage);
    uint16_t getDACChannelValue(uint8_t channel);
    float getDACChannelVoltage(uint8_t channel);

    // For I2C task – check if any DAC channel changed
    bool hasDACChanged() const { return _dacChanged; }
    void clearDACChanged() { _dacChanged = false; }

    // Access DAC values for the I2C task
    uint16_t getDACValue(uint8_t channel) const { return (channel < 4) ? dac_values[channel] : 0; }

private:
    static const int DIGITAL_PINS[6];
    static const int NUM_DIGITAL_OUTPUTS = 6;

    bool digital_states[6];
    uint16_t dac_values[4];
    volatile bool _dacChanged;      // set when any DAC value changes
};

#endif