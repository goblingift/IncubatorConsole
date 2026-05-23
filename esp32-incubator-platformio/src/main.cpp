#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>
#include <multi_channel_relay.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "HX711.h"
#include <ADXL345.h>
#include <math.h>

U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
INA226_WE ina226(0x40);
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

const byte BUZZER_PIN = D7;
const byte HUMID_PIN = A9;
const int buttonPin = D0;
const int DT_PIN = 3;
const int SCK_PIN = 2;
const float HX711_CALIBRATION_FACTOR = 112.0392;
const int soundSensorPinAdc = A8;

TaskHandle_t soundTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;

#define WTR_LVL_THRESHOLD 100
#define WTR_LVL_HIGH_ADDR 0x78
#define WTR_LVL_LOW_ADDR 0x77

void initializeSCD41();
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

  if (!ina226.init()) {
    Serial.println("INA226 not found!");
  }
  ina226.setAverage(INA226_AVERAGE_16);
  ina226.setConversionTime(INA226_CONV_TIME_1100);
  ina226.setMeasureMode(INA226_CONTINUOUS);
  ina226.waitUntilConversionCompleted();

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
    8192,
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
  const unsigned long intervalMs = 15000;
  for (;;) {
    unsigned long cycleStart = millis();
    // --- collect all values first ---
    float vBus        = ina226.getBusVoltage_V();
    float current_mA  = ina226.getCurrent_mA();
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

    int   waterLevel = WTR_LVL_readPercentStable(9, 100);
    float weight     = scale.get_units(10);

    // --- print everything as one block ---
    Serial.println("\n--- Sensor Reading ---");
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
    Serial.println("----------------------");

    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.drawStr(0, 24, "Incubator running!");
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

    Serial.print("Highest loudness in last 10s: ");
    Serial.println(highestLoudness);
  }
}

void testRelay() {
  DEBUG_PRINT.println("Channel 1 on");
  relay.turn_on_channel(1);
  delay(500);
  DEBUG_PRINT.println("Channel 2 on");
  relay.turn_off_channel(1);
  relay.turn_on_channel(2);
  delay(500);
  DEBUG_PRINT.println("Channel 3 on");
  relay.turn_off_channel(2);
  relay.turn_on_channel(3);
  delay(500);
  DEBUG_PRINT.println("Channel 4 on");
  relay.turn_off_channel(3);
  relay.turn_on_channel(4);
  delay(500);
  relay.turn_off_channel(4);
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
