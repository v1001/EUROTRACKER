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
    
    // Public static members
    static ClockSource clockSource;
    static uint16_t internalBPM;      // stored as BPM * 10 (e.g., 1200 = 120.0 BPM)
    static uint8_t externalPPQN;      // 0=1, 1=2, 2=4, 3=24, 4=48
    static bool autoplay;
    static bool syncStart;
    static uint8_t joystickSpeed;     // 1-10
    static uint8_t brightness;        // 0-255
    
    // Load/save from SPIFFS – assumes SPIFFS is already mounted
    static void load();
    static void save();
    
    // Convert PPQN index to actual value
    static uint16_t getExternalPPQNValue();
    
private:
    // Old format (version 0) – for migration
    struct OldSettingsData {
        uint8_t clockSource;
        uint16_t internalBPM;      // raw integer BPM (before scaling)
        uint8_t externalPPQN;
        bool autoplay;
        bool syncStart;
        uint8_t joystickSpeed;
        uint8_t brightness;
        uint32_t magic;
    };
    
    // New format (version 1)
    struct SettingsData {
        uint32_t magic;
        uint8_t version;            // 1 = current
        uint8_t clockSource;
        uint16_t internalBPM;       // scaled ×10
        uint8_t externalPPQN;
        bool autoplay;
        bool syncStart;
        uint8_t joystickSpeed;
        uint8_t brightness;
    };
    
    static const uint32_t MAGIC_NUMBER = 0x54475242;   // "TGRA"
    static const uint8_t  CURRENT_VERSION = 1;
    static const char*    SETTINGS_FILE;
    
    static void setDefaults();
};

#endif