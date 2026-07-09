#include <Arduino.h>
#include <Wire.h>
#include "DS1307.h"
#include <Adafruit_INA260.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>
#include <multi_channel_relay.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <RadioLib.h>
#include "HX711.h"
#include <ADXL345.h>
#include <math.h>
#include "SensorReading.h"
#include "LoRaPayload.h"
#include "AesGcm.h"
#include "SettingsMessages.h"
#include "IncubatorSettings.h"
#include "SettingsSync.h"
#include "ControlLogic.h"
#include "DisplayUi.h"

U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DS1307 rtc;
Adafruit_INA260 ina260;
HX711 scale;
Multi_Channel_Relay relay;
ADXL345 adxl;
SensirionI2cScd4x sensor;

// --- SX1262 (Wio SX1262 + XIAO ESP32S3 B2B connector) ---
static constexpr int LORA_NSS  = 41;
static constexpr int LORA_DIO1 = 39;
static constexpr int LORA_RST  = 42;
static constexpr int LORA_BUSY = 40;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Static
static constexpr float   LORA_FREQ_MHZ    = 868.0;
static constexpr float   LORA_BW_KHZ      = 125.0;
static constexpr uint8_t LORA_CR          = 5;       // 4/5
static constexpr uint8_t LORA_SYNC_WORD   = 0x14;    // private network
static constexpr uint8_t LORA_PREAMBLE    = 8;
// Configurable
static constexpr uint8_t LORA_SF          = 7;
static constexpr int8_t  LORA_TX_POWER    = 13;

static bool g_loraOk = false;

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
const byte HUMID_PIN = D7;
// const int buttonPin = D0;  // DISABLED: reused for sound sensor
const int soundSensorPinAdc = D0;
const int DT_PIN = 3;
const int SCK_PIN = 2;
const float HX711_CALIBRATION_FACTOR = 112.0392;
// const int soundSensorPinAdc = A8;  // DISABLED: conflicts with SPI SCK (GPIO 7)

TaskHandle_t soundTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t settingsSyncTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

volatile int g_highestLoudness = 0;
volatile uint8_t g_actuatorState = 0;  // bitmask: bit0=relay1, bit1=relay2, bit2=relay3, bit3=relay4, bit4=humidifier
static bool g_rtcOk = false;

#define HUMIDIFIER_BIT (1 << 4)

// Relay channel semantics (wiring, see testRelay)
static constexpr int RELAY_CH_FAN       = 1;
static constexpr int RELAY_CH_ALERT_LED = 2;
static constexpr int RELAY_CH_HEATER    = 3;

SemaphoreHandle_t g_i2cMutex      = NULL;  // guards every Wire transaction (sensors, relay, RTC, OLED)
SemaphoreHandle_t g_radioMutex    = NULL;  // guards the SX1262 and the shared AesGcm instance
SemaphoreHandle_t g_settingsMutex = NULL;  // guards g_settings

static SettingsResponse    g_settings;
static SettingsSyncContext g_syncCtx;
static DisplayUi           displayUi;

static inline void i2cLock()   { xSemaphoreTake(g_i2cMutex, portMAX_DELAY); }
static inline void i2cUnlock() { xSemaphoreGive(g_i2cMutex); }

#define WTR_LVL_THRESHOLD 100
#define WTR_LVL_HIGH_ADDR 0x78
#define WTR_LVL_LOW_ADDR 0x77

void initializeSCD41();
int16_t scd41StartSingleShot();
uint32_t readTimestamp();
void relayOn(int ch);
void relayOff(int ch);
void testRelay();
void humidifierOn();
void humidifierOff();
// bool isButtonPressed();
// void playSoundNotification();
void WTR_LVL_readBytes(byte addr, byte *buf, byte len);
int WTR_LVL_readPercent();
int WTR_LVL_readPercentStable(uint8_t samples = 15, uint16_t delayMs = 120);
void soundRecorderTask(void *parameter);
void sensorReadingTask(void *parameter);

