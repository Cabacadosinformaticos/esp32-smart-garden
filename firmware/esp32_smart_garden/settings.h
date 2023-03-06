// Runtime settings stored in flash: the watering and alert thresholds are loaded
// at boot, validated and kept in RAM, with the defaults from config.h. The web
// page can change them and save them again.

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h> // Needed for uint8_t, uint16_t and bool

struct Settings {
  float soilDry;     // default 30, pump starts below
  float soilWet;     // default 70, pump stops above
  float tempMin;     // default 18
  float tempMax;     // default 26
  float humMin;      // default 50
  float humMax;      // default 70
  uint16_t maxPumpSeconds;  // default 30, range 5..120
  uint16_t pauseSeconds;    // default 60, range 10..600
  bool dailySummary;        // default false
  uint8_t summaryHour;      // default 20, range 0..23
};

extern Settings settings;                 // defined in settings.cpp
void settingsBegin();                     // load from Preferences namespace "garden", defaults if absent or invalid
const char* settingsCheck(const Settings& s); // nullptr when valid, else a short error text
bool settingsSave();                      // writes settings to flash (only changed keys), false on failure
void settingsReset();                     // defaults + save

#endif
