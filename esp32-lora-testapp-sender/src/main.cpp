#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ===================== CONFIGURATION =====================
static constexpr uint8_t LORA_SF          = 7;        // Spreading factor (6-12)
static constexpr int8_t  LORA_TX_POWER    = 13;       // Transmit power in dBm (2-22)
static constexpr uint32_t NUM_MESSAGES    = 5;       // Number of messages to send
static constexpr uint32_t INTERVAL_MS     = 2000;     // Time between messages in ms
// =========================================================

// Fixed LoRa parameters (must match receiver)
static constexpr float   LORA_FREQ_MHZ    = 868.0;
static constexpr float   LORA_BW_KHZ     = 125.0;
static constexpr uint8_t LORA_CR          = 5;        // 4/5
static constexpr uint8_t LORA_SYNC_WORD   = 0x14;     // private network
static constexpr uint8_t LORA_PREAMBLE    = 8;

// Wio SX1262 + XIAO ESP32S3 B2B connector pins
static constexpr int LORA_NSS  = 41;
static constexpr int LORA_DIO1 = 39;
static constexpr int LORA_RST  = 42;
static constexpr int LORA_BUSY = 40;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

static constexpr int LED_PIN = D0;

static const uint8_t PAYLOAD[] = {
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
static constexpr size_t PAYLOAD_LEN = sizeof(PAYLOAD);

void blinkLed(int count, int onMs, int offMs) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(onMs);
        digitalWrite(LED_PIN, LOW);
        if (i < count - 1) delay(offMs);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    delay(3000);

    Serial.println("=== LoRa Sender / Test App ===");
    blinkLed(3, 150, 150);

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
        while (true) { blinkLed(1, 100, 0); delay(900); }
    }

    uint32_t toaMs = radio.getTimeOnAir(PAYLOAD_LEN) / 1000;

    Serial.println();
    Serial.println("--- Configuration ---");
    Serial.print("  SF:            "); Serial.println(LORA_SF);
    Serial.print("  TX power:      "); Serial.print(LORA_TX_POWER); Serial.println(" dBm");
    Serial.print("  BW:            "); Serial.print(LORA_BW_KHZ, 0); Serial.println(" kHz");
    Serial.print("  CR:            4/"); Serial.println(LORA_CR);
    Serial.print("  Payload:       "); Serial.print(PAYLOAD_LEN); Serial.println(" bytes");
    Serial.print("  ToA:           "); Serial.print(toaMs); Serial.println(" ms");
    Serial.print("  Messages:      "); Serial.println(NUM_MESSAGES);
    Serial.print("  Interval:      "); Serial.print(INTERVAL_MS); Serial.println(" ms");
    Serial.println("---------------------");
    Serial.println();

    for (uint32_t i = 1; i <= NUM_MESSAGES; i++) {
        Serial.print("[TX] Sending message ");
        Serial.print(i);
        Serial.print("/");
        Serial.print(NUM_MESSAGES);
        Serial.print(" ... ");

        int txState = radio.transmit(PAYLOAD, PAYLOAD_LEN);

        if (txState == RADIOLIB_ERR_NONE) {
            Serial.println("OK");
        } else {
            Serial.print("FAILED (code ");
            Serial.print(txState);
            Serial.println(")");
        }

        if (i < NUM_MESSAGES) {
            blinkLed(1, 100, 0);
            delay(INTERVAL_MS);
        }
    }

    Serial.println();
    Serial.println("=== All messages sent ===");
    digitalWrite(LED_PIN, HIGH);
}

void loop() {
}
