#pragma once
#include <Arduino.h>
#include "SettingsMessages.h"

// Cloud-settings cache for the LoRa settings-sync downlink.
//
// A dedicated background FreeRTOS task performs the slow, blocking HTTPS
// refresh, so the main loop's radio.receive() is never blinded by it. (This
// used to run inline in loop() before radio.receive() — a fetch and an
// incoming LoRa packet's airtime could overlap, silently dropping the
// packet. Since the incubator's settings-poll cadence and this refresh
// cadence are both ~60 s, once they drifted into phase the gateway missed
// every single poll.) get() only ever reads the mutex-protected cache; it
// never blocks on network I/O and is safe to call from any task/core.
class SettingsService {
public:
    static constexpr uint8_t       MAX_DEVICES  = 4;
    static constexpr unsigned long REFRESH_MS   = 60000;
    static constexpr unsigned long TASK_TICK_MS = 1000;

    // Starts the background refresh task. Call once from setup().
    void begin();

    // Returns true and fills out when a valid cached copy exists. Unknown
    // devices are registered so the background task starts fetching for
    // them. Thread-safe; never blocks on network I/O.
    bool get(uint8_t deviceId, SettingsResponse& out);

private:
    struct Entry {
        bool             used = false;
        bool             valid = false;
        uint8_t          deviceId = 0;
        unsigned long    lastAttemptMs = 0;  // 0 = never attempted
        SettingsResponse resp = {};
    };

    Entry             entries_[MAX_DEVICES];
    SemaphoreHandle_t mutex_ = nullptr;

    Entry* find(uint8_t deviceId);  // call only while holding mutex_
    bool   fetchOverHttp(uint8_t deviceId, SettingsResponse& out);  // network only, no lock held

    static void taskTrampoline(void* param);
    void taskBody();
};
