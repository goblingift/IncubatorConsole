#pragma once
#include <stdint.h>

// Binary representation of one sensor reading — 27 bytes packed.
// Scaling rules (applied before storing, reversed on receiver):
//   voltage      V   × 100  → uint16_t   (e.g. 10.35 V  → 1035)
//   current      A   × 100  → int16_t    (e.g.  0.41 A  →   41)
//   temperature  °C  × 100  → int16_t    (e.g. 20.81 °C → 2081)
//   humidity     %   × 100  → uint16_t   (e.g. 75.01 %  → 7501)
//   pitch/roll   deg × 10   → int16_t    (e.g. -0.5 deg →   -5)
struct __attribute__((packed)) SensorReading {
    uint32_t ts;            // Unix timestamp (seconds since 1970-01-01 UTC)
    uint8_t  deviceId;      // 0–255
    uint16_t voltage;       // V × 100
    int16_t  current;       // A × 100
    uint16_t lux;           // lux
    uint16_t co2;           // ppm
    int16_t  temperature;   // °C × 100
    uint16_t humidity;      // % × 100
    int16_t  pitch;         // deg × 10
    int16_t  roll;          // deg × 10
    uint8_t  waterLevel;    // %
    uint16_t weight;        // g (integer)
    uint16_t peakLoudness;  // ADC units
    uint8_t  actuatorState; // bitmask: bit0=relay1, bit1=relay2, bit2=relay3, bit3=relay4, bit4=humidifier
};

static_assert(sizeof(SensorReading) == 27, "SensorReading must be exactly 27 bytes");