void setup() {
  Serial.begin(9600);
  delay(5000);
  Wire.begin();
  Wire.setTimeOut(1000);

  g_i2cMutex      = xSemaphoreCreateMutex();
  g_radioMutex    = xSemaphoreCreateMutex();
  g_settingsMutex = xSemaphoreCreateMutex();

  if (SettingsStore::loadFromNvs(g_settings)) {
    Serial.printf("Settings loaded from NVS (version 0x%08lX)\n",
                  (unsigned long)g_settings.settingsVersion);
  } else {
    g_settings = SettingsStore::defaults(DEVICE_ID);
    Serial.println("No settings in NVS — using built-in defaults");
  }

  loraPayload = new LoRaPayload();
  aesGcm      = new AesGcm(AES_KEY);  // nonce domain 0x00 (gateway responses use 0x01)

  rtc.begin();
  Wire.beginTransmission(0x68);
  g_rtcOk = (Wire.endTransmission() == 0);
  if (!g_rtcOk) {
    Serial.println("DS1307 RTC not found on I2C (0x68)! Check wiring.");
  } else {
    // To set the RTC time: uncomment the three lines below, flash once, then re-comment and reflash.
    //rtc.fillByYMD(2026, 7, 7);
    //rtc.fillByHMS(16, 42, 0);
    //rtc.setTime();
    rtc.getTime();
    Serial.printf("DS1307 RTC OK — time (UTC timezone): %02d:%02d:%02d\n", rtc.hour, rtc.minute, rtc.second);
  }

  if (!ina260.begin()) {
    Serial.println("Could not find INA260 chip. Check wiring!");
  }

  TSL2561.init();

  initializeSCD41();
  Serial.println("CO2, Temp, Humidity Sensor starts work!");

  // DISABLED: buzzer pin shared with humidifier
  // pinMode(BUZZER_PIN, OUTPUT);
  // playSoundNotification();

  delay(2500);
  Serial.print("Water level: ");
  Serial.print(WTR_LVL_readPercentStable(20, 150));
  Serial.println("%");

  // pinMode(buttonPin, INPUT_PULLUP);  // DISABLED: D0 reused for sound sensor
  // Serial.println("Input button initialized");

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
  humidifierOn();
  delay(2000);
  humidifierOff();

  Serial.print("[SX1262] Initializing ... ");
  int loraState = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                              LORA_SYNC_WORD, LORA_TX_POWER, LORA_PREAMBLE);
  if (loraState == RADIOLIB_ERR_NONE) {
    radio.setCRC(true);
    g_loraOk = true;
    Serial.println("OK");
  } else {
    Serial.print("FAILED (code ");
    Serial.print(loraState);
    Serial.println(")");
  }

  u8g2.begin();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 24, "Incubator started!");
  } while (u8g2.nextPage());
  delay(2000);

  displayUi.begin(&u8g2, g_i2cMutex);

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

  g_syncCtx = { &radio, aesGcm, g_radioMutex, g_settingsMutex, &g_settings, DEVICE_ID, &g_loraOk };
  xTaskCreatePinnedToCore(
    settingsSyncTask,
    "SettingsSync",
    8192,
    &g_syncCtx,
    1,
    &settingsSyncTaskHandle,
    1
  );

  xTaskCreatePinnedToCore(
    displayUiTask,
    "Display",
    4096,
    &displayUi,
    1,
    &displayTaskHandle,
    1
  );

  Serial.println("Setup complete!");
}

void loop() {
  // Button disabled — D0 reused for sound sensor
  vTaskDelay(pdMS_TO_TICKS(50));
}

