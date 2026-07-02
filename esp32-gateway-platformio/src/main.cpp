#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <string.h>
#include "AesGcm.h"
#include "SensorReading.h"
#include "WifiManager.h"

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

static AesGcm* aesGcm = nullptr;
static uint8_t rxBuf[256];
static uint8_t plainBuf[PACKET_SIZE];

static WifiManager wifiManager;

void printReceivedJson(const SensorReading* readings, uint8_t count, float rssi, float snr) {
    JsonDocument doc;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["readingCount"] = count;

    JsonArray readingsArr = doc["readings"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
        const SensorReading& r = readings[i];
        JsonObject o = readingsArr.add<JsonObject>();
        o["deviceId"]     = r.deviceId;
        o["timestamp"]    = r.ts;
        o["voltage"]      = r.voltage / 100.0f;
        o["current"]      = r.current / 100.0f;
        o["lux"]          = r.lux;
        o["co2"]          = r.co2;
        o["temperature"]  = r.temperature / 100.0f;
        o["humidity"]     = r.humidity / 100.0f;
        o["pitch"]        = r.pitch / 10.0f;
        o["roll"]         = r.roll / 10.0f;
        o["waterLevel"]   = r.waterLevel;
        o["weight"]       = r.weight;
        o["peakLoudness"] = r.peakLoudness;
        o["relayState"]   = r.relayState;
    }

    Serial.println("--- Prepared JSON (ready for AWS IoT Core forwarding) ---");
    serializeJsonPretty(doc, Serial);
    Serial.println();
    Serial.println("-----------------------------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("=== ESP32 LoRa Gateway ===");

    wifiManager.begin();

    aesGcm = new AesGcm(AES_KEY);

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

        if (len != ENCRYPTED_SIZE) {
            Serial.print("[LoRa] Unexpected length (expected ");
            Serial.print(ENCRYPTED_SIZE);
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
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.print("[LoRa] Receive error: ");
        Serial.println(state);
    }
}
