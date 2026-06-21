#pragma once
#include <stdint.h>

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
    uint8_t  relayState;    // bitmask: bit0=relay1 … bit3=relay4
};

static_assert(sizeof(SensorReading) == 27, "SensorReading must be exactly 27 bytes");
