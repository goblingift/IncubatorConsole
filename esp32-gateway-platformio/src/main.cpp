#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <string.h>
#include <algorithm>
#include "AesGcm.h"
#include "SensorReading.h"
#include "SettingsMessages.h"
#include "SettingsService.h"
#include "WifiManager.h"
#include "AwsIotClient.h"
#include "AwsIotConfig.h"

// --- SX1262 (Wio SX1262 + XIAO ESP32S3 B2B connector) ---
// Same pinout as the incubator sender.
static constexpr int LORA_NSS  = 41;
static constexpr int LORA_DIO1 = 39;
static constexpr int LORA_RST  = 42;
static constexpr int LORA_BUSY = 40;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// --- LoRa parameters — must match the sender exactly ---
static constexpr float   LORA_FREQ_MHZ  = 868.0;
static constexpr float   LORA_BW_KHZ    = 125.0;
static constexpr uint8_t LORA_CR        = 5;       // 4/5
static constexpr uint8_t LORA_SYNC_WORD = 0x14;    // private network
static constexpr uint8_t LORA_PREAMBLE  = 8;
static constexpr uint8_t LORA_SF        = 7;
static constexpr int8_t  LORA_TX_POWER  = 13;       // unused for RX, required by begin()

// AES-128-GCM pre-shared key — PLACEHOLDER, must be identical to the sender's key.
static const uint8_t AES_KEY[AesGcm::KEY_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static constexpr uint8_t READING_COUNT = 6;
static constexpr size_t  PACKET_SIZE   = READING_COUNT * sizeof(SensorReading);  // 162 bytes
static constexpr size_t  ENCRYPTED_SIZE = PACKET_SIZE + AesGcm::OVERHEAD;        // 190 bytes
static constexpr size_t  SETTINGS_REQUEST_ENC_SIZE = sizeof(SettingsRequest) + AesGcm::OVERHEAD;  // 34 bytes

static AesGcm* aesGcm = nullptr;
static uint8_t rxBuf[256];
static uint8_t plainBuf[PACKET_SIZE];

static WifiManager     wifiManager;
static AwsIotClient    awsIotClient;
static SettingsService settingsService;

// Formats the on-wire numeric device ID into the string form used throughout
// the AWS backend (DynamoDB partition key, API paths, etc.), e.g. 1 -> "incubator-01".
// Kept in the gateway rather than the incubator sender since the LoRa payload
// stays a compact binary struct — only the JSON/MQTT boundary needs the string form.
String formatDeviceId(uint8_t deviceId) {
    char buf[16];
    snprintf(buf, sizeof(buf), "incubator-%02u", deviceId);
    return String(buf);
}

void printReceivedJson(const SensorReading* readings, uint8_t count, float rssi, float snr) {
    JsonDocument doc;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["readingCount"] = count;

    JsonArray readingsArr = doc["readings"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
        const SensorReading& r = readings[i];
        JsonObject o = readingsArr.add<JsonObject>();
        o["device_id"]           = formatDeviceId(r.deviceId);
        o["timestamp"]           = r.ts;
        o["voltage"]             = r.voltage / 100.0f;
        o["current"]             = r.current / 100.0f;
        o["light_intensity"]     = r.lux;
        o["co2_ppm"]             = r.co2;
        o["temperature_celsius"] = r.temperature / 100.0f;
        o["humidity_rh"]         = r.humidity / 100.0f;
        o["pitch_deg"]           = r.pitch / 10.0f;
        o["roll_deg"]            = r.roll / 10.0f;
        o["water_level"]         = r.waterLevel;
        o["weight_gram"]         = r.weight;
        o["sound_intensity"]     = r.peakLoudness;
        o["actuator_state"]      = r.actuatorState;
    }

    Serial.println("--- Prepared JSON (ready for AWS IoT Core forwarding) ---");
    serializeJsonPretty(doc, Serial);
    Serial.println();
    Serial.println("-----------------------------------------------------------");
}

// Publishes each reading as its own MQTT message, oldest timestamp first.
// AWS IoT Core has no ordering guarantee across separate publishes, so the
// caller-side ascending-timestamp order is what makes downstream consumers
// see a chronological stream.
void publishReadingsToAws(SensorReading* readings, uint8_t count) {
    if (!awsIotClient.isConnected()) {
        Serial.println("[AWS IoT] Not connected — skipping publish");
        return;
    }

    std::sort(readings, readings + count, [](const SensorReading& a, const SensorReading& b) {
        return a.ts < b.ts;
    });

    for (uint8_t i = 0; i < count; i++) {
        const SensorReading& r = readings[i];

        JsonDocument doc;
        doc["device_id"]           = formatDeviceId(r.deviceId);
        doc["timestamp"]           = r.ts;
        doc["voltage"]             = r.voltage / 100.0f;
        doc["current"]             = r.current / 100.0f;
        doc["light_intensity"]     = r.lux;
        doc["co2_ppm"]             = r.co2;
        doc["temperature_celsius"] = r.temperature / 100.0f;
        doc["humidity_rh"]         = r.humidity / 100.0f;
        doc["pitch_deg"]           = r.pitch / 10.0f;
        doc["roll_deg"]            = r.roll / 10.0f;
        doc["water_level"]         = r.waterLevel;
        doc["weight_gram"]         = r.weight;
        doc["sound_intensity"]     = r.peakLoudness;
        doc["actuator_state"]      = r.actuatorState;

        char payload[384];
        serializeJson(doc, payload, sizeof(payload));

        if (awsIotClient.publish(AWS_IOT_TOPIC, payload)) {
            Serial.print("[AWS IoT] Published reading, device ");
            Serial.print(formatDeviceId(r.deviceId));
            Serial.print(", ts ");
            Serial.println(r.ts);
        } else {
            Serial.print("[AWS IoT] Publish FAILED, device ");
            Serial.println(formatDeviceId(r.deviceId));
        }

        awsIotClient.loop();
    }
}

// Answers a decrypted-and-valid FETCH_SETTINGS poll. Stays silent (= "you are
// up to date") when the cache is empty or the versions already match; the
// incubator only expects a reply when something changed.
void handleSettingsRequest(const uint8_t* enc, size_t encLen) {
    SettingsRequest req;
    size_t plainLen = 0;
    if (!aesGcm->decrypt(enc, encLen, reinterpret_cast<uint8_t*>(&req), plainLen)
        || plainLen != sizeof(req) || req.msgType != SETTINGS_MSG_REQUEST) {
        Serial.println("[Settings] Invalid settings request — dropping");
        return;
    }

    Serial.print("[Settings] Request from ");
    Serial.print(formatDeviceId(req.deviceId));
    Serial.printf(", version 0x%08lX\n", (unsigned long)req.settingsVersion);

    SettingsResponse resp;
    if (!settingsService.get(req.deviceId, resp)) {
        Serial.println("[Settings] No cached settings yet — staying silent");
        return;
    }
    if (resp.settingsVersion == req.settingsVersion) {
        Serial.println("[Settings] Device is up to date — staying silent");
        return;
    }

    uint8_t encBuf[sizeof(SettingsResponse) + AesGcm::OVERHEAD];
    size_t  encOut = 0;
    if (!aesGcm->encrypt(reinterpret_cast<const uint8_t*>(&resp), sizeof(resp), encBuf, encOut)) {
        Serial.println("[Settings] Response encryption failed");
        return;
    }

    int txState = radio.transmit(encBuf, encOut);
    if (txState == RADIOLIB_ERR_NONE) {
        Serial.printf("[Settings] Sent settings version 0x%08lX (%u bytes)\n",
                      (unsigned long)resp.settingsVersion, (unsigned)encOut);
    } else {
        Serial.printf("[Settings] Response TX failed (code %d)\n", txState);
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("=== ESP32 LoRa Gateway ===");

    wifiManager.begin();
    awsIotClient.begin();

    // Nonce domain 0x01: the gateway encrypts settings responses under the
    // same pre-shared key as the incubator (domain 0x00) — the domain byte
    // keeps the two nonce streams disjoint.
    aesGcm = new AesGcm(AES_KEY, 0x01);

    Serial.print("[SX1262] Initializing ... ");
    int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_TX_POWER, LORA_PREAMBLE);
    if (state == RADIOLIB_ERR_NONE) {
        radio.setCRC(true);
        Serial.println("OK");
    } else {
        Serial.print("FAILED (code ");
        Serial.print(state);
        Serial.println(")");
        while (true) { delay(1000); }
    }

    Serial.println("Listening for LoRa packets...");
}

void loop() {
    wifiManager.loop();
    awsIotClient.loop();
    settingsService.loop(wifiManager.isConnected());

    int state = radio.receive(rxBuf, sizeof(rxBuf));

    if (state == RADIOLIB_ERR_NONE) {
        size_t len = radio.getPacketLength();
        float  rssi = radio.getRSSI();
        float  snr  = radio.getSNR();

        Serial.println();
        Serial.print("[LoRa] Received ");
        Serial.print(len);
        Serial.print(" bytes  RSSI: ");
        Serial.print(rssi);
        Serial.print(" dBm  SNR: ");
        Serial.print(snr);
        Serial.println(" dB");

        Serial.print("[LoRa] Raw bytes: ");
        for (size_t i = 0; i < len; i++) {
            if (rxBuf[i] < 0x10) Serial.print('0');
            Serial.print(rxBuf[i], HEX);
        }
        Serial.println();

        if (len == SETTINGS_REQUEST_ENC_SIZE) {
            handleSettingsRequest(rxBuf, len);
            return;
        }

        if (len != ENCRYPTED_SIZE) {
            Serial.print("[LoRa] Unexpected length (expected ");
            Serial.print(ENCRYPTED_SIZE);
            Serial.print(" or ");
            Serial.print(SETTINGS_REQUEST_ENC_SIZE);
            Serial.println(") — dropping packet");
            return;
        }

        size_t outLen = 0;
        if (!aesGcm->decrypt(rxBuf, len, plainBuf, outLen) || outLen != PACKET_SIZE) {
            Serial.println("[LoRa] Decryption failed (bad key/tag) — dropping packet");
            return;
        }

        SensorReading readings[READING_COUNT];
        memcpy(readings, plainBuf, PACKET_SIZE);

        printReceivedJson(readings, READING_COUNT, rssi, snr);
        publishReadingsToAws(readings, READING_COUNT);
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.print("[LoRa] Receive error: ");
        Serial.println(state);
    }
}
