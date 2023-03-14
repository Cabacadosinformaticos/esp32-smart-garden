// Smart garden (Horta IoT)
// Sketch for an ESP32 board. It reads the DHT11 air temperature and humidity sensor,
// the light sensor, the soil moisture sensor and the water tank level sensor, waters the
// plant with a relay and a pump, sends WhatsApp alerts through CallMeBot and serves a web
// page with the last readings.
// The Wi-Fi and CallMeBot settings live in secrets.h (copy secrets.example.h).
// Build with -DWOKWI_SIMULATION for the Wokwi simulator, see the README.

#include "readings.h" // Includes the Readings struct that groups the last sensor readings
#include "history.h" // Includes the in-RAM history ring buffer used by the charts
#include "settings.h" // Includes the runtime thresholds loaded from flash
#include "pump.h" // Includes the pump state machine, the only code that writes the relay
#include "web.h" // Includes the web server and the JSON API of the dashboard
#include "services.h" // Includes mDNS, the over the air updates and the NTP clock
#include "summary.h" // Includes the daily statistics and the daily WhatsApp summary
#include "alerts.h" // Includes the declaration of sendAlert(), shared with the other tabs

// Global state flags and timers used by loop()
bool notWorkSent = false; // true after the soil sensor alert was sent, so it is sent only once
bool tankEmptySent = false; // true after the empty tank alert was sent, so it is sent only once
bool pumpTimeoutSent = false; // True after the safety timeout alert was sent
bool pumpTimeoutPending = false; // True while the safety timeout alert still has to be sent
bool lowTempSent = false; // true after the low temperature alert was sent, so it is sent only once
bool highTempSent = false; // true after the high temperature alert was sent, so it is sent only once
bool lowHumiditySent = false; // true after the low air humidity alert was sent, so it is sent only once
bool highHumiditySent = false; // true after the high air humidity alert was sent, so it is sent only once
bool dhtErrorSent = false; // true after the DHT error alert was sent, so it is sent only once
unsigned long lastWifiAttempt = 0; // Time of the last Wi-Fi connection attempt, used to retry in loop()
unsigned long lastReadTime = 0; // Time of the last sensor reading and control cycle, used to time loop()
unsigned long nextAlertTime = 0; // Time before which no alert is attempted again, set after a failed send
unsigned long lastAlertAttempt = 0; // Time of the last send attempt, used to keep two sends apart
unsigned long lastHistoryTime = 0; // Time of the last sample stored in the history buffer
bool historyStarted = false; // False until the first sample is stored, so the charts start with a point

// Last readings, shared by loop() and the web page so the sensors are read only once per cycle
Readings readings = {0, false, NAN, NAN, false, 0, false}; // Soil percent, soil valid, temperature, humidity, DHT valid, light percent, tank empty

#include <WiFi.h> // Includes the WiFi library in the program to allow connection to Wi-Fi networks
#include <HTTPClient.h> // Includes the HTTPClient library in the program to make HTTP requests to a server
#include <UrlEncode.h> // Includes the UrlEncode library in the program to encode URLs to be sent as parameters in HTTP requests
#include <DHT.h> // Includes the DHT library in the program to allow the use of the humidity and temperature sensor
#include "secrets.h" // Includes the Wi-Fi, phone number and API key defined in the secrets.h file
#include "config.h" // Includes the pins, calibration, threshold and timing constants defined in the config.h file
DHT dht(dhtSensorPin, DHTTYPE); // Creates a DHT library instance with the DHT sensor pin and the type from config.h


const char* ssid = NET_SSID; // Defines the name of the Wi-Fi network (SSID) the device will connect to
const char* password = NET_PASSWORD; // Defines the password of the Wi-Fi network the device will connect to

// +international_country_code + phone number
// Portugal +351, example: +351912345678
String phoneNumber = WHATSAPP_PHONE; // Defines the phone number that will receive the text messages (country code + phone number)
String apiKey = CALLMEBOT_API_KEY; // Defines the API key used to send text messages

// Tries to connect to the Wi-Fi network and gives up after wifiConnectTimeout
// Returns true when the connection is up
bool connectWiFi() {
  WiFi.mode(WIFI_STA); // Sets the Wi-Fi to station mode
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis(); // Time when this connection attempt started
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < wifiConnectTimeout) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Wi-Fi connection failed, will keep trying");
  return false;
}

void setup() {
  Serial.begin(9600); // Starts serial communication
  dht.begin(); // Starts the humidity and temperature sensor
  pinMode(waterSensorPin, INPUT); // Sets the contactless liquid sensor pin as input
  pinMode(lightSensorPin, INPUT); // Sets the KY-018 light sensor pin as input
  pinMode(soilSensorPin, INPUT); // Sets the soil humidity sensor pin as input
  pumpBegin(); // Sets the relay pin as output and keeps the pump off

  settingsBegin(); // Loads the thresholds from flash, with validation
  Serial.println("Settings loaded");

  // Connects to the Wi-Fi network, setup continues even if it fails
  connectWiFi();
  lastWifiAttempt = millis();

  // Prepares mDNS, the over the air updates and the NTP clock. They really start
  // in loop() once the Wi-Fi is connected, because the board may boot offline.
  servicesBegin();

  // Starts the web server and registers every route
  webBegin();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("IP address: not connected");
  }
}

