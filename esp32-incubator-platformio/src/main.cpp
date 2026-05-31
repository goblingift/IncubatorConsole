#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA260.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>
#include <multi_channel_relay.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "HX711.h"
#include <ADXL345.h>
#include <math.h>
#include "SensorReading.h"
#include "LoRaPayload.h"
#include "AesGcm.h"

U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_INA260 ina260;
HX711 scale;
Multi_Channel_Relay relay;
ADXL345 adxl;
SensirionI2cScd4x sensor;

static int16_t error;
static char errorMessage[64];
byte WTR_LVL_lowData[8];
byte WTR_LVL_highData[12];

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

const uint8_t DEVICE_ID = 1;

// AES-128-GCM pre-shared key — PLACEHOLDER, replace before deployment.
// Must be identical on the receiver ESP32.
static const uint8_t AES_KEY[AesGcm::KEY_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static LoRaPayload* loraPayload = nullptr;
static AesGcm*      aesGcm     = nullptr;
// Kept as a global so it never lives on the task stack (190 bytes + mbedTLS internals
// would otherwise push sensorReadingTask past its stack limit).
static uint8_t encryptedBuf[LoRaPayload::SIZE + AesGcm::OVERHEAD];

const byte BUZZER_PIN = D7;
const byte HUMID_PIN = A9;
const int buttonPin = D0;
const int DT_PIN = 3;
const int SCK_PIN = 2;
const float HX711_CALIBRATION_FACTOR = 112.0392;
const int soundSensorPinAdc = A8;

TaskHandle_t soundTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;

volatile int g_highestLoudness = 0;
volatile uint8_t g_relayState = 0;  // bitmask: bit0=relay1, bit1=relay2, bit2=relay3, bit3=relay4

#define WTR_LVL_THRESHOLD 100
#define WTR_LVL_HIGH_ADDR 0x78
#define WTR_LVL_LOW_ADDR 0x77

void initializeSCD41();
uint32_t readTimestamp();
void relayOn(int ch);
void relayOff(int ch);
void testRelay();
bool isButtonPressed();
void playSoundNotification();
void WTR_LVL_readBytes(byte addr, byte *buf, byte len);
int WTR_LVL_readPercent();
int WTR_LVL_readPercentStable(uint8_t samples = 15, uint16_t delayMs = 120);
void soundRecorderTask(void *parameter);
void sensorReadingTask(void *parameter);

void setup() {
  Serial.begin(9600);
  delay(1000);
  Wire.begin();
  Wire.setTimeOut(1000);

  loraPayload = new LoRaPayload();
  aesGcm      = new AesGcm(AES_KEY);

  if (!ina260.begin()) {
    Serial.println("Could not find INA260 chip. Check wiring!");
  }

  TSL2561.init();

  initializeSCD41();
  Serial.println("CO2, Temp, Humidity Sensor starts work!");

  pinMode(BUZZER_PIN, OUTPUT);
  playSoundNotification();

  delay(2500);
  Serial.print("Water level: ");
  Serial.print(WTR_LVL_readPercentStable(20, 150));
  Serial.println("%");

  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Input button initialized");

  adxl.powerOn();
  Serial.println("ADXL345 Accelerometer initialized");

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(HX711_CALIBRATION_FACTOR);
  scale.tare(50);
  Serial.println("Initialized HX711 scale");

  relay.begin(0x11);
  testRelay();

  Serial.println("Test Humidifier for 2secs");
  pinMode(HUMID_PIN, OUTPUT);
  digitalWrite(HUMID_PIN, HIGH);
  delay(2000);
  digitalWrite(HUMID_PIN, LOW);

  u8g2.begin();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 24, "Incubator started!");
  } while (u8g2.nextPage());
  delay(2000);

  xTaskCreatePinnedToCore(
    soundRecorderTask,
    "SoundRecorder",
    4096,
    NULL,
    1,
    &soundTaskHandle,
    1
  );

  xTaskCreatePinnedToCore(
    sensorReadingTask,
    "SensorReading",
    20480,
    NULL,
    1,
    &sensorTaskHandle,
    1
  );

  Serial.println("Setup complete!");
}

