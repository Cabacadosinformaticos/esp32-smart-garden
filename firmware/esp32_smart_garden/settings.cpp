// Implements the runtime settings declared in settings.h. The values live in the
// flash key/value store of the Preferences library, namespace "garden", and every
// key is loaded at boot with the default from config.h when it is absent.

#include "settings.h" // Includes the Settings struct and the settings declarations
#include "config.h" // Includes the default threshold constants
#include <Preferences.h> // Needed for the flash key/value store
#include <math.h> // Needed for isnan

// Namespace and keys of the flash store (a key is at most 15 characters)
static const char* settingsNamespace = "garden";
static const char* keySoilDry = "soilDry";
static const char* keySoilWet = "soilWet";
static const char* keyTempMin = "tempMin";
static const char* keyTempMax = "tempMax";
static const char* keyHumMin = "humMin";
static const char* keyHumMax = "humMax";
static const char* keyMaxPump = "maxPump";
static const char* keyPause = "pause";
static const char* keySummary = "summary";
static const char* keySumHour = "sumHour";

Settings settings; // Runtime settings, shared by the sketch and the web page

// Copies the defaults from config.h into the settings, without touching flash
static void applyDefaults() {
  settings.soilDry = defaultSoilDry;
  settings.soilWet = defaultSoilWet;
  settings.tempMin = defaultTempMin;
  settings.tempMax = defaultTempMax;
  settings.humMin = defaultHumMin;
  settings.humMax = defaultHumMax;
  settings.maxPumpSeconds = defaultMaxPumpSeconds;
  settings.pauseSeconds = defaultPauseSeconds;
  settings.dailySummary = defaultDailySummary;
  settings.summaryHour = defaultSummaryHour;
}

// Writes a float only when the stored value changed, to spare flash write cycles
// Returns false only when the write fails
static bool saveFloatChanged(Preferences& prefs, const char* key, float defaultValue, float value) {
  if (prefs.getFloat(key, defaultValue) == value) {
    return true;
  }
  return prefs.putFloat(key, value) != 0;
}

// Writes a uint16_t only when the stored value changed, to spare flash write cycles
static bool saveUShortChanged(Preferences& prefs, const char* key, uint16_t defaultValue, uint16_t value) {
  if (prefs.getUShort(key, defaultValue) == value) {
    return true;
  }
  return prefs.putUShort(key, value) != 0;
}

// Writes a bool only when the stored value changed, to spare flash write cycles
static bool saveBoolChanged(Preferences& prefs, const char* key, bool defaultValue, bool value) {
  if (prefs.getBool(key, defaultValue) == value) {
    return true;
  }
  return prefs.putBool(key, value) != 0;
}

// Writes a uint8_t only when the stored value changed, to spare flash write cycles
static bool saveUCharChanged(Preferences& prefs, const char* key, uint8_t defaultValue, uint8_t value) {
  if (prefs.getUChar(key, defaultValue) == value) {
    return true;
  }
  return prefs.putUChar(key, value) != 0;
}

// Loads every setting from flash and validates the result. A missing key takes its
// default and an invalid group is replaced by all the defaults.
void settingsBegin() {
  Preferences prefs;
  if (!prefs.begin(settingsNamespace, false)) {
    applyDefaults();
    Serial.println("Settings storage not available, using defaults");
    return;
  }

  settings.soilDry = prefs.getFloat(keySoilDry, defaultSoilDry);
  settings.soilWet = prefs.getFloat(keySoilWet, defaultSoilWet);
  settings.tempMin = prefs.getFloat(keyTempMin, defaultTempMin);
  settings.tempMax = prefs.getFloat(keyTempMax, defaultTempMax);
  settings.humMin = prefs.getFloat(keyHumMin, defaultHumMin);
  settings.humMax = prefs.getFloat(keyHumMax, defaultHumMax);
  settings.maxPumpSeconds = prefs.getUShort(keyMaxPump, defaultMaxPumpSeconds);
  settings.pauseSeconds = prefs.getUShort(keyPause, defaultPauseSeconds);
  settings.dailySummary = prefs.getBool(keySummary, defaultDailySummary);
  settings.summaryHour = prefs.getUChar(keySumHour, defaultSummaryHour);
  prefs.end();

  const char* error = settingsCheck(settings);
  if (error != nullptr) {
    Serial.print("Stored settings are invalid (");
    Serial.print(error);
    Serial.println("), using defaults");
    applyDefaults();
  }
}

