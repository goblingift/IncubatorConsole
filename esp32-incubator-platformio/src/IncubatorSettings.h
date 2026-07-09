#pragma once
#include "SettingsMessages.h"

// Engineering-unit view of the packed SettingsResponse for the control logic.
struct ControlSettings {
    float temperatureMin, temperatureMax;  // degC
    float humidityMin, humidityMax;        // %RH
    float co2Max;                          // ppm
    float pitchDegMax, rollDegMax;         // deg (compared against absolute value)
    float soundMax;                        // raw ADC units
    float weightMin, weightMax;            // g
    float voltageMin, voltageMax;          // V
    float currentMin, currentMax;          // A
    float waterLevelMin, waterLevelMax;    // %
};

// Persistence and conversion for the device's threshold settings. The packed
// SettingsResponse doubles as the NVS blob format, so exactly what came over
// the air is what survives a reboot.
namespace SettingsStore {
    // Fallback matching the backend's DEFAULT_SETTINGS; used only when NVS is
    // empty and the gateway has not answered yet.
    SettingsResponse defaults(uint8_t deviceId);

    bool loadFromNvs(SettingsResponse& out);

    // Writes only when the stored blob differs (flash wear). Returns false on
    // NVS error, true otherwise.
    bool saveToNvsIfChanged(const SettingsResponse& s);

    // Sanity gate for received/loaded settings: correct type, CRC matches,
    // and min < max for every range pair.
    bool validate(const SettingsResponse& s);

    ControlSettings toFloats(const SettingsResponse& s);
}
