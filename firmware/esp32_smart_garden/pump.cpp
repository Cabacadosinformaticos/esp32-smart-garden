// Implements the pump state machine declared in pump.h. The relay is written only
// here. The tank empty and the maximum run time are always safety limits; the soil
// sensor drives the automatic mode and an invalid reading stops an automatic run.

#include "pump.h" // Includes the PumpStatus struct and the pump declarations
#include "config.h" // Includes the relay pin
#include "settings.h" // Includes the runtime thresholds
#include <stdio.h> // Needed for snprintf

// Pump state kept between control cycles
static bool pumpRunning = false; // True while the water pump is running
static bool manualRun = false; // True when the current run was started by hand
static PumpMode pumpMode = PUMP_AUTO; // Current mode, automatic by default
static unsigned long pumpStartTime = 0; // Time when the current run started
static unsigned long pumpBlockedUntil = 0; // Time until the pump has to stay off after a safety stop
static uint16_t manualRunSeconds = 0; // Duration requested for the current manual run
static bool tankEmpty = false; // Last tank state, used to refuse a manual start
static bool safetyStopEvent = false; // True once after the max run time stopped an automatic run
static const char* pumpReason = "idle"; // Short text of the last state change
static char manualErrorText[64]; // Buffer for the manual start error that holds the real maximum

// Turns the relay off and clears the running state
static void stopRun(const char* reason) {
  digitalWrite(relayPin, LOW); // Turns the relay off
  pumpRunning = false;
  manualRun = false;
  manualRunSeconds = 0;
  pumpReason = reason;
}

// Sets the relay pin as output and keeps the pump off
void pumpBegin() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
}

// Call once per control cycle: checks every stop reason and the automatic start
void pumpUpdate(const Readings& r) {
  tankEmpty = r.tankEmpty; // Remembers the tank state for pumpStartManual

  if (!pumpRunning) {
    digitalWrite(relayPin, LOW); // Keeps the relay off while the pump is not working

    // Starts the pump only in auto mode, when the soil is dry, the tank has water
    // and the pause after a safety stop is over
    if (pumpMode == PUMP_AUTO && r.soilValid && r.soil < settings.soilDry && !r.tankEmpty &&
        (long)(millis() - pumpBlockedUntil) >= 0) {
      digitalWrite(relayPin, HIGH); // Turns the relay on
      pumpRunning = true;
      manualRun = false;
      pumpStartTime = millis();
      pumpReason = "soil dry";
      Serial.println("Watering the plant...");
    }
    return;
  }

  // The tank is always a safety limit, also during a manual run
  if (r.tankEmpty) {
    stopRun("tank empty");
    Serial.println("Stopping the pump: the water tank is empty.");
    return;
  }

  // An invalid soil reading stops an automatic run; in a manual run the user decides
  if (!manualRun && !r.soilValid) {
    stopRun("soil sensor invalid");
    Serial.println("Stopping the pump: the soil humidity reading is not valid.");
    return;
  }

  // An automatic run stops when the soil is wet enough
  if (!manualRun && r.soil > settings.soilWet) {
    stopRun("soil wet enough");
    Serial.println("Stopping the pump: the soil is wet enough.");
    return;
  }

  // A manual run stops when its requested time is over
  if (manualRun && (long)(millis() - pumpStartTime) >= (long)manualRunSeconds * 1000L) {
    stopRun("manual run finished");
    Serial.println("Stopping the pump: the manual run finished.");
    return;
  }

  // The maximum run time is always a safety limit, also during a manual run
  if ((long)(millis() - pumpStartTime) >= (long)settings.maxPumpSeconds * 1000L) {
    bool automatic = !manualRun; // The safety stop event is raised only for automatic runs
    stopRun("max run time");
    Serial.println("Stopping the pump: the maximum run time was reached.");
    if (automatic) {
      pumpBlockedUntil = millis() + settings.pauseSeconds * 1000UL; // Leaves the water time to soak in
      safetyStopEvent = true; // The sketch sends the safety timeout alert once
    }
    return;
  }
}

// Changes the mode. Switching to manual stops an automatic run, so the plant is
// not watered without the user asking for it when the manual controls are used.
void pumpSetMode(PumpMode mode) {
  if (mode == PUMP_MANUAL && pumpRunning && !manualRun) {
    stopRun("stopped");
    Serial.println("Stopping the pump: the mode changed to manual.");
  }
  pumpMode = mode;
}

// Starts a timed manual run. Returns false and an error text when it is refused:
// an empty tank, a time below one second or a time above the safety maximum.
bool pumpStartManual(uint16_t seconds, const char** error) {
  if (error != nullptr) {
    *error = nullptr;
  }

  if (tankEmpty) {
    if (error != nullptr) {
      *error = "tank is empty";
    }
    return false;
  }

  if (seconds == 0) {
    if (error != nullptr) {
      *error = "seconds must be at least 1";
    }
    return false;
  }

  if (seconds > settings.maxPumpSeconds) {
    snprintf(manualErrorText, sizeof(manualErrorText), "run time above the maximum of %u s", (unsigned)settings.maxPumpSeconds);
    if (error != nullptr) {
      *error = manualErrorText;
    }
    return false;
  }

  // A manual run is allowed in both modes and replaces any run in progress
  digitalWrite(relayPin, HIGH); // Turns the relay on
  pumpRunning = true;
  manualRun = true;
  manualRunSeconds = seconds;
  pumpStartTime = millis();
  pumpReason = "manual run";
  Serial.print("Manual run, watering the plant for ");
  Serial.print(seconds);
  Serial.println(" s");
  return true;
}

// Stops any run immediately, automatic or manual
void pumpStop() {
  if (!pumpRunning) {
    return;
  }
  stopRun("stopped");
  Serial.println("Stopping the pump: stopped by the user.");
}

// Returns the current pump state, with the remaining seconds computed from millis
PumpStatus pumpStatus() {
  PumpStatus status;
  status.running = pumpRunning;
  status.mode = pumpMode;
  status.reason = pumpReason;

  status.manualRemainingSeconds = 0;
  if (pumpRunning && manualRun) {
    unsigned long total = (unsigned long)manualRunSeconds * 1000UL;
    unsigned long elapsed = millis() - pumpStartTime;
    if (elapsed < total) {
      status.manualRemainingSeconds = (uint16_t)(((total - elapsed) + 999UL) / 1000UL); // Rounds up
    }
  }

  status.blockedRemainingSeconds = 0;
  if ((long)(pumpBlockedUntil - millis()) > 0) {
    unsigned long remaining = pumpBlockedUntil - millis();
    status.blockedRemainingSeconds = (uint16_t)((remaining + 999UL) / 1000UL); // Rounds up
  }

  return status;
}

// Returns true only once after the maximum run time stopped an automatic run
bool pumpTakeSafetyStopEvent() {
  bool raised = safetyStopEvent;
  safetyStopEvent = false;
  return raised;
}