// Checks every setting and returns nullptr when they are all valid,
// else a short error text that names the API fields
const char* settingsCheck(const Settings& s) {

  // A NaN never compares true, so it is rejected before the range checks
  if (isnan(s.soilDry) || isnan(s.soilWet)) {
    return "soil_dry and soil_wet must be numbers";
  }
  if (isnan(s.tempMin) || isnan(s.tempMax)) {
    return "temp_min and temp_max must be numbers";
  }
  if (isnan(s.humMin) || isnan(s.humMax)) {
    return "hum_min and hum_max must be numbers";
  }

  if (s.soilDry < 0 || s.soilDry > 100 || s.soilWet < 0 || s.soilWet > 100) {
    return "soil_dry and soil_wet must be between 0 and 100";
  }
  if (s.soilDry + 5 > s.soilWet) {
    return "soil_dry must be at least 5 below soil_wet";
  }

  if (s.tempMin < -10 || s.tempMin > 60 || s.tempMax < -10 || s.tempMax > 60) {
    return "temp_min and temp_max must be between -10 and 60";
  }
  if (s.tempMin + 2 > s.tempMax) {
    return "temp_min must be at least 2 below temp_max";
  }

  if (s.humMin < 0 || s.humMin > 100 || s.humMax < 0 || s.humMax > 100) {
    return "hum_min and hum_max must be between 0 and 100";
  }
  if (s.humMin + 5 > s.humMax) {
    return "hum_min must be at least 5 below hum_max";
  }

  if (s.maxPumpSeconds < 5 || s.maxPumpSeconds > 120) {
    return "max_pump_s must be between 5 and 120";
  }
  if (s.pauseSeconds < 10 || s.pauseSeconds > 600) {
    return "pause_s must be between 10 and 600";
  }
  if (s.summaryHour > 23) {
    return "summary_hour must be between 0 and 23";
  }

  return nullptr;
}

// Writes only the settings that changed since the last load or save
// Returns false when any write fails
bool settingsSave() {
  Preferences prefs;
  if (!prefs.begin(settingsNamespace, false)) {
    return false;
  }

  bool ok = true;

  if (!saveFloatChanged(prefs, keySoilDry, defaultSoilDry, settings.soilDry)) {
    ok = false;
  }
  if (!saveFloatChanged(prefs, keySoilWet, defaultSoilWet, settings.soilWet)) {
    ok = false;
  }
  if (!saveFloatChanged(prefs, keyTempMin, defaultTempMin, settings.tempMin)) {
    ok = false;
  }
  if (!saveFloatChanged(prefs, keyTempMax, defaultTempMax, settings.tempMax)) {
    ok = false;
  }
  if (!saveFloatChanged(prefs, keyHumMin, defaultHumMin, settings.humMin)) {
    ok = false;
  }
  if (!saveFloatChanged(prefs, keyHumMax, defaultHumMax, settings.humMax)) {
    ok = false;
  }
  if (!saveUShortChanged(prefs, keyMaxPump, defaultMaxPumpSeconds, settings.maxPumpSeconds)) {
    ok = false;
  }
  if (!saveUShortChanged(prefs, keyPause, defaultPauseSeconds, settings.pauseSeconds)) {
    ok = false;
  }
  if (!saveBoolChanged(prefs, keySummary, defaultDailySummary, settings.dailySummary)) {
    ok = false;
  }
  if (!saveUCharChanged(prefs, keySumHour, defaultSummaryHour, settings.summaryHour)) {
    ok = false;
  }

  prefs.end();
  return ok;
}

// Restores the defaults in RAM and writes them to flash
void settingsReset() {
  applyDefaults();
  settingsSave();
}
