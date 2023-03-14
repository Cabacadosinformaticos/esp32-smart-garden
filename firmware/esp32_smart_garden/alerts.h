// Declares the alert helper that lives in the sketch, so the extra tabs can
// send a WhatsApp message through it without knowing the CallMeBot details.

#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h> // Needed for String and bool

bool sendAlert(String message); // Sends an alert, respects the retry delay, true only when sent

#endif
