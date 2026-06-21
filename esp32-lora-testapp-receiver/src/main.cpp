#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "SensorReading.h"
#include "AesGcm.h"

// Wio SX1262 + XIAO ESP32S3 B2B connector pins
static constexpr int LORA_NSS  = 41;
static constexpr int LORA_DIO1 = 39;
static constexpr int LORA_RST  = 42;
static constexpr int LORA_BUSY = 40;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// --- LoRa parameters ---
// Static
static constexpr float LORA_FREQ_MHZ      = 868.0;
static constexpr float LORA_BW_KHZ        = 125.0;
static constexpr uint8_t LORA_CR          = 5;       // 4/5
static constexpr uint8_t LORA_SYNC_WORD   = 0x14;    // private network
static constexpr uint8_t LORA_PREAMBLE    = 8;
// Configurable
static constexpr uint8_t LORA_SF          = 7;
static constexpr int8_t  LORA_TX_POWER    = 13;      // not used for RX, but kept for symmetry

// --- Encryption (must match sender) ---
static const uint8_t AES_KEY[AesGcm::KEY_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static AesGcm* aesGcm = nullptr;

static constexpr uint8_t READING_COUNT = 6;
static constexpr size_t  PAYLOAD_SIZE  = READING_COUNT * sizeof(SensorReading);  // 162 bytes
static constexpr size_t  ENCRYPTED_LEN = PAYLOAD_SIZE + AesGcm::OVERHEAD;        // 190 bytes

// --- Buffers ---
static uint8_t rxBuf[256];
static uint8_t decryptedBuf[PAYLOAD_SIZE];

enum InputMode { MODE_LORA, MODE_SERIAL_HEX };
static InputMode inputMode = MODE_LORA;

void printReading(const SensorReading& r, uint8_t index);
bool verifyReading(const SensorReading& r, uint8_t index);
void processEncryptedPayload(const uint8_t* data, size_t len);
bool parseHexString(const String& hex, uint8_t* out, size_t maxLen, size_t& outLen);

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("=== LoRa Receiver / Test App ===");

    aesGcm = new AesGcm(AES_KEY);

    Serial.print("[SX1262] Initializing ... ");
    int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_TX_POWER, LORA_PREAMBLE);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
        radio.setCRC(true);
        inputMode = MODE_LORA;
    } else {
        Serial.print("FAILED (code ");
        Serial.print(state);
        Serial.println(") — falling back to Serial hex input mode");
        inputMode = MODE_SERIAL_HEX;
    }

    if (inputMode == MODE_LORA) {
        Serial.println("Listening for LoRa packets on 868 MHz ...");
        Serial.println("(You can also paste a hex string into Serial at any time)");
    } else {
        Serial.println("Paste an encrypted hex string into Serial to decrypt and verify.");
    }
}

void loop() {
    // --- Check for LoRa packet ---
    if (inputMode == MODE_LORA) {
        int state = radio.receive(rxBuf, sizeof(rxBuf), 64000);
        if (state == RADIOLIB_ERR_NONE) {
            size_t len = radio.getPacketLength();
            Serial.println();
            Serial.print("[LoRa] Received ");
            Serial.print(len);
            Serial.print(" bytes  RSSI: ");
            Serial.print(radio.getRSSI());
            Serial.print(" dBm  SNR: ");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
            processEncryptedPayload(rxBuf, len);
        } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
            Serial.print("[LoRa] Receive error: ");
            Serial.println(state);
        }
    }

    // --- Check for Serial hex input ---
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            size_t parsedLen = 0;
            if (parseHexString(line, rxBuf, sizeof(rxBuf), parsedLen)) {
                Serial.print("[Serial] Parsed ");
                Serial.print(parsedLen);
                Serial.println(" bytes from hex input");
                processEncryptedPayload(rxBuf, parsedLen);
            } else {
                Serial.println("[Serial] Invalid hex string");
            }
        }
    }

}