void loop() {
  if (isButtonPressed()) {
    Serial.println("Button pressed!");
    playSoundNotification();
  }
  vTaskDelay(pdMS_TO_TICKS(50));
}

void sensorReadingTask(void *parameter) {
  const unsigned long intervalMs = 10000;
  for (;;) {
    unsigned long cycleStart = millis();
    // --- collect all values first ---
    float current_mA  = ina260.readCurrent();
    float vBus        = ina260.readBusVoltage() / 1000.0f;
    long  lux         = TSL2561.readVisibleLux();

    uint16_t co2 = 0;
    float    co2Temp = 0.0;
    float    rh = 0.0;
    char     scdErrMsg[64] = {0};
    int16_t  scdError = sensor.measureAndReadSingleShot(co2, co2Temp, rh);
    if (scdError != NO_ERROR) {
      errorToString(scdError, scdErrMsg, sizeof(scdErrMsg));
    }

    double xyz[3];
    adxl.getAcceleration(xyz);
    float pitch = atan2((float)xyz[0], sqrt((float)(xyz[1]*xyz[1] + xyz[2]*xyz[2]))) * 180.0f / PI;
    float roll  = atan2((float)xyz[1], sqrt((float)(xyz[0]*xyz[0] + xyz[2]*xyz[2]))) * 180.0f / PI;

    int      waterLevel = WTR_LVL_readPercentStable(9, 100);
    float    weight     = scale.get_units(10);
    uint32_t ts         = readTimestamp();

    // --- print everything as one block ---
    Serial.println("\n--- Sensor Reading ---");
    Serial.print("Device ID:    "); Serial.println(DEVICE_ID);
    Serial.print("Timestamp:    "); Serial.println(ts);
    Serial.print("Voltage:      "); Serial.print(vBus);              Serial.println(" V");
    Serial.print("Current:      "); Serial.print(current_mA / 1000.0); Serial.println(" A");
    Serial.print("Light:        "); Serial.print(lux);               Serial.println(" lux");
    if (scdError == NO_ERROR) {
      Serial.print("CO2:          "); Serial.print(co2);             Serial.println(" ppm");
      Serial.print("Temp (SCD41): "); Serial.print(co2Temp);         Serial.println(" °C");
      Serial.print("Humidity:     "); Serial.print(rh);              Serial.println(" %RH");
    } else {
      Serial.print("SCD41 error:  "); Serial.println(scdErrMsg);
    }
    Serial.print("Pitch:        "); Serial.print(pitch, 2);          Serial.println(" deg");
    Serial.print("Roll:         "); Serial.print(roll, 2);           Serial.println(" deg");
    Serial.print("Water level:  "); Serial.print(waterLevel);        Serial.println(" %");
    Serial.print("Weight:       "); Serial.print(weight, 1);         Serial.println(" g");
    Serial.print("Peak loudness:"); Serial.print(g_highestLoudness); Serial.println(" (last 10s)");
    char relayStr[5];
    for (int i = 0; i < 4; i++) relayStr[i] = (g_relayState & (1 << i)) ? '1' : '0';
    relayStr[4] = '\0';
    Serial.print("Relay state:  "); Serial.println(relayStr);
    Serial.println("----------------------");

    // --- build binary reading and add to LoRa payload ---
    SensorReading reading;
    reading.ts           = ts;
    reading.deviceId     = DEVICE_ID;
    reading.voltage      = (uint16_t)(vBus * 100.0f);
    reading.current      = (int16_t)(current_mA / 10.0f);
    reading.lux          = (uint16_t)constrain(lux, 0L, 65535L);
    reading.co2          = co2;
    reading.temperature  = (int16_t)(co2Temp * 100.0f);
    reading.humidity     = (uint16_t)(rh * 100.0f);
    reading.pitch        = (int16_t)(pitch * 10.0f);
    reading.roll         = (int16_t)(roll * 10.0f);
    reading.waterLevel   = (uint8_t)waterLevel;
    reading.weight       = (uint16_t)constrain((long)weight, 0L, 20000L);
    reading.peakLoudness = (uint16_t)g_highestLoudness;
    reading.relayState   = g_relayState;

    loraPayload->addReading(reading);
    Serial.print("Readings buffered: ");
    Serial.print(loraPayload->count());
    Serial.print("/");
    Serial.println(LoRaPayload::READING_COUNT);

    if (loraPayload->isReady()) {
        size_t encLen = 0;
        if (aesGcm->encrypt(loraPayload->data(), loraPayload->size(), encryptedBuf, encLen)) {
            Serial.print("LoRa packet ready: ");
            Serial.print(encLen);
            Serial.println(" bytes encrypted — transmission pending");
            // TODO: transmit encryptedBuf via LoRa (SX1262)
        } else {
            Serial.println("AES-GCM encryption failed");
        }
        loraPayload->reset();
    }

    char tBuf[20], hBuf[20], co2Buf[20], vBuf[20], iBuf[20];
    snprintf(vBuf, sizeof(vBuf), "V:   %.2f V",  vBus);
    snprintf(iBuf, sizeof(iBuf), "I:   %.3f A",  current_mA / 1000.0f);
    if (scdError == NO_ERROR) {
      snprintf(tBuf,   sizeof(tBuf),   "T:   %.1f C", co2Temp);
      snprintf(hBuf,   sizeof(hBuf),   "H:   %.0f %%", rh);
      snprintf(co2Buf, sizeof(co2Buf), "CO2: %u ppm", co2);
    } else {
      strncpy(tBuf,   "T:   ---", sizeof(tBuf));
      strncpy(hBuf,   "H:   ---", sizeof(hBuf));
      strncpy(co2Buf, "CO2: ---", sizeof(co2Buf));
    }
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.drawStr(0, 16,  vBuf);
      u8g2.drawStr(0, 36,  iBuf);
      u8g2.drawStr(0, 56,  tBuf);
      u8g2.drawStr(0, 76,  hBuf);
      u8g2.drawStr(0, 96,  co2Buf);
      u8g2.drawStr(0, 116, "Incubator OK");
    } while (u8g2.nextPage());

    unsigned long elapsed = millis() - cycleStart;
    if (elapsed < intervalMs) {
      vTaskDelay(pdMS_TO_TICKS(intervalMs - elapsed));
    }
  }
}

