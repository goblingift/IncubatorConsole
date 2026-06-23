#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include "ReceptionStats.h"

// Wio SX1262 + XIAO ESP32S3 B2B connector pins
static constexpr int LORA_NSS  = 41;
static constexpr int LORA_DIO1 = 39;
static constexpr int LORA_RST  = 42;
static constexpr int LORA_BUSY = 40;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static constexpr int LED_PIN = D0;

void blinkLed(int count, int onMs, int offMs) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(onMs);
        digitalWrite(LED_PIN, LOW);
        if (i < count - 1) delay(offMs);
    }
}

// --- LoRa parameters ---
static constexpr float LORA_FREQ_MHZ      = 868.0;
static constexpr float LORA_BW_KHZ        = 125.0;
static constexpr uint8_t LORA_CR          = 5;       // 4/5
static constexpr uint8_t LORA_SYNC_WORD   = 0x14;    // private network
static constexpr uint8_t LORA_PREAMBLE    = 8;
static constexpr uint8_t LORA_SF          = 7;
static constexpr int8_t  LORA_TX_POWER    = 13;

// --- Expected encrypted payload ---
static const uint8_t EXPECTED_PAYLOAD[] = {
    0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC1, 0x4E, 0xA3, 0x45,
    0x3B, 0x26, 0x80, 0x3D, 0x9A, 0xBF, 0xC5, 0xE6, 0xCB, 0x35, 0xAF, 0x87, 0x4C, 0xAF, 0x9D, 0x0C,
    0x7F, 0x46, 0xC0, 0x2D, 0x2D, 0xD8, 0xEE, 0xBF, 0x43, 0x1A, 0x80, 0x6D, 0x6E, 0xDA, 0x74, 0x85,
    0xD6, 0x72, 0x53, 0x6B, 0xA6, 0x05, 0xAB, 0xCB, 0xF3, 0x3F, 0xA2, 0x2D, 0x3F, 0x0A, 0x9C, 0x41,
    0x60, 0x29, 0xA9, 0xF5, 0xD3, 0x9E, 0x0B, 0x66, 0x94, 0x4E, 0xAD, 0x39, 0xA0, 0x47, 0x16, 0x6F,
    0x46, 0xEE, 0xBA, 0xA5, 0xA8, 0xC7, 0x4E, 0x35, 0x8F, 0xC0, 0xA6, 0xB9, 0x0C, 0x70, 0x4B, 0x38,
    0x19, 0x82, 0x9A, 0x07, 0x01, 0xA5, 0xC2, 0xBC, 0xBC, 0xC3, 0x9C, 0x62, 0x5D, 0xD2, 0x36, 0xB4,
    0xDD, 0x51, 0xD0, 0xB8, 0xB0, 0x65, 0x03, 0x21, 0x06, 0x56, 0x28, 0xB6, 0x1E, 0x41, 0xAA, 0x03,
    0x85, 0xDF, 0x96, 0x4F, 0x45, 0x7A, 0x1B, 0x82, 0xFA, 0xC5, 0x55, 0x34, 0x3C, 0x61, 0x9F, 0x92,
    0x10, 0x63, 0xC0, 0x8F, 0xE2, 0xC3, 0x97, 0xB3, 0x2E, 0xE5, 0x93, 0x42, 0x7F, 0x50, 0xE4, 0x8A,
    0x6E, 0x17, 0x41, 0x13, 0x38, 0x0E, 0x47, 0xBC, 0x1F, 0xC4, 0x8A, 0x09, 0x3C, 0x63, 0x90, 0xB3,
    0xC6, 0xAF, 0x10, 0x3F, 0xC7, 0x37, 0x26, 0xEA, 0xCC, 0x8F, 0x02, 0x3B, 0xB2, 0x30
};
static constexpr size_t EXPECTED_PAYLOAD_LEN = sizeof(EXPECTED_PAYLOAD);

static constexpr uint32_t EXPECTED_PACKET_COUNT = 100;

static uint8_t rxBuf[256];
static ReceptionStats stats(EXPECTED_PACKET_COUNT);

