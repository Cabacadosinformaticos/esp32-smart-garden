// Implements the mDNS name, the over the air updates and the NTP clock of the
// smart garden. The services start only once the Wi-Fi is connected, because the
// board may boot without a network and connect later from loop().

#include <WiFi.h>      // Needed for WiFi.status() to know when the services can start
#include <ESPmDNS.h>   // Needed for MDNS, the responder that publishes the hostname
#include <ArduinoOTA.h> // Needed for ArduinoOTA, ota_error_t and the OTA error values
#include <time.h>      // Needed for struct tm and getLocalTime

#include "services.h" // Includes the prototypes of the functions defined below
#include "config.h"   // Includes deviceHostname and timezoneRule
#include "secrets.h"  // Includes OTA_PASSWORD
#include "pump.h"     // Includes pumpStop(), called when an over the air update starts

static bool servicesStarted = false; // true after mDNS, ArduinoOTA and SNTP were started once
static bool wiFiWasConnected = false; // Wi-Fi state at the end of the previous loop() call
static int lastProgressPercent = -10; // Last OTA percent printed, -10 so 0 is printed once

// Prints the local address where the dashboard is reachable once mDNS is answering
static void printMdnsAddress() {
  Serial.print("Dashboard reachable at http://");
  Serial.print(deviceHostname);
  Serial.println(".local");
}

// Starts the mDNS responder with the device hostname and advertises the web
// server on port 80. Returns false when the responder did not start.
static bool startMdns() {
  if (!MDNS.begin(deviceHostname)) {
    Serial.println("mDNS start failed");
    return false;
  }
  MDNS.addService("http", "tcp", 80); // Advertises the web server so the name opens the page
  printMdnsAddress();
  return true;
}

// Starts mDNS, ArduinoOTA and SNTP, called once on the first connected loop()
static void startServices() {
  startMdns();

  // ArduinoOTA asks for OTA_PASSWORD before it accepts a new firmware
  ArduinoOTA.setHostname(deviceHostname);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // The pump stops as soon as an update starts, so the relay stays off
  ArduinoOTA.onStart([]() {
    lastProgressPercent = -10; // Restarts the progress scale for this update
    Serial.println("OTA update started");
    pumpStop();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished");
  });
  // One line every 10 percent at most, to keep the serial output short
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = total > 0 ? (int)((progress * 100) / total) : 0;
    if (percent >= lastProgressPercent + 10) {
      lastProgressPercent = percent;
      Serial.printf("OTA progress: %d%%\n", percent);
    }
  });
  // Prints the name of the error that aborted the update
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA error: ");
    if (error == OTA_AUTH_ERROR) {
      Serial.println("wrong password");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("begin failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("connection failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("receive failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("end failed");
    } else {
      Serial.println("unknown");
    }
  });
  ArduinoOTA.begin();
  Serial.print("Over the air updates ready, hostname ");
  Serial.println(deviceHostname);

  // Starts SNTP, the clock is needed by the daily summary
  configTzTime(timezoneRule, "pool.ntp.org", "time.google.com");
  Serial.println("NTP time configured");
}

// Reports that the services are ready, the real start happens in servicesLoop()
void servicesBegin() {
  servicesStarted = false;
  wiFiWasConnected = false;
  lastProgressPercent = -10;
  Serial.println("Network services waiting for the Wi-Fi connection");
}

// Starts the services once on the first connection and keeps ArduinoOTA running
void servicesLoop() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  // First working connection: mDNS, ArduinoOTA and SNTP start here and only once
  if (connected && !servicesStarted) {
    startServices();
    servicesStarted = true;
  }
  // The Wi-Fi was lost and came back, so only mDNS has to be started again.
  // ArduinoOTA and SNTP keep running on their own and need no restart.
  else if (connected && !wiFiWasConnected) {
    Serial.println("Wi-Fi is back, restarting mDNS");
    MDNS.end(); // Frees the sockets of the old connection before starting again
    startMdns();
  }

  wiFiWasConnected = connected;

  // Cheap and non blocking, it only does work while an update is being received
  if (servicesStarted) {
    ArduinoOTA.handle();
  }
}

// Returns true when the clock was set by NTP, the check never waits
bool timeIsSynced() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 0)) {
    return false; // No time yet, the zero wait keeps the caller non blocking
  }
  return timeInfo.tm_year >= 123; // 2023 or later, tm_year counts the years from 1900
}
