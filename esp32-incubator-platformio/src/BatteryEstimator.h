#pragma once
#include <stdint.h>
#include <stddef.h>

// Voltage -> state-of-charge percentage, ported from the AWS backend's
// calibrated discharge curves (IncubatorAWS/incubator-battery-status/
// curve.py + config.py) so the OLED can show a battery percentage without
// any round trip to the cloud — the two inputs this needs (bus voltage,
// heater relay state) are already read locally every sensor cycle.
//
// Two curves exist because the heater relay's load measurably sags the bus
// voltage; using the wrong curve for the current load would misreport the
// percentage. Keep these tables identical to the Python originals if the
// battery/curve is ever recalibrated — there is intentionally no shared
// source between the two AWS copies and this one (same trade-off already
// accepted on the AWS side).
namespace BatteryEstimator {

struct CurvePoint {
    float   voltage;
    uint8_t percent;
};

// Sorted descending by voltage/percent, spanning 100% down to 0%.
static constexpr CurvePoint IDLE_CURVE[] = {
    {12.39f, 100}, {12.19f, 95}, {12.10f, 90}, {11.93f, 85}, {11.74f, 80},
    {11.59f, 75},  {11.47f, 70}, {11.33f, 65}, {11.18f, 60}, {11.03f, 55},
    {10.90f, 50},  {10.78f, 45}, {10.69f, 40}, {10.60f, 35}, {10.51f, 30},
    {10.40f, 25},  {10.27f, 20}, {10.12f, 15}, {9.96f,  10}, {9.70f,   5},
    {8.68f,   0},
};

static constexpr CurvePoint HEATER_CURVE[] = {
    {11.88f, 100}, {11.50f, 95}, {11.42f, 90}, {11.27f, 85}, {11.10f, 80},
    {10.97f, 75},  {10.86f, 70}, {10.75f, 65}, {10.62f, 60}, {10.49f, 55},
    {10.36f, 50},  {10.26f, 45}, {10.18f, 40}, {10.10f, 35}, {10.01f, 30},
    {9.91f,  25},  {9.79f,  20}, {9.65f,  15}, {9.49f,  10}, {9.23f,   5},
    {8.26f,   0},
};

inline uint8_t interpolatePercent(const CurvePoint* curve, size_t len, float voltage) {
    if (voltage >= curve[0].voltage) return curve[0].percent;
    if (voltage <= curve[len - 1].voltage) return curve[len - 1].percent;

    for (size_t i = 0; i + 1 < len; i++) {
        float vHigh = curve[i].voltage, vLow = curve[i + 1].voltage;
        if (voltage <= vHigh && voltage >= vLow) {
            if (vHigh == vLow) return curve[i].percent;
            float fraction = (voltage - vLow) / (vHigh - vLow);
            return (uint8_t)(curve[i + 1].percent + fraction * (curve[i].percent - curve[i + 1].percent));
        }
    }
    return curve[len - 1].percent;  // unreachable if the curve is well-formed
}

inline uint8_t estimatePercent(float voltage, bool heaterOn) {
    if (heaterOn) {
        return interpolatePercent(HEATER_CURVE, sizeof(HEATER_CURVE) / sizeof(HEATER_CURVE[0]), voltage);
    }
    return interpolatePercent(IDLE_CURVE, sizeof(IDLE_CURVE) / sizeof(IDLE_CURVE[0]), voltage);
}

}  // namespace BatteryEstimator
