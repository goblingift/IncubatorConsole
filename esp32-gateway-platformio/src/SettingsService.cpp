#include "SettingsService.h"
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
    Entry* e = find(deviceId);
    if (!e || !e->valid) return false;
    out = e->resp;
    return true;
}

void SettingsService::loop(bool wifiConnected) {
    if (!wifiConnected) return;
    unsigned long now = millis();
    for (auto& e : entries_) {
        if (!e.used) continue;
        if (e.lastAttemptMs != 0 && now - e.lastAttemptMs < REFRESH_MS) continue;
        e.lastAttemptMs = now;
        fetch(e);
        break;  // at most one blocking fetch per call keeps LoRa blindness bounded
    }
}

bool SettingsService::fetch(Entry& e) {
    char url[160];
    int n = snprintf(url, sizeof(url), "https://%s", SETTINGS_API_HOST);
    snprintf(url + n, sizeof(url) - n, SETTINGS_API_PATH_FMT, e.deviceId);

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
    r.deviceId       = e.deviceId;
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

    bool changed = !e.valid || r.settingsVersion != e.resp.settingsVersion;
    e.resp  = r;
    e.valid = true;
    Serial.printf("[Settings] Fetched device %u, version 0x%08lX%s\n",
                  e.deviceId, (unsigned long)r.settingsVersion,
                  changed ? " (changed)" : " (unchanged)");
    return true;
}
