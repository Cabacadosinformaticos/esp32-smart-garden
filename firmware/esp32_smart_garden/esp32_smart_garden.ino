// Smart garden (Horta IoT)
// Sketch for an ESP32 board. It reads the DHT11 air temperature and humidity sensor,
// the light sensor, the soil moisture sensor and the water tank level sensor, waters the
// plant with a relay and a pump, sends WhatsApp alerts through CallMeBot and serves a web
// page with the last readings.
// The Wi-Fi and CallMeBot settings live in secrets.h (copy secrets.example.h).

// Pins (ESP32 GPIO numbers)
const int dhtSensorPin = 18; // GPIO18: DHT11 data pin (temperature and air humidity)
const int relayPin = 19; // GPIO19: relay input that switches the water pump
const int lightSensorPin = 34; // GPIO34 (ADC1): KY-018 light sensor analog output
const int waterSensorPin = 35; // GPIO35: contactless liquid level sensor digital output (HIGH = water in the tank)
const int soilSensorPin = 32; // GPIO32 (ADC1): soil moisture sensor analog output

// ADC and soil sensor calibration
const int adcMaxValue = 4095; // 12-bit ADC of the ESP32
const float adcReferenceVoltage = 3.3; // ESP32 ADC input range, not 5 V
// These two values describe the sensor curve and must be calibrated for the sensor in use
const float soilVoltageDry = 0.92; // voltage that is converted to 0 % (offset of the sensor curve)
const float soilVoltagePerPercent = 0.08; // volts per 1 % of soil humidity (slope of the sensor curve)

// Watering thresholds
const float soilDryLimit = 30; // soil humidity (percent) below which the pump starts
const float soilWetLimit = 70; // soil humidity (percent) above which the pump stops
const unsigned long maxPumpRunTime = 30000; // safety timeout in ms, the pump is always stopped after this time
const unsigned long pumpPauseTime = 60000; // ms the pump stays off after a safety stop so the water can soak in

// Global state flags and timers used by loop()
bool notWorkSent = false; // true after the soil sensor alert was sent, so it is sent only once
bool tankEmptySent = false; // true after the empty tank alert was sent, so it is sent only once
bool pumpWorking = false; // True while the water pump is running
unsigned long pumpStartTime = 0; // Time when the pump started, used by the safety timeout
unsigned long pumpBlockedUntil = 0; // Time until the pump has to stay off after a safety stop
bool pumpTimeoutSent = false; // True after the safety timeout alert was sent
bool lowTempSent = false; // true after the low temperature alert was sent, so it is sent only once
bool highTempSent = false; // true after the high temperature alert was sent, so it is sent only once
bool lowHumiditySent = false; // true after the low air humidity alert was sent, so it is sent only once
bool highHumiditySent = false; // true after the high air humidity alert was sent, so it is sent only once
bool dhtErrorSent = false; // true after the DHT error alert was sent, so it is sent only once
unsigned long lastWifiAttempt = 0; // Time of the last Wi-Fi connection attempt, used to retry in loop()
unsigned long lastReadTime = 0; // Time of the last sensor reading and control cycle, used to time loop()
unsigned long nextAlertTime = 0; // Time before which no alert is attempted again, set after a failed send

// Last readings, shared by loop() and the web page so the sensors are read only once per cycle
float soilhumidity = 0; // Soil humidity in percent
float humidity = 0; // Air humidity in percent
float temperature = 0; // Air temperature in degrees Celsius
float lightIntensity = 0; // Light intensity in percent
int waterSensorValue = HIGH; // Value of the contactless liquid sensor
bool dhtValid = false; // False when the DHT sensor did not answer

#include <WiFi.h> // Includes the WiFi library in the program to allow connection to Wi-Fi networks
#include <HTTPClient.h> // Includes the HTTPClient library in the program to make HTTP requests to a server
#include <WebServer.h> // Includes the WebServer library in the program to create a web server that can be used to query the IoT device
#include <UrlEncode.h> // Includes the UrlEncode library in the program to encode URLs to be sent as parameters in HTTP requests
#include <DHT.h> // Includes the DHT library in the program to allow the use of the humidity and temperature sensor
#include "secrets.h" // Includes the Wi-Fi, phone number and API key defined in the secrets.h file
#define DHTTYPE DHT11 // Defines the type of DHT sensor being used (DHT11 in this case)
DHT dht(dhtSensorPin, DHTTYPE); // Creates a DHT library instance with the DHT sensor pin and the type defined above


