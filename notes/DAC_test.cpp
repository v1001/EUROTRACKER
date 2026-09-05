#include <Arduino.h>
#include <Wire.h>

#define MCP4728_ADDR 0x60

// Write to a specific channel and update the output immediately.
// channel: 0..3
// value:   0..4095
void setDACChannel(uint8_t channel, uint16_t value) {
    // The MCP4728 expects a command byte where:
    // - bits 7-3 = 0b01000 (0x40)  →  "MULTI‑WRITE" command
    // - bit 2-1 = channel number   (0,1,2,3)
    // - bit 0   = UDAC flag        0 = update immediately, 1 = wait for LDAC pin
    uint8_t cmd = 0x40 | (channel << 1) | 0;   // UDAC = 0

    // Split the 12‑bit value into high‑nibble (bits 11..8) and low‑byte (bits 7..0)
    uint8_t dataH = (value >> 8) & 0x0F;        // high nibble (only lower 4 bits are used)
    uint8_t dataL = value & 0xFF;               // low byte

    Wire.beginTransmission(MCP4728_ADDR);
    Wire.write(cmd);
    Wire.write(dataH);
    Wire.write(dataL);
    Wire.endTransmission();
}

void setup() {
    Wire.begin();
    Wire.setClock(400000);
    delay(100);
}

void loop() {
    // Triangle wave: 0 → 4095 → 0
    for (uint16_t v = 0; v <= 4095; v++) {
        setDACChannel(0, v);
        delayMicroseconds(50);
    }
    for (uint16_t v = 4095; v > 0; v--) {
        setDACChannel(0, v);
        delayMicroseconds(50);
    }
}