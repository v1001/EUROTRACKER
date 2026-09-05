#include "DisplayManager.h"
#include "DisplayGraphics.h"

// Standard ASCII 5x7 font (from Adafruit GFX library)

// ----------------------------------------------------------------------
// Constructor & Destructor
// ----------------------------------------------------------------------
DisplayManager::DisplayManager() {
    _framebuffer = nullptr;
    _isInitialized = false;
    _needsUpdate = false;
    _textSize = TEXT_SMALL;
    _textColor = FB_WHITE;
    _textBgColor = FB_BLACK;
    _cursorX = 0;
    _cursorY = 0;
}

DisplayManager::~DisplayManager() {
    end();
}

// ----------------------------------------------------------------------
// Framebuffer management
// ----------------------------------------------------------------------
bool DisplayManager::begin() {
    _framebuffer = new uint8_t[DISPLAY_FRAMEBUFFER_SIZE];
    if (!_framebuffer) {
        _isInitialized = false;
        return false;
    }
    memset(_framebuffer, 0, DISPLAY_FRAMEBUFFER_SIZE);
    clear();
    _isInitialized = true;
    _needsUpdate = true;
    return true;
}

void DisplayManager::end() {
    delete[] _framebuffer;
    _framebuffer = nullptr;
    _isInitialized = false;
}

void DisplayManager::clear() {
    if (!_framebuffer) return;
    memset(_framebuffer, 0, DISPLAY_FRAMEBUFFER_SIZE);
}

// ----------------------------------------------------------------------
// Pixel access
// ----------------------------------------------------------------------
void DisplayManager::setPixel(int16_t x, int16_t y, uint8_t color) {
    if (!_framebuffer) return;
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    uint16_t idx = (y / 8) * DISPLAY_WIDTH + x;   // page * 128 + column
    uint8_t bit = 1 << (y % 8);
    if (color) _framebuffer[idx] |= bit;
    else       _framebuffer[idx] &= ~bit;
}

uint8_t DisplayManager::getPixel(int16_t x, int16_t y) const {
    if (!_framebuffer) return 0;
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return 0;
    uint16_t idx = (y / 8) * DISPLAY_WIDTH + x;
    uint8_t bit = 1 << (y % 8);
    return (_framebuffer[idx] & bit) ? 1 : 0;
}

void DisplayManager::drawPixel(int16_t x, int16_t y, uint16_t color) {
    setPixel(x, y, color ? 1 : 0);
}

// ----------------------------------------------------------------------
// Drawing primitives (all use setPixel)
// ----------------------------------------------------------------------
void DisplayManager::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;
    int16_t x = x0, y = y0;
    while (1) {
        setPixel(x, y, color ? 1 : 0);
        if (x == x1 && y == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

void DisplayManager::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    drawLine(x + w - 1, y + h - 1, x, y + h - 1, color);
    drawLine(x, y + h - 1, x, y, color);
}

void DisplayManager::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t i = x; i < x + w; i++)
        drawLine(i, y, i, y + h - 1, color);
}

void DisplayManager::drawCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    while (x <= y) {
        setPixel(xc + x, yc + y, color);
        setPixel(xc - x, yc + y, color);
        setPixel(xc + x, yc - y, color);
        setPixel(xc - x, yc - y, color);
        setPixel(xc + y, yc + x, color);
        setPixel(xc - y, yc + x, color);
        setPixel(xc + y, yc - x, color);
        setPixel(xc - y, yc - x, color);
        if (d < 0) d = d + 4 * x + 6;
        else { d = d + 4 * (x - y) + 10; y--; }
        x++;
    }
}

void DisplayManager::fillCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {
    for (int16_t y = -r; y <= r; y++) {
        int16_t xLimit = sqrt(r * r - y * y);
        for (int16_t x = -xLimit; x <= xLimit; x++)
            setPixel(xc + x, yc + y, color);
    }
}

void DisplayManager::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void DisplayManager::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
    for (int16_t y = y0; y <= y2; y++) {
        int16_t xStart, xEnd;
        if (y < y1) {
            xStart = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            xEnd   = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        } else {
            xStart = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            xEnd   = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        }
        if (xStart > xEnd) std::swap(xStart, xEnd);
        for (int16_t x = xStart; x <= xEnd; x++)
            setPixel(x, y, color);
    }
}

