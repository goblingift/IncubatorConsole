#pragma once
#include <Arduino.h>
#include "SettingsMessages.h"

// Cloud-settings cache for the LoRa settings-sync downlink.
//
// Devices register themselves with their first SettingsRequest; each entry is
// then refreshed from the public REST endpoint every REFRESH_MS out of loop().
// get() only ever serves from cache, so the LoRa reply path never waits on
// HTTPS — a request arriving before the first fetch completes simply goes
// unanswered and the incubator retries 15 s later.
class SettingsService {
public:
    static constexpr uint8_t       MAX_DEVICES = 4;
    static constexpr unsigned long REFRESH_MS  = 60000;

    // Refreshes at most one due entry per call (blocking HTTPS, ~1-2 s).
    // Call from loop().
    void loop(bool wifiConnected);

    // Returns true and fills out when a valid cached copy exists. Unknown
    // devices are registered so loop() starts fetching for them.
    bool get(uint8_t deviceId, SettingsResponse& out);

private:
    struct Entry {
        bool             used = false;
        bool             valid = false;
        uint8_t          deviceId = 0;
        unsigned long    lastAttemptMs = 0;  // 0 = never attempted
        SettingsResponse resp = {};
    };

    Entry entries_[MAX_DEVICES];

    Entry* find(uint8_t deviceId);
    bool   fetch(Entry& e);
};
