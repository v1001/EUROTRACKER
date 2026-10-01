#ifndef GLOBAL_SETTINGS_H
#define GLOBAL_SETTINGS_H

#include <Arduino.h>
#include <LittleFS.h>

class GlobalSettings {
public:
    enum ClockSource : uint8_t {
        SOURCE_INTERNAL,
        SOURCE_EXTERNAL
    };

    static ClockSource clockSource;
    static uint16_t internalBPM;      // stored as BPM * 10 (e.g., 1200 = 120.0 BPM)
    static uint8_t externalPPQN;      // 0=1, 1=2, 2=4, 3=24, 4=48 (clock input rate)
    static uint8_t clockOutDivider;   // 0..8, index into SongData divider table
    static bool autoplay;
    static bool syncStart;
    static uint8_t joystickSpeed;     // deprecated, kept for file format compatibility
    static uint8_t brightness;        // deprecated, kept for file format compatibility

    static void load();
    static void save();

    static uint16_t getExternalPPQNValue();

private:
    // Version 1 layout (no clockOutDivider)
    struct SettingsDataV1 {
        uint32_t magic;
        uint8_t version;
        uint8_t clockSource;
        uint16_t internalBPM;
        uint8_t externalPPQN;
        bool autoplay;
        bool syncStart;
        uint8_t joystickSpeed;
        uint8_t brightness;
    };

    // Version 2 layout (current)
    struct SettingsData {
        uint32_t magic;
        uint8_t version;
        uint8_t clockSource;
        uint16_t internalBPM;
        uint8_t externalPPQN;
        uint8_t clockOutDivider;
        bool autoplay;
        bool syncStart;
        uint8_t joystickSpeed;
        uint8_t brightness;
    };

    static const uint32_t MAGIC_NUMBER = 0x54475242;   // "TGRA"
    static const uint8_t  CURRENT_VERSION = 2;
    static const char*    SETTINGS_FILE;

    static void setDefaults();
};

#endif