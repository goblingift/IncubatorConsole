#include "IncubatorSettings.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace {
constexpr char NVS_NAMESPACE[] = "incub";
constexpr char NVS_KEY[]       = "cfg1";  // bump when SettingsResponse layout changes
}

namespace SettingsStore {

SettingsResponse defaults(uint8_t deviceId) {
    SettingsResponse s = {};
    s.msgType        = SETTINGS_MSG_RESPONSE;
    s.deviceId       = deviceId;
    s.temperatureMin = 3600;  // 36.00 degC
    s.temperatureMax = 3900;
    s.humidityMin    = 4500;  // 45.00 %RH
    s.humidityMax    = 7000;
    s.co2Max         = 7100;
    s.lightAvgMax    = 500;
    s.pitchDegMax    = 150;   // 15.0 deg
    s.rollDegMax     = 150;
    s.soundMax       = 80;
    s.weightMin      = 0;
    s.weightMax      = 5000;
    s.voltageMin     = 1100;  // 11.00 V
    s.voltageMax     = 1300;
    s.currentMin     = 0;
    s.currentMax     = 200;   // 2.00 A
    s.waterLevelMin  = 10;
    s.waterLevelMax  = 100;
    s.settingsVersion = settingsCrc32(s);
    return s;
}

bool loadFromNvs(SettingsResponse& out) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return false;
    SettingsResponse s;
    size_t got = prefs.getBytes(NVS_KEY, &s, sizeof(s));
    prefs.end();
    if (got != sizeof(s) || !validate(s)) return false;
    out = s;
    return true;
}

bool saveToNvsIfChanged(const SettingsResponse& s) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
        Serial.println("[Settings] NVS open failed");
        return false;
    }
    SettingsResponse current;
    size_t got = prefs.getBytes(NVS_KEY, &current, sizeof(current));
    bool changed = (got != sizeof(current)) || (memcmp(&current, &s, sizeof(s)) != 0);
    bool ok = true;
    if (changed) {
        ok = prefs.putBytes(NVS_KEY, &s, sizeof(s)) == sizeof(s);
        Serial.println(ok ? "[Settings] Persisted to NVS"
                          : "[Settings] NVS write failed");
    }
    prefs.end();
    return ok;
}

bool validate(const SettingsResponse& s) {
    if (s.msgType != SETTINGS_MSG_RESPONSE) return false;
    if (s.settingsVersion != settingsCrc32(s)) return false;
    if (s.temperatureMin >= s.temperatureMax) return false;
    if (s.humidityMin >= s.humidityMax) return false;
    if (s.weightMin >= s.weightMax) return false;
    if (s.voltageMin >= s.voltageMax) return false;
    if (s.currentMin >= s.currentMax) return false;
    if (s.waterLevelMin >= s.waterLevelMax) return false;
    return true;
}

ControlSettings toFloats(const SettingsResponse& s) {
    ControlSettings c;
    c.temperatureMin = s.temperatureMin / 100.0f;
    c.temperatureMax = s.temperatureMax / 100.0f;
    c.humidityMin    = s.humidityMin / 100.0f;
    c.humidityMax    = s.humidityMax / 100.0f;
    c.co2Max         = s.co2Max;
    c.pitchDegMax    = s.pitchDegMax / 10.0f;
    c.rollDegMax     = s.rollDegMax / 10.0f;
    c.soundMax       = s.soundMax;
    c.weightMin      = s.weightMin;
    c.weightMax      = s.weightMax;
    c.voltageMin     = s.voltageMin / 100.0f;
    c.voltageMax     = s.voltageMax / 100.0f;
    c.currentMin     = s.currentMin / 100.0f;
    c.currentMax     = s.currentMax / 100.0f;
    c.waterLevelMin  = s.waterLevelMin;
    c.waterLevelMax  = s.waterLevelMax;
    return c;
}

}  // namespace SettingsStore
