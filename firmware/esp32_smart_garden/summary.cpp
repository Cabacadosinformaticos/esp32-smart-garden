// Implements the daily statistics and the optional daily WhatsApp summary.
// The values are kept in static variables, so no heap memory is used, and the
// message is built with snprintf into a fixed buffer.

#include <Arduino.h> // Needed for millis(), String, snprintf and the fixed width types
#include <stdio.h>   // Needed for snprintf
#include <time.h>    // Needed for struct tm and getLocalTime()

#include "summary.h"  // Includes the prototypes of the functions defined below
#include "settings.h" // Includes settings.dailySummary and settings.summaryHour
#include "services.h" // Includes timeIsSynced()
#include "alerts.h"   // Includes sendAlert(), defined in the sketch

// Statistics since the last summary or the boot
static float tempMin = 0;        // lowest valid temperature, only used while tempCount > 0
static float tempMax = 0;        // highest valid temperature, only used while tempCount > 0
static float tempSum = 0;        // sum of the valid temperatures, for the average
static uint16_t tempCount = 0;   // number of samples with dhtValid
static float soilSum = 0;        // sum of the valid soil humidity values
static uint16_t soilCount = 0;   // number of samples with soilValid
static unsigned long pumpMillis = 0;  // time the pump ran, in milliseconds
static unsigned long sampleCount = 0; // number of control cycles recorded
static bool tankWasEmpty = false;     // true when the tank was empty at least once
static bool haveLastRecord = false;   // false until summaryRecord() ran once
static unsigned long lastRecordMillis = 0; // Time of the previous summaryRecord() call

// Daily summary timing
static unsigned long lastCheckMillis = 0; // Time of the last check in summaryLoop()
static long lastSummaryDay = -1;          // Day id of the last sent summary, -1 means none yet

// Clears the statistics so the next summary covers only the following period
static void resetStatistics() {
  tempMin = 0;
  tempMax = 0;
  tempSum = 0;
  tempCount = 0;
  soilSum = 0;
  soilCount = 0;
  pumpMillis = 0;
  sampleCount = 0;
  tankWasEmpty = false;
}

// Adds one control cycle to the daily statistics
void summaryRecord(const Readings& r, bool pumpRunning) {
  unsigned long now = millis();

  // The pump run time comes from the time between two calls, but a gap larger
  // than 5 s means loop() was blocked and is not counted as pumping
  if (haveLastRecord && pumpRunning && now - lastRecordMillis <= 5000UL) {
    pumpMillis += now - lastRecordMillis;
  }
  lastRecordMillis = now;
  haveLastRecord = true;

  sampleCount++;

  // Only the samples where the DHT answered go into the temperature statistics
  if (r.dhtValid) {
    if (tempCount == 0 || r.temperature < tempMin) {
      tempMin = r.temperature;
    }
    if (tempCount == 0 || r.temperature > tempMax) {
      tempMax = r.temperature;
    }
    tempSum += r.temperature;
    tempCount++;
  }

  // Only the samples inside 0..100 % go into the soil humidity average
  if (r.soilValid) {
    soilSum += r.soil;
    soilCount++;
  }

  if (r.tankEmpty) {
    tankWasEmpty = true;
  }
}

// Checks the clock and sends one WhatsApp summary per day when it is enabled
void summaryLoop() {
  unsigned long now = millis();

  // The whole check runs at most every 30 seconds, so loop() stays fast
  if (now - lastCheckMillis < 30000UL) {
    return;
  }
  lastCheckMillis = now;

  if (!settings.dailySummary || !timeIsSynced()) {
    return;
  }

  struct tm info;
  if (!getLocalTime(&info, 0)) {
    return;
  }

  // The day id is the year and the day of the year, so a new day changes it
  long dayId = (long)info.tm_year * 1000L + info.tm_yday;

  if (info.tm_hour < settings.summaryHour || dayId == lastSummaryDay) {
    return;
  }

  // The temperature text is "n/a" when the DHT never answered
  char temperatureText[64];
  if (tempCount > 0) {
    snprintf(temperatureText, sizeof(temperatureText), "temperature %.1f to %.1f C (avg %.1f)",
             (double)tempMin, (double)tempMax, (double)(tempSum / tempCount));
  } else {
    snprintf(temperatureText, sizeof(temperatureText), "temperature n/a");
  }

  // The soil text is "n/a" when no valid soil sample was recorded
  char soilText[48];
  if (soilCount > 0) {
    snprintf(soilText, sizeof(soilText), "soil humidity avg %.1f %%",
             (double)(soilSum / soilCount));
  } else {
    snprintf(soilText, sizeof(soilText), "soil humidity avg n/a");
  }

  char buffer[200];
  snprintf(buffer, sizeof(buffer), "Garden summary: %s, %s, pump ran %lu s, %s.",
           temperatureText, soilText, (unsigned long)(pumpMillis / 1000UL),
           tankWasEmpty ? "tank was empty today" : "tank OK");

  // The day is remembered and the statistics are cleared only after a real send,
  // otherwise the summary is tried again at the next check
  if (sendAlert(String(buffer))) {
    lastSummaryDay = dayId;
    resetStatistics();
  }
}