// Function that sends a text message to the specified phone number
// Returns true only when the server answers with HTTP 200
bool sendMessage(String message){

  // In the simulator there is no real WhatsApp account, so the message is only printed
  // and the caller behaves as if the send worked
  if (!ALERTS_ENABLED) {
    Serial.print("Alert (not sent in the simulator): ");
    Serial.println(message);
    return true;
  }

  // The message can only be sent when the Wi-Fi connection is up
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi is not connected, message not sent");
    return false;
  }

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message); // Builds the URL with the information needed to send the text message through the CallMeBot API
  HTTPClient http; // Starts an HTTP GET request using the URL created above
  http.begin(url);
  http.setTimeout(5000); // Waits at most 5 seconds for the server answer

  int httpResponseCode = http.GET(); // Sends the HTTP GET request and stores the HTTP response code in a variable
  if (httpResponseCode == 200){ // Checks if the message was sent successfully and prints a message on the serial monitor
    Serial.println("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    if (httpResponseCode < 0) { // Negative codes mean the connection itself failed
      Serial.print("Connection error: ");
      Serial.println(HTTPClient::errorToString(httpResponseCode));
    }
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  http.end(); // Releases the resources used in the HTTP request

  return httpResponseCode == 200;
}

// Sends an alert message, but not faster than alertRetryInterval after a failure
// Returns true only when the message was sent
bool sendAlert(String message) {

  // A previous send failed, so the alert waits before trying again
  if ((long)(millis() - nextAlertTime) < 0) {
    return false;
  }

  // Two sends never start close together: every HTTPS call needs a TLS handshake that takes
  // time and a lot of heap, so a second alert waits for the next control cycle
  if (lastAlertAttempt != 0 && millis() - lastAlertAttempt < alertMinSpacing) {
    return false;
  }
  lastAlertAttempt = millis();

  bool sent = sendMessage(message);
  if (!sent) {
    nextAlertTime = millis() + alertRetryInterval;
  }
  return sent;
}

// Reads the soil humidity sensor, converts the raw value to voltage and then to a percentage
float readSoilHumidity() {
  int soilSensorValue = analogRead(soilSensorPin); // Reads the value of the soil humidity sensor
  float voltage = soilSensorValue * (adcReferenceVoltage / (float)adcMaxValue); // Calculates the voltage from the value read from the soil humidity sensor
  float soilPercent = (voltage - soilVoltageDry) / soilVoltagePerPercent; // Calculates the soil humidity and converts the value to a scale from 0 to 100
  return soilPercent;
}

// Reads every sensor once and stores the values in the global last readings
void readSensors() {
  readings.soil = readSoilHumidity(); // Reads the soil humidity as a percentage
  readings.soilValid = readings.soil >= 0 && readings.soil <= 100; // False when the sensor is broken or unplugged
  readings.tankEmpty = (digitalRead(waterSensorPin) == LOW); // True when the contactless liquid sensor reads LOW
  readings.humidity = dht.readHumidity(); // Reads the relative air humidity from the DHT humidity and temperature sensor
  readings.temperature = dht.readTemperature(); // Reads the temperature from the DHT humidity and temperature sensor
  int lightSensorValue = analogRead(lightSensorPin); // Reads the value of the light intensity sensor
  readings.light = map(lightSensorValue, 0, adcMaxValue, 100, 0); // Converts the value read from the light intensity sensor to a scale from 0 to 100

  // NaN means the DHT sensor did not answer, so the readings cannot be used
  readings.dhtValid = !isnan(readings.humidity) && !isnan(readings.temperature);
}

// Sends the WhatsApp alerts that belong to the pump: the empty tank warning and
// the safety timeout warning. Every alert is sent only once per event, and a failed
// send is retried later through sendAlert().
void checkPumpAlerts() {

  // The tank has water again, so a new empty tank alert can be sent later
  if (!readings.tankEmpty) {
    tankEmptySent = false;
  }

  // Warns when the soil is dry but there is no water in the tank
  if (readings.soilValid && readings.soil < settings.soilDry && readings.tankEmpty && !tankEmptySent) {
    Serial.println("The water tank is empty, the plant cannot be watered.");
    if (sendAlert("The water tank is empty, the plant cannot be watered.")) {
      tankEmptySent = true;
    }
  }

  // The soil is wet enough again, so a new safety timeout alert can be sent later
  if (readings.soilValid && readings.soil > settings.soilDry) {
    pumpTimeoutSent = false;
  }

  // A safety timeout stopped an automatic run, so an alert has to be sent
  if (pumpTakeSafetyStopEvent()) {
    pumpTimeoutPending = true;
  }

  // Keeps trying until the safety timeout alert is sent, then waits for the next event
  if (pumpTimeoutPending && !pumpTimeoutSent) {
    Serial.println("The pump was stopped by the safety timeout, please check the soil sensor and the tank.");
    if (sendAlert("The pump was stopped by the safety timeout, please check the soil sensor and the tank.")) {
      pumpTimeoutSent = true;
      pumpTimeoutPending = false;
    }
  }
}

