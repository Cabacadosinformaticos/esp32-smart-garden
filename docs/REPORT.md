# Smart Garden: IoT garden with an ESP32

Project report (English version)

| | |
| --- | --- |
| Project | Horta IoT com Arduino: supervisão automatizada para plantas saudáveis |
| Subject | Projeto Integrado (Integrated Project) |
| Teacher | Fernando Barros |
| Course | CTeSP in Informatics, Academia de Ensino Superior de Mafra (AESM) |
| Academic year | 2022/2023 |
| Authors | Tiago Cabaça (81744) and Francisco Diniz (81809) |

The Portuguese report with the full progress log is in [project-report.pt.docx](project-report.pt.docx). This document describes the same project in English and follows the final code in `firmware/esp32_smart_garden/`.

## 1. Summary

We build an IoT garden that supervises a plant and waters it without human help. An ESP32 reads air temperature and humidity, light, soil moisture and the water level of the tank. It switches a pump through a relay when the soil is dry, sends WhatsApp alerts when something is wrong and serves a web page with the live readings. The result is a small system that keeps a plant healthy while its owner is away.

## 2. Problem

Plants die from too little or too much water, and from temperatures and light levels that nobody notices. Checking them every day is hard for people who travel or have little time, and over-watering rots the roots just as much as dry soil harms the plant. The system must therefore:

- measure the conditions of the plant continuously;
- water only when the soil needs it and stop before the soil is soaked;
- warn the owner remotely when a condition is outside the ideal range;
- show the current state in a place the owner can reach from the network.

## 3. Goals and requirements

| Id | Requirement |
| --- | --- |
| R1 | Read temperature, air humidity, light intensity, soil moisture and tank level |
| R2 | Switch the water pump automatically from the soil moisture |
| R3 | Never run the pump without water or without a valid soil reading, and never run it for too long |
| R4 | Send WhatsApp alerts for adverse conditions and sensor failures, each one once per event |
| R5 | Serve a web page with the last readings, usable on a phone |
| R6 | Keep working when Wi-Fi is not available and reconnect automatically |
| R7 | Keep Wi-Fi and WhatsApp credentials out of the source code |

## 4. Architecture and hardware

### 4.1 Components

| Part | Function |
| --- | --- |
| ESP32 development board | Controller, Wi-Fi and web server |
| DHT11 | Air temperature and humidity |
| KY-018 | Light intensity (LDR module) |
| Soil moisture sensor | Soil humidity, analog |
| Contactless liquid level sensor | Detects water in the tank from outside the tank, digital |
| Relay module | Switches the pump |
| Water pump and tank | Waters the plant |

### 4.2 Wiring

| ESP32 pin | Component |
| --- | --- |
| GPIO18 | DHT11 data |
| GPIO19 | Relay input |
| GPIO34 | KY-018 analog output (ADC1) |
| GPIO35 | Liquid level sensor digital output |
| GPIO32 | Soil moisture analog output (ADC1) |

The analog sensors use ADC1 pins because they keep working while Wi-Fi is on. The ESP32 ADC is 12 bit (0 to 4095) with a 3.3 V range.

### 4.3 Software structure

The sketch is a single file with these parts:

| Part | Role |
| --- | --- |
| `connectWiFi()` | Connects with a 15 s timeout, used in `setup()` |
| `sendMessage()` / `sendAlert()` | Sends a WhatsApp text through the CallMeBot HTTP API (GET request, response code 200 means success); failed alerts are retried after 30 s |
| `readSoilHumidity()` | Converts the soil ADC value to a percentage |
| `readSensors()` | Reads every sensor once into shared variables |
| `updatePump()` | Decides whether the pump runs, the only code that writes to the relay |
| `loop()` | Reconnects Wi-Fi, serves web clients, runs the one-second cycle |
| `handleRoot()` | Builds the web page from the last readings |

`secrets.h` holds the Wi-Fi name and password, the phone number and the CallMeBot key. It is created from `secrets.example.h` and is not part of the repository.

## 5. Implementation

### 5.1 Sensors

Every second `readSensors()` stores the soil humidity, tank state, air humidity, temperature and light intensity. The light value is mapped from the ADC range to 0 to 100 %. The soil voltage is `raw * 3.3 / 4095`, and the percentage is `(voltage - soilVoltageDry) / soilVoltagePerPercent`. The two constants describe the sensor curve and are adjusted for the sensor in use. A soil value outside 0 to 100 % means the sensor is broken or unplugged.

