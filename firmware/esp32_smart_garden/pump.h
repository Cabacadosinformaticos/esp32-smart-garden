// Pump state machine: decides when the relay is on and it is the only code that
// writes the relay pin. It supports an automatic mode driven by the soil and a
// manual mode with timed runs, while the safety limits stay always active.

#ifndef PUMP_H
#define PUMP_H

#include <Arduino.h> // Needed for uint16_t and bool
#include "readings.h" // Needed for the Readings struct used by pumpUpdate

enum PumpMode { PUMP_AUTO, PUMP_MANUAL };

struct PumpStatus {
  bool running;                      // true while the pump is on
  PumpMode mode;                     // current mode, automatic or manual
  uint16_t manualRemainingSeconds;   // seconds left in a manual run, 0 when none
  uint16_t blockedRemainingSeconds;  // seconds of pause after a safety stop, 0 when none
  const char* reason;                // short text of the last state change
};

void pumpBegin();                                  // relay pin as output, LOW
void pumpUpdate(const Readings& r);                // call once per control cycle, writes the relay
void pumpSetMode(PumpMode mode);                   // switching to manual stops an automatic run
bool pumpStartManual(uint16_t seconds, const char** error); // false + error text when refused
void pumpStop();                                   // stops any run immediately
PumpStatus pumpStatus();
bool pumpTakeSafetyStopEvent();                    // true once after the max run time stopped an AUTO run

#endif
