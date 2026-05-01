#include <Wire.h>
#include <INA226_WE.h>
#include <Digital_Light_TSL2561.h>
#include <SensirionI2cScd4x.h>

INA226_WE ina226 = INA226_WE(0x40);  // Default I2C addr

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
    while (1);
  }
  ina226.setAverage(INA226_AVERAGE_16);  // Smooth
  ina226.setConversionTime(INA226_CONV_TIME_1100);  // µs
  ina226.setMeasureMode(INA226_CONTINUOUS);  // Or TRIGGERED
  ina226.waitUntilConversionCompleted();  // Initial

  TSL2561.init();

  initializeSCD41();
  Serial.println("CO2, Temp, Humidity Sensor starts work!");
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