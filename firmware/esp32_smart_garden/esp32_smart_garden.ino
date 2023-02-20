// Smart garden: reads the sensors, waters the plant and serves a web page.

const int dhtSensorPin = 18; // DHT sensor pin connected to Arduino pin 2
const int relayPin = 19; // Relay control pin connected to Arduino pin 4
const int lightSensorPin = 34; // KY-018 light sensor pin connected to Arduino pin A0
const int waterSensorPin = 35; // Contactless liquid sensor pin connected to Arduino pin A1
const int soilSensorPin = 32; // Soil humidity sensor pin connected to Arduino pin A2

bool notWorkSent = false; // Initializes the variable notWorkSent
bool tankEmptySent = false; // Initializes the variable tankEmptySent
bool pumpworking = false; // Initializes the variable pumpworking
bool lowTempSent = false; // Initializes the variable lowTempSent
bool highTempSent = false; // Initializes the variable highTempSent
bool lowHumiditySent = false; // Initializes the variable lowHumiditySent
bool highHumiditySent = false; // Initializes the variable highHumiditySent

#include <WiFi.h> // Includes the WiFi library in the program to allow connection to Wi-Fi networks
#include <HTTPClient.h> // Includes the HTTPClient library in the program to make HTTP requests to a server
#include <WebServer.h> // Includes the WebServer library in the program to create a web server that can be used to query the IoT device
#include <UrlEncode.h> // Includes the UrlEncode library in the program to encode URLs to be sent as parameters in HTTP requests
#include <DHT.h> // Includes the DHT library in the program to allow the use of the humidity and temperature sensor
#define DHTTYPE DHT11 // Defines the type of DHT sensor being used (DHT11 in this case)
DHT dht(dhtSensorPin, DHTTYPE); // Creates a DHT library instance with the DHT sensor pin and the type defined above


const char* ssid = "YOUR_WIFI_SSID"; // Defines the name of the Wi-Fi network (SSID) the device will connect to
const char* password = "YOUR_WIFI_PASSWORD"; // Defines the password of the Wi-Fi network the device will connect to

// +international_country_code + phone number
// Portugal +351, example: +351912345678
String phoneNumber = "+351XXXXXXXXX"; // Defines the phone number that will receive the text messages (country code + phone number)
String apiKey = "YOUR_CALLMEBOT_API_KEY"; // Defines the API key used to send text messages

WebServer server(80); // Creates a server on port 80

