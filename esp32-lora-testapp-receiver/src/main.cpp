#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include "ReceptionStats.h"
#include "ReceptionLog.h"

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
static ReceptionLog receptionLog;

enum AppMode { MODE_INIT, MODE_COMMAND, MODE_RECEIVE };
static AppMode appMode = MODE_INIT;

void handleSerialCommand();

void updateDisplay() {
    char buf[22];
    u8g2.clearBuffer();

    // Test name — big font
    char testName[16];
    snprintf(testName, sizeof(testName), "test_%d", receptionLog.getTestNumber());
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 18, testName);

    // Stats — small font
    u8g2.setFont(u8g2_font_6x12_tr);

    if (stats.getTotalReceived() == 0) {
        u8g2.drawStr(0, 36, "Listening...");
    } else {
        // Last result
        u8g2.drawStr(0, 36, stats.wasLastMatched() ? "Last: MATCH" : "Last: MISMATCH");

        snprintf(buf, sizeof(buf), "Rx:%lu/%lu PDR:%.1f%%",
                 stats.getTotalReceived(), stats.getExpectedPacketCount(), stats.getPdr());
        u8g2.drawStr(0, 50, buf);

        snprintf(buf, sizeof(buf), "OK:%lu FAIL:%lu %.1f%%",
                 stats.getMatchCount(), stats.getMismatchCount(), stats.getSuccessRate());
        u8g2.drawStr(0, 64, buf);

        snprintf(buf, sizeof(buf), "RSSI: %.1f dBm", stats.getLastRssi());
        u8g2.drawStr(0, 78, buf);

        snprintf(buf, sizeof(buf), " %.1f/%.1f/%.1f",
                 stats.getAvgRssi(), stats.getMinRssi(), stats.getMaxRssi());
        u8g2.drawStr(0, 90, buf);

        snprintf(buf, sizeof(buf), "SNR: %.1f dB", stats.getLastSnr());
        u8g2.drawStr(0, 104, buf);

        snprintf(buf, sizeof(buf), " %.1f/%.1f/%.1f",
                 stats.getAvgSnr(), stats.getMinSnr(), stats.getMaxSnr());
        u8g2.drawStr(0, 116, buf);
    }

    u8g2.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    delay(3000);

    Serial.println("=== LoRa Receiver / Test App ===");

    receptionLog.begin();

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
    }

    u8g2.begin();

    if (state != RADIOLIB_ERR_NONE) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(0, 24, "LoRa INIT FAILED");
        u8g2.sendBuffer();
        while (true) { blinkLed(1, 100, 0); delay(900); }
    }

    Serial.print("Expected payload: ");
    Serial.print(EXPECTED_PAYLOAD_LEN);
    Serial.println(" bytes");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 12, "Initialized.");
    u8g2.drawStr(0, 30, "Waiting for serial");
    u8g2.drawStr(0, 44, "commands (15s)...");
    u8g2.drawStr(0, 68, "dump  - print logs");
    u8g2.drawStr(0, 82, "list  - show files");
    u8g2.drawStr(0, 96, "clean - delete logs");
    u8g2.drawStr(0, 110, "help  - show cmds");
    u8g2.sendBuffer();

    Serial.println();
    Serial.println("Serial command window (15s) — type help for commands");

    uint32_t cmdWindowEnd = millis() + 15000;
    while (millis() < cmdWindowEnd) {
        if (Serial.available()) {
            appMode = MODE_COMMAND;
            Serial.println("Entered command mode (LoRa receive disabled)");
            u8g2.clearBuffer();
            u8g2.drawStr(0, 24, "Command mode");
            u8g2.drawStr(0, 48, "LoRa RX disabled");
            u8g2.drawStr(0, 76, "Type: help");
            u8g2.sendBuffer();
            handleSerialCommand();
            break;
        }
        delay(50);
    }

    if (appMode != MODE_COMMAND) {
        appMode = MODE_RECEIVE;
        Serial.println("No commands — entering LoRa receive mode");
        updateDisplay();
    }
}

void handleSerialCommand() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "dump") {
        receptionLog.dumpAllLogs();
    } else if (cmd == "list") {
        receptionLog.listLogFiles();
    } else if (cmd == "clean") {
        receptionLog.deleteAllLogs();
    } else if (cmd == "help") {
        Serial.println("Commands: dump, list, clean, help");
    }
}

void loop() {
    if (appMode == MODE_COMMAND) {
        if (Serial.available()) {
            handleSerialCommand();
        }
        delay(50);
        return;
    }

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

        receptionLog.record({matched, rssi, snr, len});

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