const char* ssid = WIFI_SSID; // Defines the name of the Wi-Fi network (SSID) the device will connect to
const char* password = WIFI_PASSWORD; // Defines the password of the Wi-Fi network the device will connect to

// +international_country_code + phone number
// Portugal +351, example: +351912345678
String phoneNumber = WHATSAPP_PHONE; // Defines the phone number that will receive the text messages (country code + phone number)
String apiKey = CALLMEBOT_API_KEY; // Defines the API key used to send text messages

WebServer server(80); // Creates a server on port 80

const unsigned long wifiConnectTimeout = 15000; // Time in ms that setup() waits for the Wi-Fi connection
const unsigned long wifiRetryInterval = 10000; // Time in ms between reconnection attempts in loop()
const unsigned long readInterval = 1000; // Time in ms between sensor readings and control cycles
const unsigned long alertRetryInterval = 30000; // Time in ms to wait after a failed send before trying again

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
  pinMode(relayPin, OUTPUT); // Sets the relay pin as output

  // Connects to the Wi-Fi network, setup continues even if it fails
  connectWiFi();
  lastWifiAttempt = millis();

  // Sets the route for the web page
  server.on("/", handleRoot);

  // Starts the web server
  server.begin();
  Serial.println("Server started");
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
  float soilhumidity = (voltage - soilVoltageDry) / soilVoltagePerPercent; // Calculates the soil humidity and converts the value to a scale from 0 to 100
  return soilhumidity;
}

// Reads every sensor once and stores the values in the global last readings
void readSensors() {
  soilhumidity = readSoilHumidity(); // Reads the soil humidity as a percentage
  waterSensorValue = digitalRead(waterSensorPin); // Reads the value of the contactless liquid sensor
  humidity = dht.readHumidity(); // Reads the relative air humidity from the DHT humidity and temperature sensor
  temperature = dht.readTemperature(); // Reads the temperature from the DHT humidity and temperature sensor
  int lightSensorValue = analogRead(lightSensorPin); // Reads the value of the light intensity sensor
  lightIntensity = map(lightSensorValue, 0, adcMaxValue, 100, 0); // Converts the value read from the light intensity sensor to a scale from 0 to 100

  // NaN means the DHT sensor did not answer, so the readings cannot be used
  dhtValid = !isnan(humidity) && !isnan(temperature);
}