void soundRecorderTask(void *parameter) {
  for (;;) {
    const unsigned long durationMs = 10000;
    const unsigned long startMs = millis();
    int highestLoudness = 0;

    while (millis() - startMs < durationMs) {
      long sum = 0;
      for (int i = 0; i < 32; i++) {
        sum += analogRead(soundSensorPinAdc);
      }
      sum >>= 5;

      if (sum > highestLoudness) {
        highestLoudness = sum;
      }

      vTaskDelay(pdMS_TO_TICKS(10));
    }

    g_highestLoudness = highestLoudness;
  }
}

void relayOn(int ch)  { relay.turn_on_channel(ch);  g_relayState |=  (1 << (ch - 1)); }
void relayOff(int ch) { relay.turn_off_channel(ch); g_relayState &= ~(1 << (ch - 1)); }

void testRelay() {
  DEBUG_PRINT.println("Channel 1 (12V fan) on");
  relayOn(1);
  delay(500);
  relayOff(1);
  DEBUG_PRINT.println("Channel 2 (12V led) on");
  relayOn(2);
  delay(500);
  relayOff(2);
  DEBUG_PRINT.println("Channel 3 (12V heater) on");
  relayOn(3);
  delay(500);
  relayOff(3);
  DEBUG_PRINT.println("Channel 4 (unused) on");
  relayOn(4);
  delay(500);
  relayOff(4);
}


