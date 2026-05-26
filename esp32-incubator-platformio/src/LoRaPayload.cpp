#include "LoRaPayload.h"
#include <string.h>

LoRaPayload::LoRaPayload() : _count(0) {
    memset(_readings, 0, sizeof(_readings));
}

void LoRaPayload::addReading(const SensorReading& r) {
    if (_count < READING_COUNT) {
        _readings[_count++] = r;
    }
}

bool LoRaPayload::isReady() const {
    return _count >= READING_COUNT;
}

void LoRaPayload::reset() {
    _count = 0;
}

const uint8_t* LoRaPayload::data() const {
    return reinterpret_cast<const uint8_t*>(_readings);
}

size_t LoRaPayload::size() const {
    return _count * sizeof(SensorReading);
}

uint8_t LoRaPayload::count() const {
    return _count;
}