void loop() {

  // Tries to reconnect when the Wi-Fi connection is lost, without waiting here
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttempt >= wifiRetryInterval) {
    lastWifiAttempt = millis();
    Serial.println("Wi-Fi lost, reconnecting");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  webLoop(); // Handles any client that is communicating with the server at that moment
  servicesLoop(); // Keeps mDNS, the over the air updates and the NTP clock running
  summaryLoop(); // Sends the daily WhatsApp summary when it is due, cheap when it is not

  // The readings and the control cycle run only once per readInterval.
  // loop() keeps running without delay(), so the web server stays responsive.
  if (millis() - lastReadTime < readInterval) {
    return;
  }
  lastReadTime = millis();

  readSensors(); // Reads all the sensors once and updates the global last readings

  Serial.println("--- Readings ---");
  // Checks if the value read by the soil humidity sensor is inside the valid range (0-100%)
  // If the value is outside the range, sends an error message and sets notWorkSent to true
  // Otherwise, shows the soil humidity on the serial monitor
  if (!readings.soilValid) {

    if (!notWorkSent) {

    Serial.println("Error: Humidity value outside the valid range (0-100%)");

    if (sendAlert("The soil humidity sensor is not working!")) {
      notWorkSent = true;
    }

    }

   } else if (readings.soilValid) {

    notWorkSent = false;

    // Shows the result on the serial monitor
    Serial.print("Soil humidity: ");
    Serial.print(readings.soil, 2);
    Serial.println("%");
  }

  pumpUpdate(readings); // Decides if the water pump must run or stop
  summaryRecord(readings, pumpStatus().running); // Adds this control cycle to the daily statistics
  checkPumpAlerts(); // Sends the empty tank and the safety timeout alerts

  // Stores one sample every historyIntervalMs, and also on the first control
  // cycle so the charts always have a starting point
  if (!historyStarted || millis() - lastHistoryTime >= historyIntervalMs) {
    historyAdd(readings, pumpStatus().running);
    lastHistoryTime = millis();
    historyStarted = true;
  }

  // Shows the values read by the sensors on the Serial monitor
  if (readings.dhtValid) {

    dhtErrorSent = false; // The sensor is working again

    Serial.print("Humidity and temperature sensor: ");
    Serial.print("Humidity = ");
    Serial.print(readings.humidity);
    Serial.print("%, Temperature = ");
    Serial.print(readings.temperature);
    Serial.println(" ºC");

  } else if (!dhtErrorSent) {

    Serial.println("Error: could not read the DHT11 sensor");

    if (sendAlert("The temperature and humidity sensor is not working!")) {
      dhtErrorSent = true;
    }

  }

  Serial.print("Light sensor: ");
  Serial.print("Light intensity = ");
  Serial.print(readings.light);
  Serial.println("%");

  // Checks if there is water in the tank and shows the result on the Serial monitor
  Serial.print("Contactless liquid sensor: ");
  if (readings.tankEmpty) {
    Serial.println("No liquid detected");
  } else {
    Serial.println("Liquid detected");
  }

  // Alerts to send if the plant is facing an adverse situation
  // All the temperature and humidity checks are skipped when the sensor did not answer
  if (readings.dhtValid) {

    // Checks if the temperature is too low
    if (readings.temperature < settings.tempMin && !lowTempSent) {
      Serial.println("The temperature is below the ideal for the plant.");
      if (sendAlert("The temperature is below the ideal for the plant.")) {
        lowTempSent = true;
      }
    }
    else if (readings.temperature >= settings.tempMin && lowTempSent) {
      lowTempSent = false;
    }

    // Checks if the temperature is too high
    if (readings.temperature > settings.tempMax && !highTempSent) {
      Serial.println("The temperature is above the ideal for the plant.");
      if (sendAlert("The temperature is above the ideal for the plant.")) {
        highTempSent = true;
      }
    }
    else if (readings.temperature <= settings.tempMax && highTempSent) {
      highTempSent = false;
    }

    // Checks if the humidity is too low
    if (readings.humidity < settings.humMin && !lowHumiditySent) {
      Serial.println("The air humidity is below the ideal for the plant.");
      if (sendAlert("The air humidity is below the ideal for the plant.")) {
        lowHumiditySent = true;
      }
    }
    else if (readings.humidity >= settings.humMin && lowHumiditySent) {
      lowHumiditySent = false;
    }

    // Checks if the humidity is too high
    if (readings.humidity > settings.humMax && !highHumiditySent) {
      Serial.println("The air humidity is above the ideal for the plant.");
      if (sendAlert("The air humidity is above the ideal for the plant.")) {
        highHumiditySent = true;
      }
    }

    else if (readings.humidity <= settings.humMax && highHumiditySent) {
      highHumiditySent = false;
    }
  }
}