// Decides if the water pump must run or stop
// This is the only function that writes to the relay pin
void updatePump(float soilhumidity, int waterSensorValue) {

  bool soilValid = soilhumidity >= 0 && soilhumidity <= 100; // False when the sensor is broken or unplugged
  bool tankEmpty = (waterSensorValue == LOW);

  // The tank has water again, so a new empty tank alert can be sent later
  if (!tankEmpty) {
    tankEmptySent = false;
  }

  // The soil is dry again, so a new safety timeout alert can be sent later
  if (soilValid && soilhumidity > soilDryLimit) {
    pumpTimeoutSent = false;
  }

  // Warns when the soil is dry but there is no water in the tank
  if (soilValid && soilhumidity < soilDryLimit && tankEmpty && !tankEmptySent) {
    Serial.println("The water tank is empty, the plant cannot be watered.");
    if (sendAlert("The water tank is empty, the plant cannot be watered.")) {
      tankEmptySent = true;
    }
  }

  if (pumpWorking) {

    // Checks every reason to stop the pump
    if (tankEmpty) {
      digitalWrite(relayPin, LOW); // Turns the relay off
      pumpWorking = false;
      Serial.println("Stopping the pump: the water tank is empty.");
    } else if (!soilValid) {
      digitalWrite(relayPin, LOW); // Turns the relay off
      pumpWorking = false;
      Serial.println("Stopping the pump: the soil humidity reading is not valid.");
    } else if (soilhumidity > soilWetLimit) {
      digitalWrite(relayPin, LOW); // Turns the relay off
      pumpWorking = false;
      Serial.println("Stopping the pump: the soil is wet enough.");
    } else if (millis() - pumpStartTime >= maxPumpRunTime) {
      digitalWrite(relayPin, LOW); // Turns the relay off
      pumpWorking = false;
      Serial.println("Stopping the pump: the maximum run time was reached.");
      pumpBlockedUntil = millis() + pumpPauseTime; // Leaves the water time to soak in
      if (!pumpTimeoutSent) { // Sends the alert only once per safety stop
        if (sendAlert("The pump was stopped by the safety timeout, please check the soil sensor and the tank.")) {
          pumpTimeoutSent = true;
        }
      }
    }

  } else {

    digitalWrite(relayPin, LOW); // Keeps the relay off while the pump is not working

    // Starts the pump only when the soil is dry, the tank has water and the pause is over
    if (soilValid && soilhumidity < soilDryLimit && !tankEmpty && (long)(millis() - pumpBlockedUntil) >= 0) {
      digitalWrite(relayPin, HIGH); // Turns the relay on
      pumpWorking = true;
      pumpStartTime = millis();
      Serial.println("Watering the plant...");
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

  server.handleClient(); // Handles any client that is communicating with the server at that moment

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
  if (soilhumidity < 0 || soilhumidity > 100) {

    if (!notWorkSent) {

    Serial.println("Error: Humidity value outside the valid range (0-100%)");

    if (sendAlert("The soil humidity sensor is not working!")) {
      notWorkSent = true;
    }

    }

   } else if (soilhumidity >= 0 && soilhumidity <= 100) {

    notWorkSent = false;

    // Shows the result on the serial monitor
    Serial.print("Soil humidity: ");
    Serial.print(soilhumidity, 2);
    Serial.println("%");
  }

  updatePump(soilhumidity, waterSensorValue); // Decides if the water pump must run or stop

  // Shows the values read by the sensors on the Serial monitor
  if (dhtValid) {

    dhtErrorSent = false; // The sensor is working again

    Serial.print("Humidity and temperature sensor: ");
    Serial.print("Humidity = ");
    Serial.print(humidity);
    Serial.print("%, Temperature = ");
    Serial.print(temperature);
    Serial.println(" ºC");

  } else if (!dhtErrorSent) {

    Serial.println("Error: could not read the DHT11 sensor");

    if (sendAlert("The temperature and humidity sensor is not working!")) {
      dhtErrorSent = true;
    }

  }

  Serial.print("Light sensor: ");
  Serial.print("Light intensity = ");
  Serial.print(lightIntensity);
  Serial.println("%");

  // Checks if there is water in the tank and shows the result on the Serial monitor
  Serial.print("Contactless liquid sensor: ");
  if (waterSensorValue == LOW) {
    Serial.println("No liquid detected");
  } else {
    Serial.println("Liquid detected");
  }

  // Alerts to send if the plant is facing an adverse situation
  // All the temperature and humidity checks are skipped when the sensor did not answer
  if (dhtValid) {

    // Checks if the temperature is too low
    if (temperature < 18 && !lowTempSent) {
      Serial.println("The temperature is below the ideal for the plant.");
      if (sendAlert("The temperature is below the ideal for the plant.")) {
        lowTempSent = true;
      }
    }
    else if (temperature >= 18 && lowTempSent) {
      lowTempSent = false;
    }

    // Checks if the temperature is too high
    if (temperature > 26 && !highTempSent) {
      Serial.println("The temperature is above the ideal for the plant.");
      if (sendAlert("The temperature is above the ideal for the plant.")) {
        highTempSent = true;
      }
    }
    else if (temperature <= 26 && highTempSent) {
      highTempSent = false;
    }

    // Checks if the humidity is too low
    if (humidity < 50 && !lowHumiditySent) {
      Serial.println("The air humidity is below the ideal for the plant.");
      if (sendAlert("The air humidity is below the ideal for the plant.")) {
        lowHumiditySent = true;
      }
    }
    else if (humidity >= 50 && lowHumiditySent) {
      lowHumiditySent = false;
    }

    // Checks if the humidity is too high
    if (humidity > 70 && !highHumiditySent) {
      Serial.println("The air humidity is above the ideal for the plant.");
      if (sendAlert("The air humidity is above the ideal for the plant.")) {
        highHumiditySent = true;
      }
    }

    else if (humidity <= 70 && highHumiditySent) {
      highHumiditySent = false;
    }
  }
}

// Function responsible for the Web page
void handleRoot() {

  // Uses the last readings taken by readSensors(), no sensor is read here
  String watertank = ""; // Initializes the watertank variable as an empty string
  String waterpump = ""; // Initializes the waterpump variable as an empty string

  // Checks if there is water in the tank
  if (waterSensorValue == LOW) {
    watertank += "Tank empty"; // Concatenates the text into the watertank variable
  } else {
    watertank += "Tank with water"; // Concatenates the text into the watertank variable
  }

  // Checks if the water pump is on
  if (pumpWorking == false) {
    waterpump += "Pump off"; // Concatenates the text into the waterpump variable
  } else {
    waterpump += "Watering the plant"; // Concatenates the text into the waterpump variable
  }

  // Shows "n/a" instead of a number when the DHT sensor did not answer
  String temperatureText = dhtValid ? String(temperature) : "n/a";
  String humidityText = dhtValid ? String(humidity) : "n/a";

  // Web page code, built piece by piece so it is easier to read
  String html = "<html lang='en'><head><meta charset='UTF-8'> <title>IoT Garden</title> <meta name='viewport' content='width=device-width, initial-scale=1'> <link rel='icon' href='https://icons.iconarchive.com/icons/toma4025/tea/128/tea-plant-leaf-icon.png'> <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'> <style> ";

  // Page and sensor card styles
  html += "body { font-family: Arial, sans-serif; background-color: #000000; text-align: center; padding-top: 50px; padding: 20px; background-image: url('https://ensina.rtp.pt/site-uploads/2021/05/movimento_xilemico_plantas_vasculares-854x480.jpg');background-repeat: no-repeat;background-size: cover; } h1 { color: white; font-size: 70px; } .sensor-name { font-size: 24px; font-weight: bold; margin-bottom: 18px; } .sensor-value { font-size: 36px; font-weight: bold; margin-bottom: 10px; } .sensor-reading { background-color: white; border: 1px solid #333; border-radius: 10px; padding: 14px 20px; box-shadow: 2px 2px 5px #ccc; display: flex; flex-direction: column; align-items: center; justify-content: space-around; text-align: center; margin: 10px; } ";

  // Grid layout for the desktop, one area per card
  html += ".grid { display: grid; grid-template-columns: 1fr 1fr 1fr ; grid-template-rows: 150px 150px; grid-template-areas: 'temperature humidity light' 'soilHumidity tank pump'; } ";

  // Responsive layout, one card per row on small screens
  html += "@media screen and (max-width: 890px) {.grid { display: grid; grid-template-columns: 1fr; grid-template-rows: 190px 190px 190px 190px 190px 190px ; grid-template-areas: 'temperature' 'humidity' 'light' 'soilHumidity' 'tank' 'pump'; padding-left:50px; padding-right:50px; }.sensor-name { margin-bottom: -50px; }h1 {font-size: 30px;}} ";

  html += "</style></head><body> <h1>IoT Garden</h1> <div class='grid'> ";

  // Temperature card
  html += "<div class='sensor-reading' style='grid-area: temperature'> <div class='sensor-name'>Temperature:</div> <div class='sensor-value'>" + temperatureText + "ºC</div> </div> ";

  // Air humidity card
  html += "<div class='sensor-reading' style='grid-area: humidity'> <div class='sensor-name'>Humidity:</div> <div class='sensor-value'>" + humidityText + "%</div> </div> ";

  // Light card
  html += "<div class='sensor-reading' style='grid-area: light'> <div class='sensor-name'>Light:</div> <div class='sensor-value'>" + String(lightIntensity) + "%</div> </div> ";

  // Soil humidity card
  html += "<div class='sensor-reading' style='grid-area: soilHumidity'> <div class='sensor-name'>Soil humidity:</div> <div class='sensor-value'>" + String(soilhumidity) + "%</div> </div> ";

  // Water tank card
  html += "<div class='sensor-reading' style='grid-area: tank'> <div class='sensor-name'>Water tank:</div> <div class='sensor-value'>" + String(watertank) + "</div> </div> ";

  // Water pump card
  html += "<div class='sensor-reading' style='grid-area: pump'> <div class='sensor-name'>Water pump:</div> <div class='sensor-value'>" + String(waterpump) + "</div> </div> ";

  html += "</div></body></html>";


  int refreshTime = 1; // Defines the time in seconds for the page refresh

  // Sends the HTML page to the client with the automatic refresh instruction
  server.sendHeader("Refresh", String(refreshTime));
  server.send(200, "text/html", html);
}
