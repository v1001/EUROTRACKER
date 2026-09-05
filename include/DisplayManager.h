#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <string.h>

// Display dimensions
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define DISPLAY_FRAMEBUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)  // 1024 bytes

// Text options
enum TextSize { TEXT_SMALL = 1, TEXT_MEDIUM = 2, TEXT_LARGE = 3 };
enum TextAlignment { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

// Framebuffer colors
#define FB_WHITE 1
#define FB_BLACK 0

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();

    // Allocate framebuffer (no hardware init)
    bool begin();
    void end();
    bool isReady() const { return _framebuffer != nullptr; }

    // Framebuffer access for I2C/SPI task
    uint8_t* getFramebuffer() { return _framebuffer; }
    uint16_t getFramebufferSize() const { return DISPLAY_FRAMEBUFFER_SIZE; }

    // Update flag (set by UI, cleared by hardware task)
    void update() { _needsUpdate = true; }
    bool needsUpdate() const { return _needsUpdate; }
    void clearUpdateFlag() { _needsUpdate = false; }

    // Clear entire framebuffer
    void clear();

    // Drawing primitives
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color);
    void fillCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

    // Text
    void setTextSize(TextSize size) { _textSize = size; }
    void setTextColor(uint16_t fg) { _textColor = fg; }
    void setTextColor(uint16_t fg, uint16_t bg) { _textColor = fg; _textBgColor = bg; }
    void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
    void getCursor(int16_t& x, int16_t& y) const { x = _cursorX; y = _cursorY; }
    void print(const char* text);
    void println(const char* text);
    void printAt(const char* text, int16_t x, int16_t y, TextAlignment alignment = ALIGN_LEFT);
    void printCentered(const char* text, int16_t y);
    void clearLine(uint8_t lineNumber);

    // Special symbols
    void drawQuarterNote(int x, int y);
    void drawPlaySign(int x, int y);
    void drawLoopSign(int x, int y);
    void drawHourglass(int x, int y);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color);
    void drawFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t thickness, uint16_t color);

    // Utility
    static int16_t width() { return DISPLAY_WIDTH; }
    static int16_t height() { return DISPLAY_HEIGHT; }
    uint16_t colorWhite() const { return FB_WHITE; }
    uint16_t colorBlack() const { return FB_BLACK; }

private:
    uint8_t* _framebuffer;
    volatile bool _needsUpdate;

    // Text state
    uint8_t _textSize;
    uint16_t _textColor;
    uint16_t _textBgColor;
    int16_t _cursorX;
    int16_t _cursorY;
    bool _isInitialized;

    // Internal helpers
    void setPixel(int16_t x, int16_t y, uint8_t color);
    uint8_t getPixel(int16_t x, int16_t y) const;
    void drawChar(int16_t x, int16_t y, unsigned char c);
    int16_t getTextWidth(const char* text);
    int16_t getTextHeight();
    void applyAlignment(const char* text, int16_t& x, int16_t y, TextAlignment alignment);
};

#endif