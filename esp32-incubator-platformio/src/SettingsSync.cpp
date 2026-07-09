#include "SettingsSync.h"
#include "IncubatorSettings.h"

namespace {
constexpr uint32_t POLL_INTERVAL_MS  = 15000;
constexpr uint32_t LISTEN_TIMEOUT_MS = 2000;
}

void settingsSyncTask(void* param) {
    auto* ctx = static_cast<SettingsSyncContext*>(param);

    for (;;) {
        if (*ctx->loraOk) {
            SettingsRequest req;
            req.msgType  = SETTINGS_MSG_REQUEST;
            req.deviceId = ctx->deviceId;
            xSemaphoreTake(ctx->settingsMutex, portMAX_DELAY);
            req.settingsVersion = ctx->settings->settingsVersion;
            xSemaphoreGive(ctx->settingsMutex);

            uint8_t encReq[sizeof(SettingsRequest) + AesGcm::OVERHEAD];
            uint8_t rxBuf[sizeof(SettingsResponse) + AesGcm::OVERHEAD];
            SettingsResponse resp;
            bool gotResponse = false;
            int  txState = RADIOLIB_ERR_UNKNOWN;
            int  rxState = RADIOLIB_ERR_RX_TIMEOUT;
            size_t rxLen = 0;

            // One guard for radio AND crypto: the AesGcm instance is shared
            // with the measurement TX in sensorReadingTask.
            xSemaphoreTake(ctx->radioMutex, portMAX_DELAY);
            size_t encLen = 0;
            if (ctx->aes->encrypt(reinterpret_cast<const uint8_t*>(&req), sizeof(req),
                                  encReq, encLen)) {
                txState = ctx->radio->transmit(encReq, encLen);
            }
            if (txState == RADIOLIB_ERR_NONE) {
                rxState = ctx->radio->receive(rxBuf, sizeof(rxBuf), LISTEN_TIMEOUT_MS);
                if (rxState == RADIOLIB_ERR_NONE) {
                    rxLen = ctx->radio->getPacketLength();
                    if (rxLen == sizeof(rxBuf)) {
                        size_t plainLen = 0;
                        gotResponse = ctx->aes->decrypt(rxBuf, sizeof(rxBuf),
                                                        reinterpret_cast<uint8_t*>(&resp), plainLen)
                                      && plainLen == sizeof(resp);
                    }
                }
            }
            xSemaphoreGive(ctx->radioMutex);

            if (txState != RADIOLIB_ERR_NONE) {
                Serial.printf("[SettingsSync] Request TX failed (code %d)\n", txState);
            } else if (gotResponse) {
                if (resp.msgType == SETTINGS_MSG_RESPONSE
                    && resp.deviceId == ctx->deviceId
                    && SettingsStore::validate(resp)) {
                    xSemaphoreTake(ctx->settingsMutex, portMAX_DELAY);
                    *ctx->settings = resp;
                    xSemaphoreGive(ctx->settingsMutex);
                    SettingsStore::saveToNvsIfChanged(resp);
                    Serial.printf("[SettingsSync] Applied settings version 0x%08lX\n",
                                  (unsigned long)resp.settingsVersion);
                } else {
                    Serial.println("[SettingsSync] Invalid settings response — ignoring");
                }
            } else if (rxState == RADIOLIB_ERR_RX_TIMEOUT) {
                Serial.println("[SettingsSync] Polled — no reply (up to date or gateway offline)");
            } else if (rxState == RADIOLIB_ERR_NONE) {
                Serial.printf("[SettingsSync] Dropped reply (len %u or decrypt failed)\n",
                              (unsigned)rxLen);
            } else {
                Serial.printf("[SettingsSync] RX error (code %d)\n", rxState);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
