// Groups the last sensor readings in one place, so the control cycle and the
// web page share a single copy of the values read by readSensors().

#ifndef READINGS_H
#define READINGS_H

struct Readings {
  float soil;          // soil humidity, percent (can be outside 0..100 when the sensor is broken)
  bool soilValid;      // soil in 0..100
  float temperature;   // degrees C, NaN when dhtValid is false
  float humidity;      // air humidity percent, NaN when dhtValid is false
  bool dhtValid;
  float light;         // percent 0..100
  bool tankEmpty;      // true when the liquid sensor reads LOW
};
extern Readings readings;

#endif
