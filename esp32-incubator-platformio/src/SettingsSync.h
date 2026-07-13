#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "AesGcm.h"
#include "SettingsMessages.h"

// Dependencies of the settings-sync FreeRTOS task. All pointers must stay
// valid for the lifetime of the task (they point at globals in main.cpp).
struct SettingsSyncContext {
    SX1262*           radio;
    AesGcm*           aes;       // shared with the measurement TX path; only touched under radioMutex
    SemaphoreHandle_t radioMutex;
    SemaphoreHandle_t settingsMutex;
    SettingsResponse* settings;  // guarded by settingsMutex
    uint8_t           deviceId;
    bool*             loraOk;
};

// FreeRTOS task body: blocks on a task notification and polls the gateway
// with a FETCH_SETTINGS request each time one arrives, listening ~2 s for a
// reply and applying + persisting it if one comes back. sensorReadingTask
// notifies this task right after transmitting a measurement batch (see
// main.cpp), so polls piggyback on that ~60 s cadence instead of running on
// an independent timer. Gateway silence means "up to date" (or gateway
// offline) — the node keeps running on its current settings.
// param = SettingsSyncContext*.
void settingsSyncTask(void* param);
