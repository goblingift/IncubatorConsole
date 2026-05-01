#include <Wire.h>
#include <INA226_WE.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>
#include <multi_channel_relay.h>
#include <U8g2lib.h>
#include <SPI.h>

U8G2_SH1107_SEEED_128X128_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

INA226_WE ina226 = INA226_WE(0x40);  // Default I2C addr

Multi_Channel_Relay relay;

const byte BUZZER_PIN = A0;

SensirionI2cScd4x sensor;
static int16_t error;
static char errorMessage[64];

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

void setup() {
  Serial.begin(9600);
  Wire.begin();
  if (!ina226.init()) {
    Serial.println("INA226 not found!");
  }
  ina226.setAverage(INA226_AVERAGE_16);  // Smooth
  ina226.setConversionTime(INA226_CONV_TIME_1100);  // µs
  ina226.setMeasureMode(INA226_CONTINUOUS);  // Or TRIGGERED
  ina226.waitUntilConversionCompleted();  // Initial

  TSL2561.init();

  initializeSCD41();
  Serial.println("CO2, Temp, Humidity Sensor starts work!");

  u8g2.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  // Set I2C address and start relay
  relay.begin(0x11);
  testRelay();
}

void loop() {

    uint16_t co2 = 0;
    float temp = 0.0;
    float rh = 0.0;

  float vBus = ina226.getBusVoltage_V();  // V
  float current_mA = ina226.getCurrent_mA();  // Convert to A: /1000.0
  Serial.print("Voltage: "); Serial.print(vBus); Serial.println(" V");
  Serial.print("Current: "); Serial.print(current_mA / 1000.0); Serial.println(" A");
  
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

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0,24,"Hello World!");
  } while ( u8g2.nextPage() );

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