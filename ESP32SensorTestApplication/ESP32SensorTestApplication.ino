#include <Wire.h>
#include <INA226_WE.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>
#include <multi_channel_relay.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "HX711.h"
#include <ADXL345.h>
#include <math.h>

U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

INA226_WE ina226 = INA226_WE(0x40);  // Default I2C addr
HX711 scale;
Multi_Channel_Relay relay;
ADXL345 adxl;

const byte BUZZER_PIN = D7;
const byte HUMID_PIN = A9;
const int buttonPin = D2;
const int DT_PIN = 3;   // HX711 to ESP32S3 GPIO3/D2/A2
const int SCK_PIN = 2;  // HX711 to ESP32S3 GPIO2/D1/A1
const float HX711_CALIBRATION_FACTOR = 112.0392;

#define WATER_SENSOR_PIN 3
#define DS18B20_PIN 1
#define WTR_LVL_THRESHOLD 100
#define WTR_LVL_HIGH_ADDR 0x78
#define WTR_LVL_LOW_ADDR  0x77

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

SensirionI2cScd4x sensor;
static int16_t error;
static char errorMessage[64];
int lastState = HIGH;
byte WTR_LVL_lowData[8];
byte WTR_LVL_highData[12];

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialize INA226- current/voltage sensor
  if (!ina226.init()) {
    Serial.println("INA226 not found!");
  }
  ina226.setAverage(INA226_AVERAGE_16);             // Smooth
  ina226.setConversionTime(INA226_CONV_TIME_1100);  // µs
  ina226.setMeasureMode(INA226_CONTINUOUS);         // Or TRIGGERED
  ina226.waitUntilConversionCompleted();            // Initial

  // Initialize light sensor
  TSL2561.init();

  // Initialize SCD41- CO2, Temp., Humid. sensor
  initializeSCD41();
  Serial.println("CO2, Temp, Humidity Sensor starts work!");

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  playSoundNotification();

  // Initialize water level sensor
  Serial.print("Water level: ");
  Serial.print(WTR_LVL_readPercent());
  Serial.println("%");

  // Initialize input button
  pinMode(buttonPin, INPUT);
  Serial.println("Input button initialized");

  // Initialize DS18B20
  sensors.begin();
  Serial.println("DS18B20 initialized");

  // Initialize ADXL345- Accelerometer
  adxl.powerOn();
  Serial.println("ADXL345 Accelerometer initialized");

  // Initialize scale
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(HX711_CALIBRATION_FACTOR);
  scale.tare(50);
  Serial.println("Initialized HX711 scale");

  // Initialize relay
  relay.begin(0x11);
  testRelay();

  // Initialize humidifier
  Serial.println("Test Humidifier for 2secs");
  pinMode(HUMID_PIN, OUTPUT);
  digitalWrite(HUMID_PIN, HIGH);
  delay(2000);
  digitalWrite(HUMID_PIN, LOW);

  // Initialize OLED screen
  u8g2.begin();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 24, "Incubator started!");
  } while (u8g2.nextPage());
  delay(2000);

  Serial.println("Setup complete!");
}

void loop() {

  if (isButtonPressed()) {
    Serial.println("Button pressed!");
    playSoundNotification();
  }

  uint16_t co2 = 0;
  float temp = 0.0;
  float rh = 0.0;

  float vBus = ina226.getBusVoltage_V();      // V
  float current_mA = ina226.getCurrent_mA();  // Convert to A: /1000.0
  Serial.print("Voltage: ");
  Serial.print(vBus);
  Serial.println(" V");
  Serial.print("Current: ");
  Serial.print(current_mA / 1000.0);
  Serial.println(" A");

  Serial.print("The Light value is: ");
  Serial.println(TSL2561.readVisibleLux());

  error = sensor.measureAndReadSingleShot(co2, temp, rh);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute measureAndReadSingleShot(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  } else {
    printMeasurement(co2, temp, rh);
  }

  measureDS18B20();

  measureAccelerometer();

  Serial.print("Water level: ");
  Serial.print(WTR_LVL_readPercent());
  Serial.println("%");

  float weight = scale.get_units(10);
  Serial.print("Weight: ");
  Serial.print(weight, 1);
  Serial.println(" g");

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 24, "Incubator running!");
  } while (u8g2.nextPage());
}

void testRelay() {
  /* Begin Controlling Relay */
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

void measureDS18B20() {
  Serial.print("Requesting temperatures from DS18B20");
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature for DS18B20 is: ");
    Serial.println(tempC);
  } else {
    Serial.println("Error: Could not read temperature data from DS18B20");
  }
}

void measureAccelerometer() {
  double xyz[3];
  adxl.getAcceleration(xyz);

  float ax = xyz[0];
  float ay = xyz[1];
  float az = xyz[2];

  float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float roll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;

  Serial.print("Pitch = ");
  Serial.print(pitch, 2);
  Serial.println(" deg");

  Serial.print("Roll  = ");
  Serial.print(roll, 2);
  Serial.println(" deg");
}


void printMeasurement(uint16_t co2, float temp, float rh) {
  Serial.print("CO2 concentration [ppm]: ");
  Serial.print(co2);
  Serial.println();
  Serial.print("Temperature [°C]: ");
  Serial.print(temp);
  Serial.println();
  Serial.print("Relative Humidity [RH]: ");
  Serial.print(rh);
  Serial.println();
}

void initializeSCD41() {
  Wire.begin();
  sensor.begin(Wire, SCD41_I2C_ADDR_62);
  uint64_t serialNumber = 0;
  delay(30);

  error = sensor.wakeUp();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = sensor.stopPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = sensor.reinit();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute reinit(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  // Read out information about the sensor
  error = sensor.getSerialNumber(serialNumber);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  Serial.print("serial number: 0x");
  Serial.println(serialNumber);
}

bool isButtonPressed() {
  int state = digitalRead(buttonPin);
  bool pressed = (lastState == LOW && state == HIGH);
  lastState = state;
  return pressed;
}

void playSoundNotification() {
  tone(BUZZER_PIN, 400);   // low tone
  delay(40);
  noTone(BUZZER_PIN);
  delay(60);
  tone(BUZZER_PIN, 1200);  // high tone
  delay(40);
  noTone(BUZZER_PIN);
}

void WTR_LVL_readBytes(byte addr, byte *buf, byte len) {
  Wire.requestFrom(addr, len);
  while (Wire.available() < len);
  for (byte i = 0; i < len; i++) buf[i] = Wire.read();
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