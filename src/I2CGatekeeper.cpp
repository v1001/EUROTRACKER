#include "I2CGatekeeper.h"

I2CGatekeeper::I2CGatekeeper() {
    Wire.begin();
}

bool I2CGatekeeper::begin(uint32_t clockSpeed) {
    Wire.setClock(clockSpeed);
    delay(100);
    
    // Quick check if devices are present (optional)
    Wire.beginTransmission(0x60);  // MCP4728
    uint8_t err = Wire.endTransmission();
    if (err != 0) return false;
    
    Wire.beginTransmission(0x3C);  // SH1106
    err = Wire.endTransmission();
    if (err != 0) return false;
    
    return true;
}

// ========== DAC ==========
void I2CGatekeeper::setDAC(uint8_t channel, uint16_t value) {
    if (channel > 3) return;
    uint8_t cmd = 0x40 | (channel << 1) | 0;   // multi‑write, update immediately
    uint8_t dataH = (value >> 8) & 0x0F;
    uint8_t dataL = value & 0xFF;
    Wire.beginTransmission(0x60);
    Wire.write(cmd);
    Wire.write(dataH);
    Wire.write(dataL);
    Wire.endTransmission();
}

void I2CGatekeeper::setAllDAC(const uint16_t values[4]) {
    // Fast write to all channels (command 0x58)
    uint8_t buffer[9];
    buffer[0] = 0x58;
    for (int i = 0; i < 4; i++) {
        buffer[1 + i*2] = (values[i] >> 8) & 0x0F;
        buffer[2 + i*2] = values[i] & 0xFF;
    }
    Wire.beginTransmission(0x60);
    Wire.write(buffer, 9);
    Wire.endTransmission();
}

// ========== SH1106 low‑level ==========
void I2CGatekeeper::writeCommand(uint8_t cmd) {
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);   // command mode
    Wire.write(cmd);
    Wire.endTransmission();
}

void I2CGatekeeper::writeData(const uint8_t* data, uint16_t len) {
    Wire.beginTransmission(0x3C);
    Wire.write(0x40);   // data mode
    Wire.write(data, len);
    Wire.endTransmission();
}

void I2CGatekeeper::setPageColumn(uint8_t page, uint8_t col_start) {
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);                               // command mode
    Wire.write(0xB0 | (page & 0x07));               // page address
    Wire.write(col_start & 0x0F);                   // lower column start
    Wire.write(0x10 | ((col_start >> 4) & 0x0F));   // higher column start
    Wire.endTransmission();
}

void I2CGatekeeper::displayInit() {
    writeCommand(0xAE);   // Display OFF
    writeCommand(0x20);   // Set memory addressing mode
    writeCommand(0x10);   // Page addressing mode
    writeCommand(0xB0);   // Set page start address (page 0)
    writeCommand(0xC8);   // Set COM output scan direction
    writeCommand(0x00);   // Set low column address (0x00; some 1.3" displays need 0x02)
    writeCommand(0x10);   // Set high column address
    writeCommand(0x40);   // Set start line address
    writeCommand(0x81);   // Set contrast control register
    writeCommand(0xFF);   // Contrast value
    writeCommand(0xA1);   // Set segment re‑map (0 to 127)
    writeCommand(0xA6);   // Set normal display (not inverted)
    writeCommand(0xA8);   // Set multiplex ratio
    writeCommand(0x3F);   // 1/64 duty
    writeCommand(0xA4);   // Output follows RAM content
    writeCommand(0xD3);   // Set display offset
    writeCommand(0x00);   // No offset
    writeCommand(0xD5);   // Set display clock divide ratio
    writeCommand(0xF0);   // Divide ratio
    writeCommand(0xD9);   // Set pre‑charge period
    writeCommand(0x22);   // Pre‑charge period
    writeCommand(0xDA);   // Set COM pins hardware configuration
    writeCommand(0x12);   // COM pins configuration
    writeCommand(0xDB);   // Set VCOMH deselect level
    writeCommand(0x20);   // 0x77xVcc
    writeCommand(0x8D);   // Charge pump set
    writeCommand(0x14);   // Enable charge pump
    delay(100);           // Stabilise
    writeCommand(0xA4);
    writeCommand(0xAF);   // Display ON
}

void I2CGatekeeper::displayClear() {
    setPageColumn(0, 0);           // Set starting address
    uint8_t blank[1024] = {0};     // Prepare the full zero buffer
    writeData(blank, 1024);        // Send 1024 zeros to clear GDDRAM
}

void I2CGatekeeper::displayWritePage(uint8_t page, uint8_t col_start, const uint8_t* data, uint8_t len) {
    uint8_t actual_col_start = col_start + 2;
    setPageColumn(page, actual_col_start);
    writeData(data, len);
}