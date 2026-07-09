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

// FreeRTOS task body: polls the gateway with a FETCH_SETTINGS request every
// 15 s, listens ~2 s for a reply, and applies + persists it if one arrives.
// Gateway silence means "up to date" (or gateway offline) — the node keeps
// running on its current settings. param = SettingsSyncContext*.
void settingsSyncTask(void* param);