// MOCK — replace body with: return rtc.now().unixtime();
// Once DS1307 is wired and RTClib is added to lib_deps, that single line
// returns the same uint32_t Unix timestamp format this function produces.
uint32_t readTimestamp() {
  const uint32_t BASE_TS = 1779796800UL;  // 2026-05-26 12:00:00 UTC
  return BASE_TS + (millis() / 1000UL);
}

void initializeSCD41() {
  sensor.begin(Wire, SCD41_I2C_ADDR_62);
  uint64_t serialNumber = 0;
  delay(30);

  error = sensor.wakeUp();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.println(errorMessage);
  }
  error = sensor.stopPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.println(errorMessage);
  }
  error = sensor.reinit();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute reinit(): ");
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.println(errorMessage);
  }
  error = sensor.getSerialNumber(serialNumber);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.println(errorMessage);
    return;
  }
  Serial.print("serial number: 0x");
  Serial.println((unsigned long)(serialNumber & 0xFFFFFFFF));
}

bool isButtonPressed() {
  static unsigned long pressStart = 0;
  static bool alreadyFired = false;
  static bool seenHighOnce = false;

  int state = digitalRead(buttonPin);

  if (!seenHighOnce) {
    if (state == HIGH) seenHighOnce = true;
    return false;
  }

  if (state == LOW) {
    if (pressStart == 0) pressStart = millis();
    if (!alreadyFired && millis() - pressStart >= 1000) {
      alreadyFired = true;
      return true;
    }
  } else {
    pressStart = 0;
    alreadyFired = false;
  }
  return false;
}

void playSoundNotification() {
  tone(BUZZER_PIN, 400);
  delay(40);
  noTone(BUZZER_PIN);
  delay(60);
  tone(BUZZER_PIN, 1200);
  delay(40);
  noTone(BUZZER_PIN);
}

void WTR_LVL_readBytes(byte addr, byte *buf, byte len) {
  Wire.requestFrom((int)addr, (int)len);
  unsigned long t = millis();
  while (Wire.available() < len) {
    if (millis() - t > 500) break;
  }
  for (byte i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
}

int WTR_LVL_readPercent() {
  uint32_t WTR_LVL_mask = 0;

  WTR_LVL_readBytes(WTR_LVL_LOW_ADDR, WTR_LVL_lowData, 8);
  WTR_LVL_readBytes(WTR_LVL_HIGH_ADDR, WTR_LVL_highData, 12);

  for (byte i = 0; i < 8; i++) {
    if (WTR_LVL_lowData[i] > WTR_LVL_THRESHOLD) WTR_LVL_mask |= (1UL << i);
  }
  for (byte i = 0; i < 12; i++) {
    if (WTR_LVL_highData[i] > WTR_LVL_THRESHOLD) WTR_LVL_mask |= (1UL << (8 + i));
  }

  byte WTR_LVL_sections = 0;
  while (WTR_LVL_mask & 1) {
    WTR_LVL_sections++;
    WTR_LVL_mask >>= 1;
  }

  return WTR_LVL_sections * 5;
}

int WTR_LVL_readPercentStable(uint8_t samples, uint16_t delayMs) {
  uint8_t histogram[21] = {0};

  for (uint8_t i = 0; i < samples; i++) {
    int pct = WTR_LVL_readPercent();
    if (pct >= 0 && pct <= 100 && pct % 5 == 0) {
      histogram[pct / 5]++;
    }
    delay(delayMs);
  }

  uint8_t bestIndex = 0;
  uint8_t bestCount = 0;
  for (uint8_t i = 0; i <= 20; i++) {
    if (histogram[i] > bestCount) {
      bestCount = histogram[i];
      bestIndex = i;
    }
  }

  return bestIndex * 5;
}
