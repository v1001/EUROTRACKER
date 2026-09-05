#ifndef I2C_GATEKEEPER_H
#define I2C_GATEKEEPER_H

#include <Arduino.h>
#include <Wire.h>

class I2CGatekeeper {
public:
    I2CGatekeeper();
    
    // Initialisation
    bool begin(uint32_t clockSpeed = 400000);
    
    // DAC (MCP4728)
    void setDAC(uint8_t channel, uint16_t value);
    void setAllDAC(const uint16_t values[4]);
    
    // Display (SH1106)
    void displayInit();
    void displayClear();
    void displayWritePage(uint8_t page, uint8_t col_start, const uint8_t* data, uint8_t len);
    
private:
    // Low‑level helpers
    void writeCommand(uint8_t cmd);
    void writeData(const uint8_t* data, uint16_t len);
    void setPageColumn(uint8_t page, uint8_t col_start);
};

#endif