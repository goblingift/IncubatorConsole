#pragma once
#include <stdint.h>
#include <stddef.h>
#include "SensorReading.h"

// Accumulates READING_COUNT sensor readings and exposes them as a flat
// byte array ready for encryption and LoRa transmission.
// Total raw payload: 6 × 27 = 162 bytes.
class LoRaPayload {
public:
    static constexpr uint8_t READING_COUNT = 6;
    static constexpr size_t  SIZE          = READING_COUNT * sizeof(SensorReading);  // 162 bytes

    LoRaPayload();

    // Add one reading. Ignored if already full (call reset() first).
    void addReading(const SensorReading& r);

    // True when READING_COUNT readings have been collected.
    bool isReady() const;

    // Reset the buffer so a new accumulation cycle can begin.
    void reset();

    // Pointer to the raw byte array (readings laid out contiguously).
    const uint8_t* data() const;

    // Number of bytes currently filled (count × sizeof(SensorReading)).
    size_t size() const;

    // How many readings have been added so far.
    uint8_t count() const;

private:
    SensorReading _readings[READING_COUNT];
    uint8_t       _count;
};
