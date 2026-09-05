#include <Arduino.h>
#include <Wire.h>

#define MCP4728_ADDR 0x60
#define SH1106_ADDR  0x3C

#define COLUMNS_NUMBER 16

// ========== DAC ==========
void setDACChannel(uint8_t channel, uint16_t value) {
  uint8_t cmd = 0x40 | (channel << 1) | 0;   // multi‑write, update immediately
  uint8_t dataH = (value >> 8) & 0x0F;
  uint8_t dataL = value & 0xFF;
  Wire.beginTransmission(MCP4728_ADDR);
  Wire.write(cmd);
  Wire.write(dataH);
  Wire.write(dataL);
  Wire.endTransmission();
}

// ========== SH1106 low‑level ==========
void sh1106_write_cmd(uint8_t cmd) {
  Wire.beginTransmission(SH1106_ADDR);
  Wire.write(0x00);   // command mode
  Wire.write(cmd);
  Wire.endTransmission();
}

void sh1106_write_page(uint8_t page, uint8_t col_start, uint8_t *data, uint8_t len) {
  // Set page and column address
  Wire.beginTransmission(SH1106_ADDR);
  Wire.write(0x00);
  Wire.write(0xB0 | (page & 0x07));
  Wire.write(col_start & 0x0F);
  Wire.write(0x10 | ((col_start >> 4) & 0x0F));
  Wire.endTransmission();

  // Send pixel data
  Wire.beginTransmission(SH1106_ADDR);
  Wire.write(0x40);
  Wire.write(data, len);
  Wire.endTransmission();
}

void sh1106_init() {
  sh1106_write_cmd(0xAE);   // Display OFF
  sh1106_write_cmd(0x20);   // Set memory addressing mode
  sh1106_write_cmd(0x10);   // Page addressing mode
  sh1106_write_cmd(0xB0);   // Set page start address (page 0)
  sh1106_write_cmd(0xC8);   // Set COM output scan direction
  sh1106_write_cmd(0x00);   // Set low column address (0x00; some 1.3" displays need 0x02)
  sh1106_write_cmd(0x10);   // Set high column address
  sh1106_write_cmd(0x40);   // Set start line address
  sh1106_write_cmd(0x81);   // Set contrast control register
  sh1106_write_cmd(0xFF);   // Contrast value
  sh1106_write_cmd(0xA1);   // Set segment re‑map (0 to 127)
  sh1106_write_cmd(0xA6);   // Set normal display (not inverted)
  sh1106_write_cmd(0xA8);   // Set multiplex ratio
  sh1106_write_cmd(0x3F);   // 1/64 duty
  sh1106_write_cmd(0xA4);   // Output follows RAM content
  sh1106_write_cmd(0xD3);   // Set display offset
  sh1106_write_cmd(0x00);   // No offset
  sh1106_write_cmd(0xD5);   // Set display clock divide ratio
  sh1106_write_cmd(0xF0);   // Divide ratio
  sh1106_write_cmd(0xD9);   // Set pre‑charge period
  sh1106_write_cmd(0x22);   // Pre‑charge period
  sh1106_write_cmd(0xDA);   // Set COM pins hardware configuration
  sh1106_write_cmd(0x12);   // COM pins configuration
  sh1106_write_cmd(0xDB);   // Set VCOMH deselect level
  sh1106_write_cmd(0x20);   // 0x77xVcc
  sh1106_write_cmd(0x8D);   // Charge pump set
  sh1106_write_cmd(0x14);   // Enable charge pump
  delay(100);               // Stabilise
  sh1106_write_cmd(0xAF);   // Display ON
}

void sh1106_clear_screen() {
  uint8_t blank[128];
  memset(blank, 0x00, 128);
  for (uint8_t page = 0; page < 8; page++) {
    sh1106_write_page(page, 0, blank, 128);
  }
}

void draw_square_8x8(uint8_t page, uint8_t col_start, bool white, uint8_t width) {
  uint8_t pattern[width];
  memset(pattern, white ? 0xFF : 0x00, width);
  sh1106_write_page(page, col_start, pattern, width);
}

// ========== Global dot position ==========
uint8_t dotColumn = 0;       // column index (0–120, step 8 pixels)

void setup() {

  Wire.begin();
  Wire.setClock(400000);      // 1 MHz I2C
  delay(100);

  // Check MCP4728
  Wire.beginTransmission(MCP4728_ADDR);
  Wire.endTransmission();

  // Check SH1106
  Wire.beginTransmission(SH1106_ADDR);
  Wire.endTransmission();

  sh1106_init();
  sh1106_clear_screen();
}

void loop() {
  // Triangle wave: 0 → 4095
    setDACChannel(0, 0);
    setDACChannel(1, 0);
    setDACChannel(2, 0);
    setDACChannel(3, 0);
    draw_square_8x8(0, 0, true, COLUMNS_NUMBER);          // clear old position

  // After rising ramp, move dot one step to the right (clear old, draw new)


  // Triangle wave: 4095 → 0
    setDACChannel(0, 4095);
    setDACChannel(1, 4095);
    setDACChannel(2, 4095);
    setDACChannel(3, 4095);
    draw_square_8x8(0, 0, false, COLUMNS_NUMBER);

  // After falling ramp, move dot again (optional – you can move only once per full wave)
  // To keep one step per full wave, we move again here.
  // If you want one step per full cycle, remove the second move.
}