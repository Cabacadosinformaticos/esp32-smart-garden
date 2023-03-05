// Tunable constants for the smart garden sketch: pins, sensor calibration,
// watering thresholds and timing. Change them here instead of in the sketch.

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h> // Needed for the fixed width integer types used below

// Pins (ESP32 GPIO numbers)
const int dhtSensorPin = 18; // GPIO18: DHT11 data pin (temperature and air humidity)
const int relayPin = 19; // GPIO19: relay input that switches the water pump
const int lightSensorPin = 34; // GPIO34 (ADC1): KY-018 light sensor analog output
const int waterSensorPin = 35; // GPIO35: contactless liquid level sensor digital output (HIGH = water in the tank)
const int soilSensorPin = 32; // GPIO32 (ADC1): soil moisture sensor analog output

// ADC and soil sensor calibration
const int adcMaxValue = 4095; // 12-bit ADC of the ESP32
const float adcReferenceVoltage = 3.3; // ESP32 ADC input range, not 5 V
// These two values describe the sensor curve and must be calibrated for the sensor in use.
// They were measured as 0.92 V and 0.08 V per % on a 5 V scale, so they are scaled by 3.3 / 5
// to keep the same curve with the real 3.3 V reference of the ESP32.
const float soilVoltageDry = 0.92 * 3.3 / 5.0; // voltage that is converted to 0 % (offset of the sensor curve)
const float soilVoltagePerPercent = 0.08 * 3.3 / 5.0; // volts per 1 % of soil humidity (slope of the sensor curve)

// Watering thresholds
const float soilDryLimit = 30; // soil humidity (percent) below which the pump starts
const float soilWetLimit = 70; // soil humidity (percent) above which the pump stops
const unsigned long maxPumpRunTime = 30000; // safety timeout in ms, the pump is always stopped after this time
const unsigned long pumpPauseTime = 60000; // ms the pump stays off after a safety stop so the water can soak in

// Timing
const unsigned long wifiConnectTimeout = 15000; // Time in ms that setup() waits for the Wi-Fi connection
const unsigned long wifiRetryInterval = 10000; // Time in ms between reconnection attempts in loop()
const unsigned long readInterval = 1000; // Time in ms between sensor readings and control cycles
const unsigned long alertRetryInterval = 30000; // Time in ms to wait after a failed send before trying again

// History buffer
// A build flag can shorten the interval so the simulator fills the buffer faster.
#ifndef HISTORY_INTERVAL_MS
#define HISTORY_INTERVAL_MS 60000UL // Time in ms between two history samples (one per minute)
#endif
const uint16_t historyCapacity = 180; // Number of samples kept in RAM (3 hours at one sample per minute)
const unsigned long historyIntervalMs = HISTORY_INTERVAL_MS; // Time in ms between two history samples

#endif
