#include "SettingsService.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include "AwsIotConfig.h"  // AWS_IOT_ROOT_CA (Amazon Root CA 1 also signs API Gateway)
#include "SettingsApiConfig.h"

SettingsService::Entry* SettingsService::find(uint8_t deviceId) {
    for (auto& e : entries_) {
        if (e.used && e.deviceId == deviceId) return &e;
    }
    for (auto& e : entries_) {
        if (!e.used) {
            e.used = true;
            e.deviceId = deviceId;
            return &e;
        }
    }
    return nullptr;
}

bool SettingsService::get(uint8_t deviceId, SettingsResponse& out) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    Entry* e = find(deviceId);
    bool ok = e && e->valid;
    if (ok) out = e->resp;
    xSemaphoreGive(mutex_);
    return ok;
}

void SettingsService::begin() {
    mutex_ = xSemaphoreCreateMutex();
    // Pinned to core 0, alongside the WiFi/TLS stack, so the blocking HTTPS
    // fetch can never delay core 1's radio.receive() loop.
    xTaskCreatePinnedToCore(taskTrampoline, "SettingsFetch", 8192, this, 1, nullptr, 0);
}

void SettingsService::taskTrampoline(void* param) {
    static_cast<SettingsService*>(param)->taskBody();
}

void SettingsService::taskBody() {
    for (;;) {
        uint8_t deviceId = 0;
        bool    due = false;

        xSemaphoreTake(mutex_, portMAX_DELAY);
        unsigned long now = millis();
        for (auto& e : entries_) {
            if (!e.used) continue;
            if (e.lastAttemptMs != 0 && now - e.lastAttemptMs < REFRESH_MS) continue;
            e.lastAttemptMs = now;  // mark attempted now so a slow/failed fetch isn't retried immediately
            deviceId = e.deviceId;
            due = true;
            break;  // at most one fetch per tick
        }
        xSemaphoreGive(mutex_);

        if (due && WiFi.status() == WL_CONNECTED) {
            SettingsResponse fetched;
            if (fetchOverHttp(deviceId, fetched)) {
                xSemaphoreTake(mutex_, portMAX_DELAY);
                Entry* e = find(deviceId);
                bool changed = !e->valid || fetched.settingsVersion != e->resp.settingsVersion;
                e->resp  = fetched;
                e->valid = true;
                xSemaphoreGive(mutex_);
                Serial.printf("[Settings] Fetched device %u, version 0x%08lX%s\n",
                              deviceId, (unsigned long)fetched.settingsVersion,
                              changed ? " (changed)" : " (unchanged)");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
    }
}

bool SettingsService::fetchOverHttp(uint8_t deviceId, SettingsResponse& out) {
    char url[160];
    int n = snprintf(url, sizeof(url), "https://%s", SETTINGS_API_HOST);
    snprintf(url + n, sizeof(url) - n, SETTINGS_API_PATH_FMT, deviceId);

    WiFiClientSecure client;
    client.setCACert(AWS_IOT_ROOT_CA);
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        Serial.println("[Settings] http.begin failed");
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Settings] GET %s -> %d\n", url, code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Settings] JSON parse failed: %s\n", err.c_str());
        return false;
    }

    // Fixed-point scaling mirrors SensorReading / SettingsMessages.h. Fallback
    // values match the backend's DEFAULT_SETTINGS for missing fields.
    SettingsResponse r = {};
    r.msgType        = SETTINGS_MSG_RESPONSE;
    r.deviceId       = deviceId;
    r.temperatureMin = (int16_t) lroundf((doc["temperature_min"] | 36.0f)   * 100.0f);
    r.temperatureMax = (int16_t) lroundf((doc["temperature_max"] | 39.0f)   * 100.0f);
    r.humidityMin    = (uint16_t)lroundf((doc["humidity_min"]    | 45.0f)   * 100.0f);
    r.humidityMax    = (uint16_t)lroundf((doc["humidity_max"]    | 70.0f)   * 100.0f);
    r.co2Max         = (uint16_t)lroundf( doc["co2_max"]         | 7100.0f);
    r.lightAvgMax    = (uint16_t)lroundf( doc["light_avg_max"]   | 500.0f);
    r.pitchDegMax    = (uint16_t)lroundf((doc["pitch_deg_max"]   | 15.0f)   * 10.0f);
    r.rollDegMax     = (uint16_t)lroundf((doc["roll_deg_max"]    | 15.0f)   * 10.0f);
    r.soundMax       = (uint16_t)lroundf( doc["sound_max"]       | 80.0f);
    r.weightMin      = (uint16_t)lroundf( doc["weight_min"]      | 0.0f);
    r.weightMax      = (uint16_t)lroundf( doc["weight_max"]      | 5000.0f);
    r.voltageMin     = (uint16_t)lroundf((doc["voltage_min"]     | 11.0f)   * 100.0f);
    r.voltageMax     = (uint16_t)lroundf((doc["voltage_max"]     | 13.0f)   * 100.0f);
    r.currentMin     = (int16_t) lroundf((doc["current_min"]     | 0.0f)    * 100.0f);
    r.currentMax     = (int16_t) lroundf((doc["current_max"]     | 2.0f)    * 100.0f);
    r.waterLevelMin  = (uint8_t) lroundf( doc["water_level_min"] | 10.0f);
    r.waterLevelMax  = (uint8_t) lroundf( doc["water_level_max"] | 100.0f);
    r.settingsVersion = settingsCrc32(r);

    out = r;
    return true;
}
