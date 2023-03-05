// Implements the in-RAM history ring buffer declared in history.h.
// The samples live in a fixed static array, so no heap memory is used.

#include "history.h" // Includes HistorySample, historyCapacity and the Readings struct

// Flags stored in HistorySample::flags
#define HISTORY_FLAG_PUMP 0x01 // bit0: pump running
#define HISTORY_FLAG_DHT 0x02  // bit1: DHT valid
#define HISTORY_FLAG_SOIL 0x04 // bit2: soil valid

// 180 samples x 8 bytes = 1440 bytes of static RAM (3 hours at one sample per minute)
static HistorySample samples[historyCapacity];
static uint16_t head = 0;  // Index where the next sample is written
static uint16_t count = 0; // Number of valid samples, 0..historyCapacity

// Multiplies a finite value by 10 and rounds it to the nearest integer
static int16_t scaled10(float value) {
  float scaled = value * 10.0f;
  return (int16_t)(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
}

// Multiplies a value by 10, rounds it and clamps the result to low..high
static int16_t scaled10Clamped(float value, int16_t low, int16_t high) {
  float scaled = value * 10.0f;
  if (scaled < (float)low) {
    return low;
  }
  if (scaled > (float)high) {
    return high;
  }
  return (int16_t)(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
}

// Rounds a percent value to a byte and clamps it to 0..100
static uint8_t lightToByte(float value) {
  float rounded = value + 0.5f;
  if (rounded < 0.0f) {
    return 0;
  }
  if (rounded > 100.0f) {
    return 100;
  }
  return (uint8_t)rounded;
}

// Adds one sample to the buffer, overwriting the oldest one when it is full
void historyAdd(const Readings& r, bool pumpRunning) {
  HistorySample sample;

  // The DHT values are stored as 0 when the sensor did not answer,
  // because a NaN cannot be converted to an integer
  if (r.dhtValid) {
    sample.temp10 = scaled10(r.temperature);
    sample.hum10 = scaled10(r.humidity);
  } else {
    sample.temp10 = 0;
    sample.hum10 = 0;
  }

  sample.soil10 = scaled10Clamped(r.soil, -1000, 2000);
  sample.light = lightToByte(r.light);

  sample.flags = 0;
  if (pumpRunning) {
    sample.flags |= HISTORY_FLAG_PUMP;
  }
  if (r.dhtValid) {
    sample.flags |= HISTORY_FLAG_DHT;
  }
  if (r.soilValid) {
    sample.flags |= HISTORY_FLAG_SOIL;
  }

  samples[head] = sample; // Overwrites the oldest sample when the buffer is full
  head = (head + 1) % historyCapacity;
  if (count < historyCapacity) {
    count++;
  }
}

// Returns how many samples are stored, 0..historyCapacity
uint16_t historyCount() {
  return count;
}

// Copies one sample, 0 = oldest and count-1 = newest
// Returns false when the index is outside the stored range
bool historyGet(uint16_t index, HistorySample& out) {
  if (index >= count) {
    return false;
  }
  uint16_t oldest = (head + historyCapacity - count) % historyCapacity;
  out = samples[(oldest + index) % historyCapacity];
  return true;
}
