#include "GlobalSettings.h"

// Static member initialisation
GlobalSettings::ClockSource GlobalSettings::clockSource = GlobalSettings::SOURCE_INTERNAL;
uint16_t GlobalSettings::internalBPM = 1200;          // 120.0 BPM
uint8_t GlobalSettings::externalPPQN = 3;             // 24 PPQN
uint8_t GlobalSettings::clockOutDivider = 4;          // x1
bool GlobalSettings::autoplay = false;
bool GlobalSettings::syncStart = false;
uint8_t GlobalSettings::joystickSpeed = 5;
uint8_t GlobalSettings::brightness = 128;

const char* GlobalSettings::SETTINGS_FILE = "/global.settings";

void GlobalSettings::setDefaults() {
    clockSource = SOURCE_INTERNAL;
    internalBPM = 1200;
    externalPPQN = 3;
    clockOutDivider = 4;
    autoplay = false;
    syncStart = false;
    joystickSpeed = 5;
    brightness = 128;
}

void GlobalSettings::load() {
    if (!LittleFS.exists(SETTINGS_FILE)) {
        setDefaults();
        save();
        return;
    }

    File file = LittleFS.open(SETTINGS_FILE, FILE_READ);
    if (!file) {
        setDefaults();
        return;
    }

    // Read header first to decide layout
    uint32_t magic = 0;
    uint8_t version = 0;
    if (file.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic) ||
        file.read(&version, sizeof(version)) != sizeof(version) ||
        magic != MAGIC_NUMBER) {
        file.close();
        setDefaults();
        return;
    }

    file.seek(0);
    bool ok = false;

    if (version == 1) {
        SettingsDataV1 data;
        if (file.read((uint8_t*)&data, sizeof(data)) == sizeof(data)) {
            clockSource = static_cast<ClockSource>(data.clockSource);
            internalBPM = data.internalBPM;
            externalPPQN = data.externalPPQN;
            clockOutDivider = 4;   // default for x1 on migration
            autoplay = data.autoplay;
            syncStart = data.syncStart;
            joystickSpeed = data.joystickSpeed;
            brightness = data.brightness;
            ok = true;
        }
    } else if (version == CURRENT_VERSION) {
        SettingsData data;
        if (file.read((uint8_t*)&data, sizeof(data)) == sizeof(data)) {
            clockSource = static_cast<ClockSource>(data.clockSource);
            internalBPM = data.internalBPM;
            externalPPQN = data.externalPPQN;
            clockOutDivider = data.clockOutDivider;
            autoplay = data.autoplay;
            syncStart = data.syncStart;
            joystickSpeed = data.joystickSpeed;
            brightness = data.brightness;
            ok = true;
        }
    }

    file.close();

    if (!ok) {
        setDefaults();
    } else if (version != CURRENT_VERSION) {
        // Upgrade to v2 on next save
        save();
    }
}

void GlobalSettings::save() {
    SettingsData data;
    data.magic = MAGIC_NUMBER;
    data.version = CURRENT_VERSION;
    data.clockSource = static_cast<uint8_t>(clockSource);
    data.internalBPM = internalBPM;
    data.externalPPQN = externalPPQN;
    data.clockOutDivider = clockOutDivider;
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