void processEncryptedPayload(const uint8_t* data, size_t len) {
    if (len != ENCRYPTED_LEN) {
        Serial.print("  ERROR: expected ");
        Serial.print(ENCRYPTED_LEN);
        Serial.print(" bytes, got ");
        Serial.println(len);
        return;
    }

    size_t decLen = 0;
    if (!aesGcm->decrypt(data, len, decryptedBuf, decLen)) {
        Serial.println("  ERROR: AES-GCM decryption failed (wrong key or corrupted data)");
        return;
    }

    Serial.print("  Decrypted OK: ");
    Serial.print(decLen);
    Serial.println(" bytes");

    if (decLen != READING_COUNT * sizeof(SensorReading)) {
        Serial.print("  ERROR: decrypted size mismatch (expected ");
        Serial.print(READING_COUNT * sizeof(SensorReading));
        Serial.println(" bytes)");
        return;
    }

    const SensorReading* readings = reinterpret_cast<const SensorReading*>(decryptedBuf);
    bool allOk = true;

    Serial.println();
    Serial.println("====== Decoded Sensor Readings ======");
    for (uint8_t i = 0; i < READING_COUNT; i++) {
        printReading(readings[i], i);
        if (!verifyReading(readings[i], i)) {
            allOk = false;
        }
        Serial.println();
    }

    if (allOk) {
        Serial.println("RESULT: ALL READINGS OK");
    } else {
        Serial.println("RESULT: SOME READINGS OUT OF RANGE (see warnings above)");
    }
    Serial.println("=====================================");
    Serial.println();
}

void printReading(const SensorReading& r, uint8_t index) {
    Serial.print("--- Reading #");
    Serial.print(index + 1);
    Serial.println(" ---");
    Serial.print("  Timestamp:    "); Serial.println(r.ts);
    Serial.print("  Device ID:    "); Serial.println(r.deviceId);
    Serial.print("  Voltage:      "); Serial.print(r.voltage / 100.0f, 2);   Serial.println(" V");
    Serial.print("  Current:      "); Serial.print(r.current / 100.0f, 3);   Serial.println(" A");
    Serial.print("  Light:        "); Serial.print(r.lux);                    Serial.println(" lux");
    Serial.print("  CO2:          "); Serial.print(r.co2);                    Serial.println(" ppm");
    Serial.print("  Temperature:  "); Serial.print(r.temperature / 100.0f, 2); Serial.println(" C");
    Serial.print("  Humidity:     "); Serial.print(r.humidity / 100.0f, 1);   Serial.println(" %");
    Serial.print("  Pitch:        "); Serial.print(r.pitch / 10.0f, 1);      Serial.println(" deg");
    Serial.print("  Roll:         "); Serial.print(r.roll / 10.0f, 1);       Serial.println(" deg");
    Serial.print("  Water level:  "); Serial.print(r.waterLevel);             Serial.println(" %");
    Serial.print("  Weight:       "); Serial.print(r.weight);                 Serial.println(" g");
    Serial.print("  Peak loudness:"); Serial.println(r.peakLoudness);
    Serial.print("  Relay state:  ");
    for (int i = 3; i >= 0; i--) Serial.print((r.relayState >> i) & 1);
    Serial.println();
}

bool verifyReading(const SensorReading& r, uint8_t index) {
    bool ok = true;
    auto warn = [&](const char* field, const char* msg) {
        Serial.print("  WARNING [#");
        Serial.print(index + 1);
        Serial.print("] ");
        Serial.print(field);
        Serial.print(": ");
        Serial.println(msg);
        ok = false;
    };

    if (r.ts == 0)
        warn("Timestamp", "is zero");
    if (r.voltage > 6000)
        warn("Voltage", "> 60V — unlikely for 12V system");
    if (r.current < -3000 || r.current > 3000)
        warn("Current", "magnitude > 30A — out of INA260 range");
    if (r.co2 > 5000)
        warn("CO2", "> 5000 ppm — sensor max is 5000");
    float tempC = r.temperature / 100.0f;
    if (tempC < -40.0f || tempC > 85.0f)
        warn("Temperature", "outside -40..85 C range");
    if (r.humidity > 10000)
        warn("Humidity", "> 100% — impossible");
    if (r.waterLevel > 100)
        warn("Water level", "> 100%");
    if (r.weight > 20000)
        warn("Weight", "> 20 kg — exceeds expected range");

    return ok;
}

bool parseHexString(const String& hex, uint8_t* out, size_t maxLen, size_t& outLen) {
    size_t hexLen = hex.length();
    if (hexLen % 2 != 0) return false;

    outLen = hexLen / 2;
    if (outLen > maxLen) return false;

    for (size_t i = 0; i < outLen; i++) {
        char hi = hex.charAt(i * 2);
        char lo = hex.charAt(i * 2 + 1);
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        int h = nibble(hi), l = nibble(lo);
        if (h < 0 || l < 0) return false;
        out[i] = (h << 4) | l;
    }
    return true;
}
