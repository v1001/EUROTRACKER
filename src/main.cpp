#include <Arduino.h>
#include "DisplayManager.h"
#include "UserInput.h"
#include "SyncTimer.h"
#include "OutputHandler.h"
#include "TrackerApp.h"
#include "I2CGatekeeper.h"
#include <SPIFFS.h>


// Global instances
DisplayManager display;
UserInput userInput(10);
SyncTimer syncTimer(5);
OutputHandler outputHandler;
TrackerApp trackerApp(display, userInput, outputHandler);
I2CGatekeeper i2cGatekeeper;

TaskHandle_t uiTaskHandle = NULL;
uint64_t lastStateChangeTime = 0;
const uint64_t STATE_CHANGE_COOLDOWN = 500;

// Timer restart flag
volatile bool timerNeedsRestart = false;

// Display update state machine
uint8_t currentPage = 0;
uint8_t currentBlock = 0;
const uint8_t BLOCKS_PER_PAGE = 4;
const uint8_t BLOCK_WIDTH = 32;  // 128 / 4 = 32 columns per block

uint64_t getQuarterNoteTime() {
    if (trackerApp.getClockSource() == GlobalSettings::SOURCE_INTERNAL) {
        return 60000000.0f / trackerApp.getInternalBPMFloat();
    } else {
        return syncTimer.getBasePeriod();      // time between external pulses (µs)
    }
}

void IRAM_ATTR onTimerTick(uint16_t tickCount) {
    trackerApp.processClockTick(tickCount);
}

void startTimer() {
    syncTimer.destroy();
    if (trackerApp.getClockSource() == GlobalSettings::SOURCE_INTERNAL) {
        float frequencyHz = trackerApp.getInternalBPMFloat() / 60.0f;
        syncTimer.beginStandalone(192, onTimerTick, frequencyHz);
    } else {
        uint16_t ppqn = trackerApp.getExternalPPQNValue();
        syncTimer.begin(192, onTimerTick, ppqn);
    }
}

void uiTask(void* parameter) {
    while (1) {
        // Wait until the previous display frame is fully sent
        while (display.needsUpdate()) {
            vTaskDelay(pdMS_TO_TICKS(1));   // yield to I2C gatekeeper task
        }

        userInput.readInputs();
        display.clear();

        trackerApp.update(getQuarterNoteTime());

        // Signal that a new frame is ready to be sent
        display.update();

        // Check if timer settings changed and set restart flag
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

    // Initialize I2C gatekeeper
    if (!i2cGatekeeper.begin(1000000)) {
        while (1);   // I2C devices not found – halt
    }

    // Initialize display hardware
    i2cGatekeeper.displayInit();
    i2cGatekeeper.displayClear();

    // Initialize display framebuffer
    display.begin();

    userInput.begin();
    delay(500);
    userInput.calibrateJoystick();
    outputHandler.setAllDigitalOutputs(false);

    for (int ch = 0; ch < 4; ch++) {
        outputHandler.setDACChannel(ch, 0);
    }

    // Check if save button is pressed during startup
    pinMode(userInput.save_button.pin, INPUT_PULLUP);
    bool resetRequested = (digitalRead(userInput.save_button.pin) == LOW);

    if (!SPIFFS.begin(true)) {
        display.print("Memory Error");
    }else{
        trackerApp.begin(resetRequested);
        startTimer();

        xTaskCreatePinnedToCore(uiTask, "UITask", 8192, NULL, 1, &uiTaskHandle, 0);
    }
}

void loop() {
    // Check if timer needs restart (settings changed)
    if (timerNeedsRestart) {
        timerNeedsRestart = false;
        startTimer();
    }

    // Real‑time sequencer processing (Core 1)
    trackerApp.processOutputs();

    // I2C GATEKEEPER OPERATIONS (one per loop iteration, non‑blocking)
    // Priority 1: Update DAC – send all four channels
    for (int ch = 0; ch < 4; ch++) {
        i2cGatekeeper.setDAC(ch, outputHandler.getDACValue(ch));
    }

    outputHandler.setAllDigitalOutputs();

    // Priority 2: Send one display block if a frame is pending
    if (display.needsUpdate()) {
        uint8_t* framebuffer = display.getFramebuffer();

        uint8_t col_start = currentBlock * BLOCK_WIDTH;
        const uint8_t* blockData = &framebuffer[currentPage * DISPLAY_WIDTH + col_start];
        i2cGatekeeper.displayWritePage(currentPage, col_start, blockData, BLOCK_WIDTH);

        currentBlock++;
        if (currentBlock >= BLOCKS_PER_PAGE) {
            currentBlock = 0;
            currentPage++;
            if (currentPage >= 8) {
                currentPage = 0;
                display.clearUpdateFlag();   // all pages sent
            }
        }
    }

    delayMicroseconds(20);
}