void setup() {
  Serial.begin(9600); // Starts serial communication
  dht.begin(); // Starts the humidity and temperature sensor
  pinMode(waterSensorPin, INPUT); // Sets the contactless liquid sensor pin as input
  pinMode(lightSensorPin, INPUT); // Sets the KY-018 light sensor pin as input
  pinMode(soilSensorPin, INPUT); // Sets the soil humidity sensor pin as input
  pinMode(relayPin, OUTPUT); // Sets the relay pin as output

  // Connects to the Wi-Fi network
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Sets the route for the web page
  server.on("/", handleRoot);

  // Starts the web server
  server.begin();
  Serial.println("Server started");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function that sends a text message to the specified phone number
void sendMessage(String message){

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message); // Builds the URL with the information needed to send the text message through the CallMeBot API
  HTTPClient http; // Starts an HTTP POST request using the URL created above
  http.begin(url);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Sets the Content-Type header to application/x-www-form-urlencoded

  int httpResponseCode = http.POST(url); // Sends the HTTP POST request and stores the HTTP response code in a variable
  if (httpResponseCode == 200){ // Checks if the message was sent successfully and prints a message on the serial monitor
    Serial.print("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  http.end(); // Releases the resources used in the HTTP request
}

void loop() {

  server.handleClient(); // Handles any client that is communicating with the server at that moment

  int soilSensorValue = analogRead(soilSensorPin); // Reads the value of the soil humidity sensor
  float voltage = soilSensorValue * (5.0 / 4095.0); // Calculates the voltage from the value read from the soil humidity sensor
  float soilhumidity = (voltage - 0.92) / 0.08; // Calculates the soil humidity from the voltage read from the soil humidity sensor and converts the value to a scale from 0 to 100
  int waterSensorValue = digitalRead(waterSensorPin); // Reads the value of the contactless liquid sensor
  float humidity = dht.readHumidity(); // Reads the relative air humidity from the DHT humidity and temperature sensor
  float temperature = dht.readTemperature(); // Reads the temperature from the DHT humidity and temperature sensor
  int lightSensorValue = analogRead(lightSensorPin); // Reads the value of the light intensity sensor
  float lightIntensity = map(lightSensorValue, 0, 4095, 100, 0); // Converts the value read from the light intensity sensor to a scale from 0 to 100

  Serial.println("--- Readings ---");
  ("Soil humidity sensor: ");
  // Checks if the value read by the soil humidity sensor is inside the valid range (0-100%)
  // If the value is outside the range, sends an error message and sets notWorkSent to true
  // Otherwise, shows the soil humidity and checks whether the plant needs to be watered or not
  if (soilhumidity < 0 || soilhumidity > 100) {

    if (!notWorkSent) {

    Serial.println("Error: Humidity value outside the valid range (0-100%)");

    sendMessage("The soil humidity sensor is not working!");

    notWorkSent = true;

    }

   } else if (soilhumidity >= 0 && soilhumidity <= 100) {

    notWorkSent = false;

    // Shows the result on the serial monitor
    Serial.print("Soil humidity: ");
    Serial.print(soilhumidity, 2);
    Serial.println("%");

    // Checks if the soil humidity is below 30% and there is water in the tank; if both are true it starts watering
    if (soilhumidity < 30) {
      if (waterSensorValue == LOW && tankEmptySent == false) {
        Serial.println("The water tank is empty, the plant cannot be watered.");
        sendMessage("The water tank is empty, the plant cannot be watered.");
        tankEmptySent = true;

      }
      if (waterSensorValue == HIGH) {
        digitalWrite(relayPin, HIGH); // Turns the relay on
        Serial.println("Watering the plant...");
        tankEmptySent = false;
        pumpworking = true;
      }
    }

    // Checks if the soil humidity is above 70% or if there is no more water in the tank; if either is true it stops watering
    if (soilhumidity > 70 || waterSensorValue == LOW) {
      digitalWrite(relayPin, LOW); // Turns the relay off
      Serial.println("Stopping watering the plant...");
      pumpworking = false;
    }
  }

  // Shows the values read by the sensors on the Serial monitor
  Serial.print("Humidity and temperature sensor: ");
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.print("%, Temperature = ");
  Serial.print(temperature);
  Serial.println(" ºC");
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
  // Checks if the temperature is too low
  if (temperature < 18 && !lowTempSent) {
    Serial.println("The temperature is below the ideal for the plant.");
    lowTempSent = true;
     sendMessage("The temperature is below the ideal for the plant.");
  }
  else if (temperature >= 18 && lowTempSent) {
    lowTempSent = false;
  }

  // Checks if the temperature is too high
  if (temperature > 26 && !highTempSent) {
    Serial.println("The temperature is above the ideal for the plant.");
    highTempSent = true;
     sendMessage("The temperature is above the ideal for the plant.");
  }
  else if (temperature <= 26 && highTempSent) {
    highTempSent = false;
  }

  // Checks if the humidity is too low
  if (humidity < 50 && !lowHumiditySent) {
    Serial.println("The air humidity is below the ideal for the plant.");
    lowHumiditySent = true;
  sendMessage("The air humidity is below the ideal for the plant.");
  }
  else if (humidity >= 50 && lowHumiditySent) {
    lowHumiditySent = false;
  }

  // Checks if the humidity is too high
  if (humidity > 70 && !highHumiditySent) {
    Serial.println("The air humidity is above the ideal for the plant.");
    sendMessage("The air humidity is above the ideal for the plant.");
    highHumiditySent = true;
  }

  else if (humidity <= 70 && highHumiditySent) {
    highHumiditySent = false;
  }

  delay(1000); // Waits one second before running the loop again
}

// Function responsible for the Web page
void handleRoot() {

  int soilSensorValue = analogRead(soilSensorPin); // Reads the value of the soil humidity sensor
  float voltage = soilSensorValue * (5.0 / 4095.0); // Calculates the voltage from the value read from the soil humidity sensor
  float soilhumidity = (voltage - 0.92) / 0.08; // Calculates the soil humidity from the voltage read from the soil humidity sensor and converts the value to a scale from 0 to 100
  int waterSensorValue = digitalRead(waterSensorPin); // Reads the value of the contactless liquid sensor
  float humidity = dht.readHumidity(); // Reads the relative air humidity from the DHT humidity and temperature sensor
  float temperature = dht.readTemperature(); // Reads the temperature from the DHT humidity and temperature sensor
  int lightSensorValue = analogRead(lightSensorPin); // Reads the value of the light intensity sensor
  float lightIntensity = map(lightSensorValue, 0, 4095, 100, 0); // Converts the value read from the light intensity sensor to a scale from 0 to 100

  String watertank = ""; // Initializes the watertank variable as an empty string
  String waterpump = ""; // Initializes the waterpump variable as an empty string

  // Checks if there is water in the tank
  if (waterSensorValue == LOW) {
    watertank += "Tank empty"; // Concatenates the text into the watertank variable
  } else {
    watertank += "Tank with water"; // Concatenates the text into the watertank variable
  }

  // Checks if the water pump is on
  if (pumpworking == false) {
    waterpump += "Pump off"; // Concatenates the text into the waterpump variable
  } else {
    waterpump += "Watering the plant"; // Concatenates the text into the waterpump variable
  }

  // Web page code
  String html = "<html lang='en'><head><meta charset='UTF-8'> <title>IoT Garden</title> <meta name='viewport' content='width=device-width, initial-scale=1'> <link rel='icon' href='https://icons.iconarchive.com/icons/toma4025/tea/128/tea-plant-leaf-icon.png'> <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'> <style> body { font-family: Arial, sans-serif; background-color: #000000; text-align: center; padding-top: 50px; padding: 20px; background-image: url('https://ensina.rtp.pt/site-uploads/2021/05/movimento_xilemico_plantas_vasculares-854x480.jpg');background-repeat: no-repeat;background-size: cover; } h1 { color: white; font-size: 70px; } .grid { display: grid; grid-template-columns: 1fr 1fr 1fr ; grid-template-rows: 150px 150px; grid-template-areas: 'temperature humidity light' 'soilHumidity tank pump' } .sensor-name { font-size: 24px; font-weight: bold; margin-bottom: 18px; } .sensor-value { font-size: 36px; font-weight: bold; margin-bottom: 10px; } .sensor-reading { background-color: white; border: 1px solid #333; border-radius: 10px; padding: 14px 20px; box-shadow: 2px 2px 5px #ccc; display: flex; flex-direction: column; align-items: center; justify-content: space-around; text-align: center; margin: 10px; }@media screen and (max-width: 890px) {.grid { display: grid; grid-template-columns: 1fr; grid-template-rows: 190px 190px 190px 190px 190px 190px ; grid-template-areas: 'temperature' 'humidity''light' 'soilHumidity ''tank''pump'; padding-left:50px; padding-right:50px; }.sensor-name { margin-bottom: -50px; }h1 {font-size: 30px;}} </style></head><body> <h1>IoT Garden</h1> <div class='grid'> <div class='sensor-reading'> <div class='sensor-name'>Temperature:</div> <div class='sensor-value'>" + String(temperature) + "ºC</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Humidity:</div> <div class='sensor-value'>" + String(humidity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Light:</div> <div class='sensor-value'>" + String(lightIntensity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Soil humidity:</div> <div class='sensor-value'>" + String(soilhumidity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Water tank:</div> <div class='sensor-value'>" + String(watertank) + "</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Water pump:</div> <div class='sensor-value'>" + String(waterpump) + "</div> </div> </div></body></html>";


  int refreshTime = 1; // Defines the time in seconds for the page refresh

  // Sends the HTML page to the client with the automatic refresh instruction
  server.sendHeader("Refresh", String(refreshTime));
  server.send(200, "text/html", html);
}
