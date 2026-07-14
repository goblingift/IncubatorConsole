#include "ControlLogic.h"
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

namespace {

void addLine(ControlResult& r, const char* fmt, ...) {
    if (r.violationCount >= MAX_VIOLATIONS) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(r.lines[r.violationCount], VIOLATION_LINE_LEN, fmt, args);
    va_end(args);
    r.violationCount++;
}

void checkRangeF(ControlResult& r, const char* label, float v, float lo, float hi) {
    if (v < lo)      addLine(r, "%s %.1f<%.1f", label, v, lo);
    else if (v > hi) addLine(r, "%s %.1f>%.1f", label, v, hi);
}

void checkRangeI(ControlResult& r, const char* label, int v, int lo, int hi) {
    if (v < lo)      addLine(r, "%s %d<%d", label, v, lo);
    else if (v > hi) addLine(r, "%s %d>%d", label, v, hi);
}

}  // namespace

namespace ControlLogic {

ControlResult evaluate(const Measurements& m, const ControlSettings& s,
                       bool prevFan, bool prevHeater, bool prevHumidifier) {
    ControlResult r = {};

    if (m.scdValid) {
        // Fan pulls in fresh (cooler, drier) outside air.
        r.fanOn    = (m.co2 > s.co2Max)
                     || (m.temperature > s.temperatureMax)
                     || (m.humidity > s.humidityMax);
        r.heaterOn = (m.temperature < s.temperatureMin);
        r.humidifierOn = (m.humidity < s.humidityMin);

        checkRangeF(r, "T",   m.temperature, s.temperatureMin, s.temperatureMax);
        checkRangeF(r, "Hum", m.humidity,    s.humidityMin,    s.humidityMax);
        if (m.co2 > s.co2Max) addLine(r, "CO2 %u>%d", m.co2, (int)s.co2Max);
    } else {
        // Hold the previous actuator states rather than reacting to garbage.
        r.fanOn        = prevFan;
        r.heaterOn     = prevHeater;
        r.humidifierOn = prevHumidifier;
        addLine(r, "SCD41 ERR");
    }

    if (m.peakLoudness > s.soundMax) {
        addLine(r, "Snd %d>%d", m.peakLoudness, (int)s.soundMax);
    }
    checkRangeI(r, "Wgt", (int)m.weightG, (int)s.weightMin, (int)s.weightMax);
    if (fabsf(m.pitch) > s.pitchDegMax) {
        addLine(r, "Pitch %.1f>%.1f", fabsf(m.pitch), s.pitchDegMax);
    }
    if (fabsf(m.roll) > s.rollDegMax) {
        addLine(r, "Roll %.1f>%.1f", fabsf(m.roll), s.rollDegMax);
    }
    checkRangeF(r, "Volt", m.voltage, s.voltageMin, s.voltageMax);
    if (m.currentA < s.currentMin) {
        addLine(r, "Curr %.2f<%.2f", m.currentA, s.currentMin);
    } else if (m.currentA > s.currentMax) {
        addLine(r, "Curr %.2f>%.2f", m.currentA, s.currentMax);
    }
    checkRangeI(r, "Wtr", m.waterLevel, (int)s.waterLevelMin, (int)s.waterLevelMax);

    r.alertOn = (r.violationCount > 0);
    return r;
}

}  // namespace ControlLogic