void sensorReadingTask(void *parameter) {
  const unsigned long intervalMs = 10000;
  for (;;) {
    unsigned long cycleStart = millis();
    // --- collect all values first ---
    i2cLock();
    float current_mA  = ina260.readCurrent();
    float vBus        = ina260.readBusVoltage() / 1000.0f;
    long  lux         = TSL2561.readVisibleLux();
    i2cUnlock();

    // SCD41 single-shot, split into start/wait/read so the I2C bus stays free
    // during the ~5 s measurement window (the display task needs it every 5 s).
    uint16_t co2 = 0;
    float    co2Temp = 0.0;
    float    rh = 0.0;
    bool     scdValid = false;
    char     scdErrMsg[64] = {0};
    i2cLock();
    int16_t scdError = scd41StartSingleShot();
    i2cUnlock();
    if (scdError == NO_ERROR) {
      vTaskDelay(pdMS_TO_TICKS(4900));
      bool scdReady = false;
      for (int i = 0; i < 12 && !scdReady; i++) {
        i2cLock();
        scdError = sensor.getDataReadyStatus(scdReady);
        i2cUnlock();
        if (scdError != NO_ERROR) break;
        if (!scdReady) vTaskDelay(pdMS_TO_TICKS(100));
      }
      if (scdError == NO_ERROR && scdReady) {
        i2cLock();
        scdError = sensor.readMeasurement(co2, co2Temp, rh);
        i2cUnlock();
        scdValid = (scdError == NO_ERROR);
      }
    }
    if (!scdValid) {
      if (scdError != NO_ERROR) {
        errorToString(scdError, scdErrMsg, sizeof(scdErrMsg));
      } else {
        strncpy(scdErrMsg, "data not ready (timeout)", sizeof(scdErrMsg) - 1);
      }
    }

    double xyz[3];
    i2cLock();
    adxl.getAcceleration(xyz);
    i2cUnlock();
    float pitch = atan2((float)xyz[0], sqrt((float)(xyz[1]*xyz[1] + xyz[2]*xyz[2]))) * 180.0f / PI;
    float roll  = atan2((float)xyz[1], sqrt((float)(xyz[0]*xyz[0] + xyz[2]*xyz[2]))) * 180.0f / PI;

    int      waterLevel = WTR_LVL_readPercentStable(9, 100);  // locks per sample internally
    float    weight     = scale.get_units(10);                // bit-banged GPIO, no I2C
    uint32_t ts         = readTimestamp();                    // locks internally

    // --- print everything as one block ---
    Serial.println("\n--- Sensor Reading ---");
    Serial.print("Device ID:    "); Serial.println(DEVICE_ID);
    Serial.print("Timestamp:    "); Serial.println(ts);
    Serial.print("Voltage:      "); Serial.print(vBus);              Serial.println(" V");
    Serial.print("Current:      "); Serial.print(current_mA / 1000.0); Serial.println(" A");
    Serial.print("Light:        "); Serial.print(lux);               Serial.println(" lux");
    if (scdValid) {
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
    char relayStr[6];
    for (int i = 0; i < 5; i++) relayStr[i] = (g_actuatorState & (1 << i)) ? '1' : '0';
    relayStr[5] = '\0';
    Serial.print("Actuator state (relay1-4,humid): "); Serial.println(relayStr);
    Serial.println("----------------------");

    // --- evaluate thresholds and drive the actuators ---
    Measurements m;
    m.voltage      = vBus;
    m.currentA     = current_mA / 1000.0f;
    m.temperature  = co2Temp;
    m.humidity     = rh;
    m.co2          = co2;
    m.scdValid     = scdValid;
    m.pitch        = pitch;
    m.roll         = roll;
    m.weightG      = (float)constrain((long)weight, 0L, 20000L);
    m.waterLevel   = waterLevel;
    m.peakLoudness = g_highestLoudness;

    xSemaphoreTake(g_settingsMutex, portMAX_DELAY);
    ControlSettings cs = SettingsStore::toFloats(g_settings);
    xSemaphoreGive(g_settingsMutex);

    bool prevFan    = g_actuatorState & (1 << (RELAY_CH_FAN - 1));
    bool prevAlert  = g_actuatorState & (1 << (RELAY_CH_ALERT_LED - 1));
    bool prevHeater = g_actuatorState & (1 << (RELAY_CH_HEATER - 1));
    bool prevHumid  = g_actuatorState & HUMIDIFIER_BIT;

    ControlResult ctl = ControlLogic::evaluate(m, cs, prevFan, prevHeater, prevHumid);

    // Switch relays only on state change, before the reading is built below
    // so its actuatorState already carries the new states.
    if (ctl.fanOn != prevFan)          { ctl.fanOn        ? relayOn(RELAY_CH_FAN)       : relayOff(RELAY_CH_FAN); }
    if (ctl.heaterOn != prevHeater)    { ctl.heaterOn     ? relayOn(RELAY_CH_HEATER)    : relayOff(RELAY_CH_HEATER); }
    if (ctl.alertOn != prevAlert)      { ctl.alertOn      ? relayOn(RELAY_CH_ALERT_LED) : relayOff(RELAY_CH_ALERT_LED); }
    if (ctl.humidifierOn != prevHumid) { ctl.humidifierOn ? humidifierOn()              : humidifierOff(); }

    Serial.printf("Control: fan=%d heater=%d humidifier=%d alert=%d, %u violation(s)\n",
                  ctl.fanOn, ctl.heaterOn, ctl.humidifierOn, ctl.alertOn, ctl.violationCount);
    for (uint8_t i = 0; i < ctl.violationCount; i++) {
      Serial.print("  ! "); Serial.println(ctl.lines[i]);
    }

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
    reading.weight       = (uint16_t)m.weightG;
    reading.peakLoudness = (uint16_t)m.peakLoudness;
    reading.actuatorState = g_actuatorState;

    loraPayload->addReading(reading);
    Serial.print("Readings buffered: ");
    Serial.print(loraPayload->count());
    Serial.print("/");
    Serial.println(LoRaPayload::READING_COUNT);

    if (loraPayload->isReady()) {
        size_t encLen = 0;
        // Radio mutex covers crypto too: the AesGcm instance is shared with
        // the settings-sync task. Worst-case wait is its ~2 s listen window.
        xSemaphoreTake(g_radioMutex, portMAX_DELAY);
        if (aesGcm->encrypt(loraPayload->data(), loraPayload->size(), encryptedBuf, encLen)) {
            Serial.print("LoRa packet ready: ");
            Serial.print(encLen);
            Serial.println(" bytes encrypted");

            Serial.println("--- Encrypted payload (hex) ---");
            for (size_t i = 0; i < encLen; i++) {
                if (encryptedBuf[i] < 0x10) Serial.print('0');
                Serial.print(encryptedBuf[i], HEX);
            }
            Serial.println();
            Serial.println("-------------------------------");

            if (g_loraOk) {
                int txState = radio.transmit(encryptedBuf, encLen);
                if (txState == RADIOLIB_ERR_NONE) {
                    Serial.println("[SX1262] Transmitted OK");
                } else {
                    Serial.print("[SX1262] TX failed (code ");
                    Serial.print(txState);
                    Serial.println(")");
                }
            } else {
                Serial.println("[SX1262] Radio not available — skipping TX");
            }
        } else {
            Serial.println("AES-GCM encryption failed");
        }
        xSemaphoreGive(g_radioMutex);
        loraPayload->reset();
    }

    // --- hand the cycle's results to the display task ---
    DisplaySnapshot snap = {};
    snap.valid          = true;
    snap.voltage        = vBus;
    snap.currentA       = current_mA / 1000.0f;
    snap.temperature    = co2Temp;
    snap.humidity       = rh;
    snap.co2            = co2;
    snap.scdValid       = scdValid;
    snap.hour           = rtc.hour;
    snap.minute         = rtc.minute;
    snap.alert          = ctl.alertOn;
    snap.violationCount = ctl.violationCount;
    memcpy(snap.lines, ctl.lines, sizeof(snap.lines));
    displayUi.publish(snap);

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

void relayOn(int ch)  { i2cLock(); relay.turn_on_channel(ch);  i2cUnlock(); g_actuatorState |=  (1 << (ch - 1)); }
void relayOff(int ch) { i2cLock(); relay.turn_off_channel(ch); i2cUnlock(); g_actuatorState &= ~(1 << (ch - 1)); }

void humidifierOn()  { digitalWrite(HUMID_PIN, HIGH); g_actuatorState |=  HUMIDIFIER_BIT; }
void humidifierOff() { digitalWrite(HUMID_PIN, LOW);  g_actuatorState &= ~HUMIDIFIER_BIT; }

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


uint32_t readTimestamp() {
  if (!g_rtcOk) return millis() / 1000UL;
  i2cLock();
  rtc.getTime();
  i2cUnlock();
  // Sanity-check: if any field is out of range the DS1307 returned garbage.
  if (rtc.month  < 1 || rtc.month  > 12 ||
      rtc.hour   > 23 || rtc.minute > 59 || rtc.second > 59) {
    Serial.println("RTC returned invalid data — check wiring/power.");
    return millis() / 1000UL;
  }
  uint16_t y = (uint16_t)rtc.year + 2000;
  uint8_t  m = rtc.month;
  uint8_t  d = rtc.dayOfMonth;
  if (m < 3) { m += 12; y--; }
  uint32_t days = 365UL * y + y/4 - y/100 + y/400
                + (153 * m - 457) / 5 + d - 719469UL;
  return days * 86400UL
       + (uint32_t)rtc.hour   * 3600UL
       + (uint32_t)rtc.minute * 60UL
       + rtc.second;
}

// Sends the SCD41 measure_single_shot command (0x219d) WITHOUT the driver's
// built-in 5-second delay(), so the caller can wait with the I2C bus unlocked.
// Mirrors SensirionI2cScd4x::measureSingleShot().
int16_t scd41StartSingleShot() {
  uint8_t buffer[2];
  SensirionI2CTxFrame txFrame =
      SensirionI2CTxFrame::createWithUInt16Command(0x219d, buffer, 2);
  return SensirionI2CCommunication::sendFrame(SCD41_I2C_ADDR_62, txFrame, Wire);
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

// DISABLED: button removed, D0 reused for sound sensor
// bool isButtonPressed() { ... }

// DISABLED: buzzer pin shared with humidifier
// void playSoundNotification() { ... }

void WTR_LVL_readBytes(byte addr, byte *buf, byte len) {
  i2cLock();
  Wire.requestFrom((int)addr, (int)len);
  unsigned long t = millis();
  while (Wire.available() < len) {
    if (millis() - t > 500) break;
  }
  for (byte i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  i2cUnlock();
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
