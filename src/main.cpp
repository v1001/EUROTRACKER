#include <Arduino.h>
#include "DisplayManager.h"
#include "UserInput.h"
#include "SyncTimer.h"
#include "OutputHandler.h"
#include "TrackerApp.h"
#include "I2CGatekeeper.h"
#include "SongData.h"
#include <LittleFS.h>

const uint8_t CLOCK_PIN = 5;
static volatile bool clockOutputState = false;

DisplayManager display;
UserInput userInput(10);
SyncTimer syncTimer(CLOCK_PIN);
OutputHandler outputHandler;
TrackerApp trackerApp(display, userInput, outputHandler);
I2CGatekeeper i2cGatekeeper;

TaskHandle_t uiTaskHandle = NULL;
uint64_t lastStateChangeTime = 0;
const uint64_t STATE_CHANGE_COOLDOWN = 500;

volatile bool timerNeedsRestart = false;

uint8_t currentPage = 0;
uint8_t currentBlock = 0;
const uint8_t BLOCKS_PER_PAGE = 4;
const uint8_t BLOCK_WIDTH = 32;

uint64_t getQuarterNoteTime() {
    if (trackerApp.getClockSource() == GlobalSettings::SOURCE_INTERNAL) {
        return 60000000.0f / trackerApp.getInternalBPMFloat();
    } else {
        uint16_t ppqn = trackerApp.getExternalPPQNValue();
        return syncTimer.getBasePeriod() * ppqn;
    }
}

void IRAM_ATTR onTimerTick(uint16_t tickCount) {
    trackerApp.processClockTick(tickCount);

    if (trackerApp.getClockSource() == GlobalSettings::SOURCE_INTERNAL) {
        uint8_t idx = trackerApp.getClockOutDivider();
        if (idx >= SongData::getNumDividers()) idx = SongData::getNumDividers() - 1;
        uint16_t divValue = SongData::getDividerValueByIndex(idx);
        uint16_t toggleEvery = divValue / 2;
        if (toggleEvery > 0 && (tickCount % toggleEvery) == 0) {
            clockOutputState = !clockOutputState;
            digitalWrite(CLOCK_PIN, clockOutputState ? HIGH : LOW);
        }
    }
}

void startTimer() {
    syncTimer.destroy();
    if (trackerApp.getClockSource() == GlobalSettings::SOURCE_INTERNAL) {
        float frequencyHz = trackerApp.getInternalBPMFloat() / 60.0f;
        syncTimer.beginStandalone(192, onTimerTick, frequencyHz);
        pinMode(CLOCK_PIN, OUTPUT);
        clockOutputState = false;
        digitalWrite(CLOCK_PIN, LOW);
    } else {
        uint16_t ppqn = trackerApp.getExternalPPQNValue();
        syncTimer.begin(192, onTimerTick, ppqn);
    }
}

void uiTask(void* parameter) {
    while (1) {
        // Wait until Core 1 finishes sending the previous frame
        while (display.needsUpdate()) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        userInput.readInputs();
        display.clear();

        trackerApp.update(getQuarterNoteTime());

        // Signal that the frame is ready and swap buffers
        display.swapBuffers();

        static GlobalSettings::ClockSource lastClockSource = trackerApp.getClockSource();
        static uint16_t lastInternalBPM = trackerApp.getInternalBPM();
        static uint8_t lastExternalPPQN = trackerApp.getExternalPPQN();

        if (lastClockSource != trackerApp.getClockSource() ||
            lastInternalBPM != trackerApp.getInternalBPM() ||
            lastExternalPPQN != trackerApp.getExternalPPQN()) {

            lastClockSource = trackerApp.getClockSource();
            lastInternalBPM = trackerApp.getInternalBPM();
            lastExternalPPQN = trackerApp.getExternalPPQN();
            timerNeedsRestart = true;
        }
    }
}

void setup() {
    pinMode(25, INPUT);
    randomSeed(analogRead(25));

    // 1. Initialize I2C and physical display
    if (!i2cGatekeeper.begin(1000000)) {
        while (1);
    }
    i2cGatekeeper.displayInit();
    i2cGatekeeper.displayClear();

    // 2. Initialize framebuffers
    display.begin();

    // ==========================================
    // 3. DRAW AND PUSH SPLASH SCREEN IMMEDIATELY
    // ==========================================
    display.clear();
    display.setTextSize(TEXT_MEDIUM);
    display.setTextColor(display.colorWhite());
    
    // Draw text centered vertically
    display.printCentered("Euro", 4);
    display.printCentered("Tracker", 20);
    display.setTextSize(TEXT_SMALL);
    display.printCentered("v1.1.0", 48);
    
    // Swap to read buffer so it's ready for transmission
    display.swapBuffers();
    
    // Push directly to the physical screen in 32-byte chunks
    uint8_t* fb = display.getFramebuffer();
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < 128; col += 32) {
            i2cGatekeeper.displayWritePage(page, col, &fb[page * 128 + col], 32);
        }
    }
    
    // Clear the update flag so the main loop() doesn't immediately resend it
    display.clearUpdateFlag();
    
    // Hold the splash screen for exactly 1 second
    delay(1000);
    // ==========================================

    // 4. Initialize inputs (The 1-second delay above replaces the old delay(500))
    userInput.begin();
    
    outputHandler.setAllDigitalOutputs(false);
    for (int ch = 0; ch < 4; ch++) {
        outputHandler.setDACChannel(ch, 0);
    }

    pinMode(userInput.save_button.pin, INPUT_PULLUP);
    bool resetRequested = (digitalRead(userInput.save_button.pin) == LOW);

    // 5. Initialize Filesystem and App
    if (!LittleFS.begin(true)) {
        display.clear();
        display.printCentered("MEMORY ERROR", 30);
        display.swapBuffers();
        
        // Push error screen
        uint8_t* err_fb = display.getFramebuffer();
        for (uint8_t page = 0; page < 8; page++) {
            for (uint8_t col = 0; col < 128; col += 32) {
                i2cGatekeeper.displayWritePage(page, col, &err_fb[page * 128 + col], 32);
            }
        }
        while (1) { delay(100); } // Halt on error
    } else {
        trackerApp.begin(resetRequested);
        startTimer();
        xTaskCreatePinnedToCore(uiTask, "UITask", 8192, NULL, 1, &uiTaskHandle, 0);
    }
}

void loop() {
    if (timerNeedsRestart) {
        timerNeedsRestart = false;
        startTimer();
    }

    trackerApp.processOutputs();

    for (int ch = 0; ch < 4; ch++) {
        i2cGatekeeper.setDAC(ch, outputHandler.getDACValue(ch));
    }

    outputHandler.setAllDigitalOutputs();

    // Use the safe, atomic needsUpdate() check
    if (display.needsUpdate()) {
        uint8_t* framebuffer = display.getFramebuffer(); // Points to _readBuffer now

        uint8_t col_start = currentBlock * BLOCK_WIDTH;
        const uint8_t* blockData = &framebuffer[currentPage * DISPLAY_WIDTH + col_start];
        i2cGatekeeper.displayWritePage(currentPage, col_start, blockData, BLOCK_WIDTH);

        currentBlock++;
        if (currentBlock >= BLOCKS_PER_PAGE) {
            currentBlock = 0;
            currentPage++;
            if (currentPage >= 8) {
                currentPage = 0;
                display.clearUpdateFlag(); // Atomically sets _needsUpdate to false
            }
        }
    }

    delayMicroseconds(20);
}