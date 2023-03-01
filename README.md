# Smart Garden: IoT garden with an ESP32

This is our project for the Integrated Project subject of the CTeSP in Informatics at AESM. An ESP32 watches a plant with several sensors, waters it automatically with a relay-controlled pump, sends WhatsApp alerts when the conditions are bad and serves a small web page with the live readings. The project is called "Horta IoT com Arduino: supervisão automatizada para plantas saudaveis" in the delivered report.

## Features

- Measures air temperature and humidity (DHT11), light intensity, soil moisture and whether there is water in the tank.
- Starts the pump when the soil is dry (below 30 %) and stops it when the soil is wet enough (above 70 %).
- Protects the pump: it stops when the tank is empty, when the soil sensor gives an invalid value and after a maximum run time of 30 seconds, followed by a pause of 60 seconds.
- Sends WhatsApp alerts through CallMeBot: temperature outside 18 to 26 C, air humidity outside 50 to 70 %, empty tank, broken soil or DHT11 sensor and pump safety stop. Each alert is sent once per event and is retried if the message could not be delivered.
- Serves a web page on port 80 with the last readings, refreshed every second.
- Keeps working without Wi-Fi and reconnects on its own when the network comes back.

## Hardware

| Part | Notes |
| --- | --- |
| ESP32 development board | Any ESP32 DevKit |
| DHT11 | Temperature and air humidity |
| KY-018 | Light sensor (LDR), analog output |
| Soil moisture sensor | Analog output |
| Contactless liquid level sensor | Digital output, HIGH when there is water |
| Relay module | Switches the pump, active HIGH |
| Water pump and tank | Powered through the relay |

### Wiring

| ESP32 pin | Component |
| --- | --- |
| GPIO18 | DHT11 data |
| GPIO19 | Relay input (water pump) |
| GPIO34 | KY-018 light sensor, analog output |
| GPIO35 | Contactless liquid level sensor, digital output |
| GPIO32 | Soil moisture sensor, analog output |
| 3V3 | Sensor supply (analog sensors must not output more than 3.3 V) |
| GND | Common ground for sensors, relay and the pump supply |

The pump has its own power supply and is only switched by the relay contacts.

## Software requirements

- Arduino IDE (or arduino-cli).
- Board package: **esp32 by Espressif Systems** (Boards Manager), board "ESP32 Dev Module".
- Libraries (Library Manager):
  - **DHT sensor library** by Adafruit (it also asks for Adafruit Unified Sensor).
  - **UrlEncode** by plageoj.
- `WiFi`, `WebServer` and `HTTPClient` come with the ESP32 board package.

## Setup

1. Open `firmware/esp32_smart_garden/esp32_smart_garden.ino` in the Arduino IDE. The folder name must stay equal to the sketch name.
2. Create the local configuration file. In the sketch folder, copy `secrets.example.h` to `secrets.h` and fill in your values:

   ```cpp
   #define WIFI_SSID "your Wi-Fi network name"
   #define WIFI_PASSWORD "your Wi-Fi password"
   #define WHATSAPP_PHONE "+351912345678"   // country code and number
   #define CALLMEBOT_API_KEY "your CallMeBot key"
   ```

   `secrets.h` is ignored by git and must never be committed.
3. Get a CallMeBot API key:
   1. Add the CallMeBot WhatsApp number shown at <https://www.callmebot.com/blog/free-api-whatsapp-messages/> to your phone contacts.
   2. Send it the message `I allow callmebot to send me messages` on WhatsApp.
   3. CallMeBot answers with your API key. Put it in `secrets.h` together with your phone number.
4. Select the board and port, then upload.

### Calibration

The soil humidity is computed from the sensor voltage with two constants at the top of the sketch, `soilVoltageDry` and `soilVoltagePerPercent`. They depend on the sensor, so check the values printed on the Serial monitor with the sensor in dry and in wet soil and adjust them.

## Run

1. Open the Serial monitor at 9600 baud. The board prints the IP address once it is connected to Wi-Fi.
2. Open `http://<ip address>/` in a browser on the same network. The page shows temperature, air humidity, light, soil humidity, tank state and pump state. The background image and the style sheet are loaded from the internet.
3. Every second the Serial monitor shows the readings and the actions of the pump.

## Project structure

```
esp32-smart-garden/
  firmware/
    esp32_smart_garden/
      esp32_smart_garden.ino   sketch
      secrets.example.h        template for the local secrets.h
  docs/
    REPORT.md                  project report in English
    project-report.pt.docx     delivered project report (Portuguese)
    schedule-original.mpp      MS Project schedule, initial plan
    schedule-edited.mpp        MS Project schedule, adjusted
  README.md
```

## How it works

`loop()` runs without `delay()`. On every pass it checks the Wi-Fi connection and serves web clients. Once per second it reads all the sensors once into shared variables, checks that the soil value is valid, lets `updatePump()` decide whether the relay is on, prints the readings and checks the alert conditions. The web page is built from the last stored readings, so the page, the Serial output and the pump logic always agree. `updatePump()` is the only place that writes to the relay pin.

| Condition | Action |
| --- | --- |
| Soil below 30 % and tank has water | Pump on |
| Soil above 70 %, tank empty, invalid soil value or 30 s run time | Pump off |
| After a 30 s safety stop | Pump stays off for 60 s and an alert is sent |
| Temperature below 18 C or above 26 C | WhatsApp alert |
| Air humidity below 50 % or above 70 % | WhatsApp alert |
| Soil value outside 0 to 100 % or DHT11 not answering | WhatsApp alert |

## Team

- Tiago Cabaça (81744)
- Francisco Diniz (81809)

## Course context

Integrated Project, teacher Fernando Barros. CTeSP in Informatics (Curso Técnico Superior Profissional), Academia de Ensino Superior de Mafra (AESM), academic year 2022/2023.

## Documentation

- [Project report in English](docs/REPORT.md)
- [Delivered project report in Portuguese](docs/project-report.pt.docx)
- Schedules: [initial plan](docs/schedule-original.mpp) and [adjusted schedule](docs/schedule-edited.mpp)
