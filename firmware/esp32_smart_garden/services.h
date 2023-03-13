// Network services of the smart garden: the mDNS name, the over the air updates
// and the NTP clock. They can only start when the Wi-Fi is connected, so they
// are started lazily from servicesLoop() and not in setup().

#ifndef SERVICES_H
#define SERVICES_H

#include <Arduino.h> // Needed for bool

void servicesBegin(); // Prints a line and resets the state, the services start from loop()
void servicesLoop();  // Starts the services once and keeps them running, call it from loop()
bool timeIsSynced();  // true when the clock was set by NTP, zero wait and non blocking

#endif