// ----------------------------------------------------------------------
// Text rendering using FreeMono9pt7b font
// ----------------------------------------------------------------------
void DisplayManager::drawChar(int16_t x, int16_t y, unsigned char c) {
    if (!_framebuffer) return;
    if (c < 32 || c > 126) return;

    uint8_t idx = c - 32;
    for (int8_t col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int8_t row = 0; row < 8; row++) {
            if (line & (1 << row)) {
                for (uint8_t sy = 0; sy < _textSize; sy++) {
                    for (uint8_t sx = 0; sx < _textSize; sx++) {
                        setPixel(x + col * _textSize + sx, y + row * _textSize + sy, _textColor);
                    }
                }
            } else if (_textBgColor != _textColor) {
                for (uint8_t sy = 0; sy < _textSize; sy++) {
                    for (uint8_t sx = 0; sx < _textSize; sx++) {
                        setPixel(x + col * _textSize + sx, y + row * _textSize + sy, _textBgColor);
                    }
                }
            }
        }
    }
}

int16_t DisplayManager::getTextWidth(const char* text) {
    if (!text) return 0;
    return strlen(text) * 6 * _textSize;
}

int16_t DisplayManager::getTextHeight() {
    return 8 * _textSize;
}

void DisplayManager::applyAlignment(const char* text, int16_t& x, int16_t y, TextAlignment alignment) {
    int16_t tw = getTextWidth(text);
    if (alignment == ALIGN_CENTER) {
        x = (DISPLAY_WIDTH - tw) / 2;
    } else if (alignment == ALIGN_RIGHT) {
        x = DISPLAY_WIDTH - tw - x;
    }
}

void DisplayManager::print(const char* text) {
    if (!_framebuffer || !text) return;
    int16_t charWidth = 6 * _textSize;
    int16_t charHeight = 8 * _textSize;

    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            _cursorX = 0;
            _cursorY += charHeight;
        } else {
            // Wrap before drawing if needed
            if (_cursorX + charWidth > DISPLAY_WIDTH) {
                _cursorX = 0;
                _cursorY += charHeight;
            }
            drawChar(_cursorX, _cursorY, *c);
            _cursorX += charWidth;
        }
    }
}

void DisplayManager::println(const char* text) {
    print(text);
    _cursorX = 0;
    _cursorY += getTextHeight();
}

void DisplayManager::printAt(const char* text, int16_t x, int16_t y, TextAlignment alignment) {
    if (!text) return;
    applyAlignment(text, x, y, alignment);
    setCursor(x, y);
    print(text);
}

void DisplayManager::printCentered(const char* text, int16_t y) {
    printAt(text, 0, y, ALIGN_CENTER);
}

void DisplayManager::clearLine(uint8_t lineNumber) {
    int16_t y = lineNumber * getTextHeight();
    fillRect(0, y, DISPLAY_WIDTH, getTextHeight(), FB_BLACK);
}

// ----------------------------------------------------------------------
// Special symbols (using existing draw primitives)
// ----------------------------------------------------------------------
void DisplayManager::drawQuarterNote(int x, int y) {
    fillCircle(x + 1, y, 1, FB_WHITE);
    fillCircle(x + 2, y, 1, FB_WHITE);
    drawLine(x + 3, y, x + 3, y - 5, FB_WHITE);
    drawLine(x + 3, y - 5, x + 5, y - 3, FB_WHITE);
}

void DisplayManager::drawPlaySign(int x, int y) {
    fillTriangle(x, y, x, y + 8, x + 8, y + 4, FB_WHITE);
}

void DisplayManager::drawLoopSign(int x, int y) {
    drawCircle(x + 3, y + 4, 3, FB_WHITE);
    drawCircle(x + 9, y + 4, 3, FB_WHITE);
}

void DisplayManager::drawHourglass(int x, int y) {
    drawLine(x, y, x + 8, y, FB_WHITE);
    drawLine(x, y + 8, x + 8, y + 8, FB_WHITE);
    drawLine(x, y, x + 4, y + 4, FB_WHITE);
    drawLine(x + 4, y + 4, x, y + 8, FB_WHITE);
    drawLine(x + 8, y, x + 4, y + 4, FB_WHITE);
    drawLine(x + 4, y + 4, x + 8, y + 8, FB_WHITE);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color) {
    if (percent > 100) percent = 100;
    drawRect(x, y, w, h, color);
    int16_t fillW = (w - 2) * percent / 100;
    if (fillW > 0) fillRect(x + 1, y + 1, fillW, h - 2, color);
}

void DisplayManager::drawFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t thickness, uint16_t color) {
    for (uint8_t i = 0; i < thickness; i++)
        drawRect(x + i, y + i, w - 2 * i, h - 2 * i, color);
}