void updateDisplay() {
    char buf[22];
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr);

    // Line 1: Title + last result
    if (stats.getTotalReceived() == 0) {
        u8g2.drawStr(0, 12, "LoRa RX - Waiting...");
    } else if (stats.wasLastMatched()) {
        u8g2.drawStr(0, 12, "LoRa RX - MATCH");
    } else {
        u8g2.drawStr(0, 12, "LoRa RX - MISMATCH");
    }

    // Line 2: Rx count and PDR
    snprintf(buf, sizeof(buf), "Rx:%lu/%lu PDR:%.1f%%",
             stats.getTotalReceived(), stats.getExpectedPacketCount(), stats.getPdr());
    u8g2.drawStr(0, 26, buf);

    // Line 3: Match / Mismatch counts
    snprintf(buf, sizeof(buf), "OK:%lu FAIL:%lu",
             stats.getMatchCount(), stats.getMismatchCount());
    u8g2.drawStr(0, 40, buf);

    // Line 4: Success rate
    snprintf(buf, sizeof(buf), "Match rate: %.1f%%", stats.getSuccessRate());
    u8g2.drawStr(0, 54, buf);

    if (stats.getTotalReceived() > 0) {
        // Line 5: Last RSSI
        snprintf(buf, sizeof(buf), "RSSI: %.1f dBm", stats.getLastRssi());
        u8g2.drawStr(0, 68, buf);

        // Line 6: RSSI avg/min/max
        snprintf(buf, sizeof(buf), " %.1f/%.1f/%.1f",
                 stats.getAvgRssi(), stats.getMinRssi(), stats.getMaxRssi());
        u8g2.drawStr(0, 80, buf);

        // Line 7: Last SNR
        snprintf(buf, sizeof(buf), "SNR: %.1f dB", stats.getLastSnr());
        u8g2.drawStr(0, 94, buf);

        // Line 8: SNR avg/min/max
        snprintf(buf, sizeof(buf), " %.1f/%.1f/%.1f",
                 stats.getAvgSnr(), stats.getMinSnr(), stats.getMaxSnr());
        u8g2.drawStr(0, 106, buf);
    }

    u8g2.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    delay(3000);

    Serial.println("=== LoRa Receiver / Test App ===");

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 24, "LoRa RX Test");
    u8g2.drawStr(0, 48, "Initializing...");
    u8g2.sendBuffer();

    blinkLed(5, 150, 150);

    Serial.print("[SX1262] Initializing ... ");
    int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_TX_POWER, LORA_PREAMBLE);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
        radio.setCRC(true);
    } else {
        Serial.print("FAILED (code ");
        Serial.print(state);
        Serial.println(")");
        u8g2.clearBuffer();
        u8g2.drawStr(0, 24, "LoRa INIT FAILED");
        u8g2.sendBuffer();
        while (true) { blinkLed(1, 100, 0); delay(900); }
    }

    Serial.print("Expected payload: ");
    Serial.print(EXPECTED_PAYLOAD_LEN);
    Serial.println(" bytes");
    Serial.println("Listening for LoRa packets on 868 MHz ...");

    updateDisplay();
}

void loop() {
    int state = radio.receive(rxBuf, sizeof(rxBuf), 64000);

    if (state == RADIOLIB_ERR_NONE) {
        size_t len = radio.getPacketLength();
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();
        uint32_t timeOnAirMs = radio.getTimeOnAir(len) / 1000;

        Serial.println();
        Serial.print("[LoRa] Received ");
        Serial.print(len);
        Serial.print(" bytes  ToA: ");
        Serial.print(timeOnAirMs);
        Serial.print(" ms  RSSI: ");
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

        bool matched = (len == EXPECTED_PAYLOAD_LEN) &&
                       (memcmp(rxBuf, EXPECTED_PAYLOAD, EXPECTED_PAYLOAD_LEN) == 0);

        if (matched) {
            Serial.println("[LoRa] MATCH — payload equals expected bytes");
        } else {
            Serial.println("[LoRa] MISMATCH — payload differs from expected bytes");
            if (len == EXPECTED_PAYLOAD_LEN) {
                for (size_t i = 0; i < len; i++) {
                    if (rxBuf[i] != EXPECTED_PAYLOAD[i]) {
                        Serial.print("  First diff at byte ");
                        Serial.print(i);
                        Serial.print(": got 0x");
                        if (rxBuf[i] < 0x10) Serial.print('0');
                        Serial.print(rxBuf[i], HEX);
                        Serial.print(", expected 0x");
                        if (EXPECTED_PAYLOAD[i] < 0x10) Serial.print('0');
                        Serial.println(EXPECTED_PAYLOAD[i], HEX);
                        break;
                    }
                }
            } else {
                Serial.print("  Length mismatch: got ");
                Serial.print(len);
                Serial.print(", expected ");
                Serial.println(EXPECTED_PAYLOAD_LEN);
            }
        }

        stats.recordReception(matched, rssi, snr);

        if (matched) {
            blinkLed(2, 400, 200);
        } else {
            blinkLed(5, 80, 80);
        }

        stats.printSummary();
        updateDisplay();
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.print("[LoRa] Receive error: ");
        Serial.println(state);
    }
}
