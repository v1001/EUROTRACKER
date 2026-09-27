#include "GlobalSettings.h"

// Static member initialisation
GlobalSettings::ClockSource GlobalSettings::clockSource = GlobalSettings::SOURCE_INTERNAL;
uint16_t GlobalSettings::internalBPM = 1200;          // 120.0 BPM
uint8_t GlobalSettings::externalPPQN = 3;             // 24 PPQN
bool GlobalSettings::autoplay = false;
bool GlobalSettings::syncStart = false;
uint8_t GlobalSettings::joystickSpeed = 5;
uint8_t GlobalSettings::brightness = 128;

const char* GlobalSettings::SETTINGS_FILE = "/global.settings";

void GlobalSettings::setDefaults() {
    clockSource = SOURCE_INTERNAL;
    internalBPM = 1200;          // 120.0 BPM
    externalPPQN = 3;
    autoplay = false;
    syncStart = false;
    joystickSpeed = 5;
    brightness = 128;
}

void GlobalSettings::load() {
    // SPIFFS is expected to be already mounted by setup()
    if (!LittleFS.exists(SETTINGS_FILE)) {
        setDefaults();
        save();  // create file with defaults
        return;
    }
    
    File file = LittleFS.open(SETTINGS_FILE, FILE_READ);
    if (!file) {
        setDefaults();
        return;
    }
    
    size_t fileSize = file.size();
    
    SettingsData data;
    if (file.read((uint8_t*)&data, sizeof(data)) == sizeof(data) &&
        data.magic == MAGIC_NUMBER) {
        if (data.version == CURRENT_VERSION) {
            clockSource = static_cast<ClockSource>(data.clockSource);
            internalBPM = data.internalBPM;
            externalPPQN = data.externalPPQN;
            autoplay = data.autoplay;
            syncStart = data.syncStart;
            joystickSpeed = data.joystickSpeed;
            brightness = data.brightness;
        } else {
            // Unknown version – reset
            setDefaults();
        }
    } else {
        // Corrupt file – reset
        setDefaults();
    }
    
    file.close();
}

void GlobalSettings::save() {
    // SPIFFS is expected to be already mounted
    SettingsData data;
    data.magic = MAGIC_NUMBER;
    data.version = CURRENT_VERSION;
    data.clockSource = static_cast<uint8_t>(clockSource);
    data.internalBPM = internalBPM;
    data.externalPPQN = externalPPQN;
    data.autoplay = autoplay;
    data.syncStart = syncStart;
    data.joystickSpeed = joystickSpeed;
    data.brightness = brightness;
    
    File file = LittleFS.open(SETTINGS_FILE, FILE_WRITE);
    if (file) {
        file.write((uint8_t*)&data, sizeof(data));
        file.close();
    }
}

uint16_t GlobalSettings::getExternalPPQNValue() {
    const uint16_t ppqnValues[] = {1, 2, 4, 24, 48};
    if (externalPPQN >= 5) return 24;
    return ppqnValues[externalPPQN];
}