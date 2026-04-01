// =============================================================
// WCS1500 Hall Current Sensor + ESP32 (30-pin breakout board)
// Aout -> 10kΩ -> GPIO34 -> 6.8kΩ -> GND
// Measured VDD = 4.6V
// =============================================================

const int SENSOR_PIN = 34;

// --- Sensor specs from datasheet ---
const float VDD          = 4.6;       // Measured supply voltage
const float SENSITIVITY  = 0.011 * (VDD / 5.0);  // Ratiometric: ~10.12 mV/A
const float V_OUT_MIN    = 0.3;
const float V_OUT_MAX    = VDD - 0.3;

// --- Voltage divider (10kΩ + 6.8kΩ) ---
const float R1 = 10000.0;
const float R2 = 6800.0;
const float DIVIDER = R2 / (R1 + R2);

// --- ESP32 ADC ---
const float ADC_RESOLUTION = 4095.0;
const float ADC_REF        = 3.3;

// --- Calibration ---
float zeroOffset = 0;

// Moving average filter
const int FILTER_SIZE = 50;
float readings[FILTER_SIZE];
int readIndex = 0;
float readTotal = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);

  for (int i = 0; i < FILTER_SIZE; i++) readings[i] = 0;

  Serial.println("=== WCS1500 Calibration ===");
  Serial.println("Ensure NO current is flowing...");
  //delay(3000);

  float sum = 0;
  const int calSamples = 500;
  for (int i = 0; i < calSamples; i++) {
    sum += readSensorVoltage();
    delay(5);
  }
  zeroOffset = sum / calSamples;

  Serial.print("Calibrated zero offset: ");
  Serial.print(zeroOffset, 4);
  Serial.print(" V (expected ~");
  Serial.print(VDD / 2.0, 1);
  Serial.println(" V)");

  // Adjusted sanity check for ratiometric + ADC non-linearity
  float expectedZero = VDD / 2.0;
  if (zeroOffset < (expectedZero - 0.8) || zeroOffset > (expectedZero + 0.5)) {
    Serial.println("WARNING: Zero offset out of expected range!");
    Serial.println("Check wiring: VDD->5V, GND->GND, Aout->divider->GPIO34");
  } else {
    Serial.println("Calibration OK!");
  }

  Serial.println("\n  Sensor V  |  Current (A)  |  Power @ 12V");
  Serial.println("  ---------|--------------|-------------");
}

float readSensorVoltage() {
  long total = 0;
  const int samples = 100;
  for (int i = 0; i < samples; i++) {
    total += analogRead(SENSOR_PIN);
    delayMicroseconds(100);
  }
  float avgADC = (float)total / samples;
  float pinVoltage = (avgADC / ADC_RESOLUTION) * ADC_REF;
  return pinVoltage / DIVIDER;
}

float filteredCurrent(float newReading) {
  readTotal -= readings[readIndex];
  readings[readIndex] = newReading;
  readTotal += readings[readIndex];
  readIndex = (readIndex + 1) % FILTER_SIZE;
  return readTotal / FILTER_SIZE;
}

void loop() {
  float sensorV = readSensorVoltage();
  sensorV = constrain(sensorV, V_OUT_MIN, V_OUT_MAX);

  float rawCurrent = (sensorV - zeroOffset) / SENSITIVITY;
  float current = filteredCurrent(rawCurrent);
  float power = current * 12.0;

  Serial.printf("  %6.3f V  |  %+8.1f A   |  %7.1f W\n",
                sensorV, current, power);

  delay(200);
}
