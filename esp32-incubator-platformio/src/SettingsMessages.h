#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_rom_crc.h"

// LoRa settings-sync protocol between the incubator node and the gateway.
//
// The incubator polls the gateway right after each measurement batch it
// transmits (~every 60 s) with a SettingsRequest carrying the CRC32 of the
// settings it currently runs. The gateway answers with a
// SettingsResponse ONLY when its cloud-fetched copy differs — silence means
// "up to date" (or gateway offline; the node keeps running on its NVS copy).
//
// Both structs travel AES-128-GCM encrypted (see AesGcm.h, +28 bytes overhead).
// The gateway tells message types apart by ciphertext length:
//   190 bytes -> measurement batch (6x SensorReading)
//    34 bytes -> SettingsRequest
//    66 bytes -> SettingsResponse (only ever gateway -> incubator)
//
// This file must stay identical in esp32-incubator-platformio and
// esp32-gateway-platformio (same duplication scheme as SensorReading.h).

static constexpr uint8_t SETTINGS_MSG_REQUEST  = 0x01;
static constexpr uint8_t SETTINGS_MSG_RESPONSE = 0x02;

struct SettingsRequest {
    uint8_t  msgType;          // SETTINGS_MSG_REQUEST
    uint8_t  deviceId;
    uint32_t settingsVersion;  // CRC32 the node currently runs (0 = none yet)
} __attribute__((packed));

static_assert(sizeof(SettingsRequest) == 6, "SettingsRequest must stay 6 bytes");

// Threshold fields use the same fixed-point conventions as SensorReading.
struct SettingsResponse {
    uint8_t  msgType;          // SETTINGS_MSG_RESPONSE
    uint8_t  deviceId;
    uint32_t settingsVersion;  // CRC32 over the threshold fields below
    int16_t  temperatureMin;   // degC x100
    int16_t  temperatureMax;   // degC x100
    uint16_t humidityMin;      // %RH x100
    uint16_t humidityMax;      // %RH x100
    uint16_t co2Max;           // ppm
    uint16_t lightAvgMax;      // lux (24h-average threshold; not evaluated on the node)
    uint16_t pitchDegMax;      // deg x10, compared against |pitch|
    uint16_t rollDegMax;       // deg x10, compared against |roll|
    uint16_t soundMax;         // raw ADC units (matches peakLoudness)
    uint16_t weightMin;        // g
    uint16_t weightMax;        // g
    uint16_t voltageMin;       // V x100
    uint16_t voltageMax;       // V x100
    int16_t  currentMin;       // A x100
    int16_t  currentMax;       // A x100
    uint8_t  waterLevelMin;    // %
    uint8_t  waterLevelMax;    // %
} __attribute__((packed));

static_assert(sizeof(SettingsResponse) == 38, "SettingsResponse must stay 38 bytes");

static constexpr size_t SETTINGS_FIELDS_OFFSET = 6;  // header: msgType + deviceId + settingsVersion
static constexpr size_t SETTINGS_FIELDS_SIZE   = sizeof(SettingsResponse) - SETTINGS_FIELDS_OFFSET;

// CRC32 over the threshold fields only, so the version is independent of
// deviceId/header. Both sides must compute it identically.
inline uint32_t settingsCrc32(const SettingsResponse& s) {
    const uint8_t* fields = reinterpret_cast<const uint8_t*>(&s) + SETTINGS_FIELDS_OFFSET;
    return esp_rom_crc32_le(0, fields, SETTINGS_FIELDS_SIZE);
}
