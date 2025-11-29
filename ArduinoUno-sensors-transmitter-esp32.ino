#include <OneWire.h>
#include <DallasTemperature.h>

/* -----------------------
   DS18B20 TEMP SENSOR
------------------------*/
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/* -----------------------
   TDS SENSOR
------------------------*/
#define TdsSensorPin A1
float tdsRaw = 0;
float tdsCompensated = 0;
float voltageTDS = 0;

/* -----------------------
   pH SENSOR
------------------------*/
#define PhSensorPin A0
float phValue = 0;
float voltagePH = 0;

/* -----------------------
   TURBIDITY SENSOR
------------------------*/
#define TurbidityPin A2
float voltageTurb = 0;
float turbidityNTU = 0;

/* -----------------------
   SETTINGS
------------------------*/
#define NUM_SAMPLES 5  // Number of analog readings to average

void setup() {
  Serial.begin(9600);  // TX to ESP32
  sensors.begin();
}

float readAnalogAverage(int pin) {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return sum / (float)NUM_SAMPLES;
}

void loop() {
  // --- READ TEMPERATURE
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // --- READ TDS SENSOR
  float sensorValueTDS = readAnalogAverage(TdsSensorPin);
  voltageTDS = sensorValueTDS * (5.0 / 1024.0);

  // Raw TDS
  tdsRaw = (133.42 * voltageTDS * voltageTDS * voltageTDS
            - 255.86 * voltageTDS * voltageTDS
            + 857.39 * voltageTDS) * 0.5;

  // Temp-compensated TDS
  float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);
  float compensationVoltage = voltageTDS / compensationCoefficient;
  tdsCompensated = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
                    - 255.86 * compensationVoltage * compensationVoltage
                    + 857.39 * compensationVoltage) * 0.5;

  // --- READ pH SENSOR
  float sensorValuePH = readAnalogAverage(PhSensorPin);
  voltagePH = sensorValuePH * (5.0 / 1024.0);

  // pH conversion
  phValue = 7 + ((2.5 - voltagePH) / 0.18);

  // --- READ TURBIDITY SENSOR
  float sensorValueTurb = readAnalogAverage(TurbidityPin);
  voltageTurb = sensorValueTurb * (5.0 / 1024.0);

  // Approximate NTU formula
  turbidityNTU = -1120.4 * voltageTurb * voltageTurb + 5742.3 * voltageTurb - 4352.9;

  // --- SEND CSV TO ESP32
  Serial.print(tempC, 2); Serial.print(",");
  Serial.print(tdsRaw, 2); Serial.print(",");
  Serial.print(tdsCompensated, 2); Serial.print(",");
  Serial.print(phValue, 2); Serial.print(",");
  Serial.println(turbidityNTU, 2);

  delay(1500); // send every 1.5s
}