A DHT11 reading that is not a number (`NaN`) means the sensor did not answer. The temperature and humidity alerts are skipped in that cycle, one alert reports the failure and the web page shows "n/a".

### 5.2 Pump control

| Rule | Value |
| --- | --- |
| Start | Soil valid, below 30 %, tank has water and no pause active |
| Stop | Soil above 70 %, tank empty, soil value invalid, or run time of 30 s reached |
| Pause after a safety stop | 60 s |

The relay is always switched off when the pump is not supposed to run, including when the soil reading is invalid and the tank is empty. After a safety stop an alert asks the owner to check the soil sensor and the tank.

### 5.3 Alerts

Alerts go to one WhatsApp number through CallMeBot. The flags `lowTempSent`, `highTempSent` and the similar ones make each alert go out once while the condition lasts, and they are cleared when the value returns to the normal range. A flag is only set after the message is accepted by the server, so a message lost because of the network is attempted again.

| Alert | Condition |
| --- | --- |
| Temperature too low / too high | Below 18 C / above 26 C |
| Air humidity too low / too high | Below 50 % / above 70 % |
| Tank empty | Soil is dry and there is no water |
| Soil sensor not working | Soil value outside 0 to 100 % |
| Temperature and humidity sensor not working | DHT11 returns no value |
| Pump safety stop | Maximum run time reached |

### 5.4 Web page

The ESP32 serves a page on port 80 with six cards: temperature, humidity, light, soil humidity, water tank and water pump. The layout is a CSS grid with three columns and switches to one column below 890 px width. The page asks the browser to refresh every second. The values come from the last readings, so the page never reads the sensors itself.

### 5.5 Timing and connectivity

`loop()` has no blocking delay. The web server runs on every pass and the readings run once per second, using `millis()`. If Wi-Fi is lost the sketch tries to reconnect every 10 seconds without blocking, while the pump logic keeps running.

## 6. Data

The system has no database. It keeps only the last readings in memory, which are shown on the web page and printed on the Serial monitor at 9600 baud.

## 7. Project management

We planned the work in MS Project (the initial plan is in `schedule-original.mpp`, the adjusted schedule in `schedule-edited.mpp`). The progress log in the Portuguese report has six entries:

| Date (2023) | Progress |
| --- | --- |
| 27 January | Planning finished: objectives, materials, schedule, components bought and checked |
| 30 January | Materials ready, assembly of the electronic components starts |
| 6 February | Assembly, sensor configuration and tests finished, sensor data collected, work on pump control starts |
| 15 February | Pump control, WhatsApp alerts, data processing algorithms and the web page finished |
| 27 February | Delays because of sensor configuration and assembly adjustments, and a communication problem that needs some components replaced |
| 6 March | Web page integration, field tests, stress and long-duration tests, adjustments and final test finished; finishing the project and the documentation continues |

## 8. Testing

The tests of the schedule are field tests of the whole system, identification of problems, a stress test, a long-duration test and a final test. We check that:

- every sensor gives plausible values on the Serial monitor;
- the pump starts with dry soil and stops with wet soil or an empty tank;
- the alerts reach the phone once per event;
- the web page shows the same values as the Serial monitor on a computer and on a phone;
- the board recovers after the Wi-Fi network goes away and comes back.

## 9. Difficulties

- The initial configuration of the components and the calibration of the sensors take longer than planned.
- A communication problem between components forces us to replace some of them, which delays the schedule.
- The ESP32 ADC works with 3.3 V, so the analog sensors and the voltage conversion have to use that range.
- The soil sensor curve differs between sensors, so its two calibration constants must be measured.
- Without limits, a faulty sensor or an empty tank could keep the pump running, so the sketch has the safety rules of section 5.2.

## 10. Conclusions

The project shows that an ESP32 with a few cheap sensors can supervise a plant, water it only when needed, warn its owner remotely and show the state on a web page. The main lessons are the importance of calibrating the sensors, protecting the pump with simple safety rules and handling the failure of every sensor and of the network. Possible extensions are more sensors (for example CO2), more pumps for several plants and a database to keep the history of the readings.
