#pragma once
#include <stdint.h>
#include "IncubatorSettings.h"

static constexpr uint8_t MAX_VIOLATIONS     = 12;
static constexpr uint8_t VIOLATION_LINE_LEN = 22;  // 21 chars @ 6 px fits the 128 px OLED

// One measurement cycle's inputs, engineering units (built in sensorReadingTask).
struct Measurements {
    float    voltage;       // V
    float    currentA;      // A
    float    temperature;   // degC (SCD41)
    float    humidity;      // %RH
    uint16_t co2;           // ppm
    bool     scdValid;      // false -> temperature/humidity/co2 are garbage
    float    pitch;         // deg
    float    roll;          // deg
    float    weightG;       // g, already clamped to [0, 20000]
    int      waterLevel;    // %
    int      peakLoudness;  // raw ADC
};

struct ControlResult {
    bool    fanOn;
    bool    heaterOn;
    bool    humidifierOn;
    bool    alertOn;
    uint8_t violationCount;
    char    lines[MAX_VIOLATIONS][VIOLATION_LINE_LEN];  // preformatted for the alert screen
};

namespace ControlLogic {
    // Pure threshold evaluation (strict thresholds, no hysteresis — per
    // product decision). When the SCD41 reading is invalid the previous
    // fan/heater/humidifier states are held instead of reacting to garbage.
    ControlResult evaluate(const Measurements& m, const ControlSettings& s,
                           bool prevFan, bool prevHeater, bool prevHumidifier);

    // Enforces a rest period after continuous operation: some humidifiers
    // stop working correctly if driven for many consecutive minutes. Call
    // once per sensor cycle with evaluate()'s ControlResult.humidifierOn and
    // the current millis(); returns the state to actually apply. Holds its
    // own timing state (function-local statics) across calls — there is
    // only ever one caller (sensorReadingTask), so this is safe without a
    // mutex, unlike state shared across tasks.
    bool applyHumidifierRestCycle(bool desiredOn, uint32_t nowMs);
}
