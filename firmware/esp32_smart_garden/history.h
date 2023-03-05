// In-RAM history ring buffer with the last readings, used by the charts of the
// web page. One sample per minute and 180 samples cover the last three hours.

#ifndef HISTORY_H
#define HISTORY_H

#include <Arduino.h> // Needed for uint8_t and uint16_t
#include "config.h" // Needed for historyCapacity and historyIntervalMs
#include "readings.h" // Needed for the Readings struct

struct HistorySample {   // 8 bytes
  int16_t temp10;        // temperature * 10
  int16_t hum10;         // air humidity * 10
  int16_t soil10;        // soil percent * 10, clamped to -1000..2000
  uint8_t light;         // percent 0..100
  uint8_t flags;         // bit0 pump running, bit1 dht valid, bit2 soil valid
};

void historyAdd(const Readings& r, bool pumpRunning);
uint16_t historyCount();                         // 0..historyCapacity
bool historyGet(uint16_t index, HistorySample& out); // 0 = oldest, count-1 = newest

#endif
