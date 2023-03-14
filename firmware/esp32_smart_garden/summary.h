// Daily statistics and the optional daily WhatsApp summary. summaryRecord()
// collects the values of every control cycle and summaryLoop() sends one
// message per day, at or after the configured hour.

#ifndef SUMMARY_H
#define SUMMARY_H

#include "readings.h" // Includes the Readings struct passed to summaryRecord()

void summaryRecord(const Readings& r, bool pumpRunning); // Updates the statistics, call once per control cycle
void summaryLoop(); // Sends the daily summary when it is due, call once per loop()

#endif
