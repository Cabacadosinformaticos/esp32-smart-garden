// Web server and JSON API of the smart garden: serves the old readings page and
// the routes under /api used by the dashboard. The JSON answers are built with
// fixed char buffers and snprintf, so the heap stays free and no ArduinoJson is
// needed. A JSON value that is not a usable number is printed as null.

#include "web.h" // Includes the webBegin and webLoop declarations
#include <Arduino.h> // Needed for String, Serial and millis
#include <WiFi.h> // Needed for WiFi.RSSI and WiFi.localIP
#include <WebServer.h> // Needed for the WebServer class
#include <stdio.h> // Needed for snprintf
#include <stdlib.h> // Needed for strtod and strtol
#include <string.h> // Needed for strlen and memcpy
#include <math.h> // Needed for isnan, isinf and NAN
#include "config.h" // Needed for historyIntervalMs
#include "readings.h" // Needed for the last sensor readings
#include "history.h" // Needed for the history ring buffer
#include "settings.h" // Needed for the runtime settings and settingsCheck
#include "pump.h" // Needed for the pump commands and the pump status

WebServer server(80); // Creates a server on port 80

// ---------------------------------------------------------------- helpers ---

// Writes a JSON number with one decimal, or null when the value is not usable,
// so "nan" or "inf" never reach the client
static void formatNumber(char* out, size_t outSize, float value) {
  if (isnan(value) || isinf(value)) {
    snprintf(out, outSize, "null");
    return;
  }
  snprintf(out, outSize, "%.1f", value);
}

// Writes the local IP address as "a.b.c.d", or "0.0.0.0" when there is no link,
// without building a String
static void formatIp(char* out, size_t outSize) {
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(out, outSize, "0.0.0.0");
    return;
  }
  IPAddress ip = WiFi.localIP();
  snprintf(out, outSize, "%u.%u.%u.%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
}

// Returns the text of the pump mode used in the JSON
static const char* pumpModeName(PumpMode mode) {
  return mode == PUMP_MANUAL ? "manual" : "auto";
}

// Writes the pump JSON object, shared by /api/readings and /api/pump
static int formatPumpJson(char* out, size_t outSize) {
  PumpStatus status = pumpStatus();
  return snprintf(out, outSize,
                  "{\"running\":%s,\"mode\":\"%s\",\"manual_remaining_s\":%u,"
                  "\"blocked_remaining_s\":%u,\"reason\":\"%s\"}",
                  status.running ? "true" : "false", pumpModeName(status.mode),
                  (unsigned)status.manualRemainingSeconds,
                  (unsigned)status.blockedRemainingSeconds, status.reason);
}

// Writes the settings JSON used by GET and POST /api/settings
static void formatSettingsJson(char* out, size_t outSize) {
  snprintf(out, outSize,
           "{\"soil_dry\":%g,\"soil_wet\":%g,\"temp_min\":%g,\"temp_max\":%g,\"hum_min\":%g,"
           "\"hum_max\":%g,\"max_pump_s\":%u,\"pause_s\":%u,\"daily_summary\":%s,\"summary_hour\":%u}",
           settings.soilDry, settings.soilWet, settings.tempMin, settings.tempMax,
           settings.humMin, settings.humMax, (unsigned)settings.maxPumpSeconds,
           (unsigned)settings.pauseSeconds, settings.dailySummary ? "true" : "false",
           (unsigned)settings.summaryHour);
}

// Sends a JSON error object with the given status code
static void sendJsonError(int code, const char* text) {
  char buffer[160];
  snprintf(buffer, sizeof(buffer), "{\"error\":\"%s\"}", text);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", buffer);
}

// Sends the settings JSON with the given status code
static void sendSettingsJson(int code) {
  char buffer[256];
  formatSettingsJson(buffer, sizeof(buffer));
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", buffer);
}

// Sends the pump JSON object with the given status code
static void sendPumpJson(int code) {
  char buffer[160];
  formatPumpJson(buffer, sizeof(buffer));
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", buffer);
}

// Reads a float from a form value, accepting only a full valid number,
// so "abc" and "30abc" are both rejected
static bool parseFloatStrict(const String& text, float* out) {
  if (text.length() == 0) {
    return false;
  }
  const char* start = text.c_str();
  char* end = nullptr;
  double value = strtod(start, &end);
  if (end == start || *end != '\0') {
    return false;
  }
  *out = (float)value;
  return true;
}

// Reads a whole number from a form value, accepting only a full valid number
static bool parseLongStrict(const String& text, long* out) {
  if (text.length() == 0) {
    return false;
  }
  const char* start = text.c_str();
  char* end = nullptr;
  long value = strtol(start, &end, 10);
  if (end == start || *end != '\0') {
    return false;
  }
  *out = value;
  return true;
}

// --------------------------------------------------------------- handlers ---

// Serves the old readings page, kept until the dashboard replaces it
static void handleLegacyPage() {

  // Uses the last readings taken by readSensors(), no sensor is read here
  String watertank = ""; // Initializes the watertank variable as an empty string
  String waterpump = ""; // Initializes the waterpump variable as an empty string

  // Checks if there is water in the tank
  if (readings.tankEmpty) {
    watertank += "Tank empty"; // Concatenates the text into the watertank variable
  } else {
    watertank += "Tank with water"; // Concatenates the text into the watertank variable
  }

  // Checks if the water pump is on
  if (pumpStatus().running == false) {
    waterpump += "Pump off"; // Concatenates the text into the waterpump variable
  } else {
    waterpump += "Watering the plant"; // Concatenates the text into the waterpump variable
  }

  // Shows "n/a" instead of a number when the DHT sensor did not answer
  String temperatureText = readings.dhtValid ? String(readings.temperature) : "n/a";
  String humidityText = readings.dhtValid ? String(readings.humidity) : "n/a";

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
  html += "<div class='sensor-reading' style='grid-area: light'> <div class='sensor-name'>Light:</div> <div class='sensor-value'>" + String(readings.light) + "%</div> </div> ";

  // Soil humidity card
  html += "<div class='sensor-reading' style='grid-area: soilHumidity'> <div class='sensor-name'>Soil humidity:</div> <div class='sensor-value'>" + String(readings.soil) + "%</div> </div> ";

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

// Serves GET /api/readings: all the current values in one fixed buffer
static void handleReadings() {
  char ipText[16]; // Holds the local IP address
  formatIp(ipText, sizeof(ipText));

  // The DHT values are null when the sensor did not answer
  char temperatureText[16];
  char humidityText[16];
  formatNumber(temperatureText, sizeof(temperatureText), readings.dhtValid ? readings.temperature : NAN);
  formatNumber(humidityText, sizeof(humidityText), readings.dhtValid ? readings.humidity : NAN);

  // The soil value is always sent, the soil_ok flag tells if it can be trusted
  char lightText[16];
  char soilText[16];
  formatNumber(lightText, sizeof(lightText), readings.light);
  formatNumber(soilText, sizeof(soilText), readings.soil);

  char buffer[480];
  int length = snprintf(buffer, sizeof(buffer),
                        "{\"uptime_s\":%lu,\"wifi_rssi\":%d,\"ip\":\"%s\",\"heap_free\":%lu,"
                        "\"heap_min\":%lu,\"temperature\":%s,\"humidity\":%s,\"dht_ok\":%s,"
                        "\"light\":%s,\"soil\":%s,\"soil_ok\":%s,\"tank_empty\":%s,\"pump\":",
                        (unsigned long)(millis() / 1000UL), (int)WiFi.RSSI(), ipText,
                        (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                        temperatureText, humidityText, readings.dhtValid ? "true" : "false",
                        lightText, soilText, readings.soilValid ? "true" : "false",
                        readings.tankEmpty ? "true" : "false");

  // Keeps the offset inside the buffer even if the fixed part was truncated
  if (length < 0) {
    length = 0;
  }
  if ((size_t)length >= sizeof(buffer)) {
    length = sizeof(buffer) - 1;
  }

  // Adds the pump object and the closing brace
  int written = formatPumpJson(buffer + length, sizeof(buffer) - (size_t)length);
  if (written < 0 || (size_t)written >= sizeof(buffer) - (size_t)length) {
    length = sizeof(buffer) - 2;
  } else {
    length += written;
  }
  buffer[length] = '}';
  buffer[length + 1] = '\0';

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buffer);
}

// Small buffer that collects the history JSON before it is sent as one chunk
struct ChunkWriter {
  char buffer[256];
  size_t length;
};

// Sends the buffered part of the response and starts a new chunk
static void chunkFlush(ChunkWriter& writer) {
  if (writer.length == 0) {
    return;
  }
  server.sendContent(writer.buffer, writer.length);
  writer.length = 0;
}

// Adds a short text to the chunk buffer, flushing it first when it is nearly full
static void chunkAppend(ChunkWriter& writer, const char* text) {
  size_t textLength = strlen(text);

  // A text larger than the buffer is sent on its own, so it is never dropped
  if (textLength >= sizeof(writer.buffer)) {
    chunkFlush(writer);
    server.sendContent(text, textLength);
    return;
  }

  if (writer.length + textLength > sizeof(writer.buffer) - 8) {
    chunkFlush(writer);
  }

  memcpy(writer.buffer + writer.length, text, textLength);
  writer.length += textLength;
}

// The order of the series in the history JSON
enum HistorySeries { SERIES_TEMPERATURE, SERIES_HUMIDITY, SERIES_SOIL, SERIES_LIGHT, SERIES_PUMP };

// Writes one history value as JSON, null when the sample flag says it is invalid
static void formatHistoryValue(char* out, size_t outSize, const HistorySample& sample, HistorySeries series) {
  bool dhtValid = (sample.flags & 0x02) != 0; // bit1: DHT reading valid
  bool soilValid = (sample.flags & 0x04) != 0; // bit2: soil reading valid

  switch (series) {
    case SERIES_TEMPERATURE:
      if (dhtValid) {
        snprintf(out, outSize, "%.1f", sample.temp10 / 10.0f);
      } else {
        snprintf(out, outSize, "null");
      }
      break;
    case SERIES_HUMIDITY:
      if (dhtValid) {
        snprintf(out, outSize, "%.1f", sample.hum10 / 10.0f);
      } else {
        snprintf(out, outSize, "null");
      }
      break;
    case SERIES_SOIL:
      if (soilValid) {
        snprintf(out, outSize, "%.1f", sample.soil10 / 10.0f);
      } else {
        snprintf(out, outSize, "null");
      }
      break;
    case SERIES_LIGHT:
      snprintf(out, outSize, "%u", (unsigned)sample.light);
      break;
    case SERIES_PUMP:
    default:
      snprintf(out, outSize, "%u", (sample.flags & 0x01) != 0 ? 1u : 0u); // bit0: pump running
      break;
  }
}

// Serves GET /api/history: the JSON is streamed in chunks, one series at a time,
// so no large buffer and no String are used
static void handleHistory() {
  uint16_t count = historyCount(); // 0..historyCapacity

  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  ChunkWriter writer;
  writer.length = 0;

  char header[48]; // Holds the two opening fields of the response
  snprintf(header, sizeof(header), "{\"interval_s\":%lu,\"count\":%u",
           (unsigned long)(historyIntervalMs / 1000UL), (unsigned)count);
  chunkAppend(writer, header);

  char text[16]; // Holds one value at a time

  const char* seriesNames[] = {"temperature", "humidity", "soil", "light", "pump"};

  for (int series = 0; series < 5; series++) {
    chunkAppend(writer, ",\"");
    chunkAppend(writer, seriesNames[series]);
    chunkAppend(writer, "\":[");

    for (uint16_t index = 0; index < count; index++) {
      HistorySample sample;
      if (!historyGet(index, sample)) {
        break;
      }
      if (index > 0) {
        chunkAppend(writer, ",");
      }
      formatHistoryValue(text, sizeof(text), sample, (HistorySeries)series);
      chunkAppend(writer, text);
    }

    chunkAppend(writer, "]");
  }

  chunkAppend(writer, "}");
  chunkFlush(writer);

  server.sendContent(""); // Ends the chunked response
}

// Serves GET /api/settings with the current thresholds
static void handleSettingsGet() {
  sendSettingsJson(200);
}

// Serves POST /api/settings: the given fields are applied to a copy, the whole
// set is validated and only then saved, so a bad field changes nothing
static void handleSettingsPost() {

  // reset=1 restores the defaults and answers with the new settings
  if (server.hasArg("reset") && server.arg("reset") == "1") {
    settingsReset();
    sendSettingsJson(200);
    return;
  }

  Settings next = settings; // Copy changed by the fields, the global one is kept
  char errorText[64];

  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    String value = server.arg(i);
    float number = 0;
    long integer = 0;

    if (name == "soil_dry" || name == "soil_wet" || name == "temp_min" ||
        name == "temp_max" || name == "hum_min" || name == "hum_max") {
      if (!parseFloatStrict(value, &number)) {
        snprintf(errorText, sizeof(errorText), "%s is not a number", name.c_str());
        sendJsonError(400, errorText);
        return;
      }
      if (name == "soil_dry") {
        next.soilDry = number;
      } else if (name == "soil_wet") {
        next.soilWet = number;
      } else if (name == "temp_min") {
        next.tempMin = number;
      } else if (name == "temp_max") {
        next.tempMax = number;
      } else if (name == "hum_min") {
        next.humMin = number;
      } else {
        next.humMax = number;
      }
    } else if (name == "max_pump_s" || name == "pause_s" || name == "summary_hour") {
      if (!parseLongStrict(value, &integer)) {
        snprintf(errorText, sizeof(errorText), "%s is not a number", name.c_str());
        sendJsonError(400, errorText);
        return;
      }
      if (integer < 0 || integer > 65535) { // Keeps the value inside the 16 bit field
        snprintf(errorText, sizeof(errorText), "%s is out of range", name.c_str());
        sendJsonError(400, errorText);
        return;
      }
      if (name == "max_pump_s") {
        next.maxPumpSeconds = (uint16_t)integer;
      } else if (name == "pause_s") {
        next.pauseSeconds = (uint16_t)integer;
      } else {
        next.summaryHour = (uint8_t)integer;
      }
    } else if (name == "daily_summary") {
      if (value == "0") {
        next.dailySummary = false;
      } else if (value == "1") {
        next.dailySummary = true;
      } else {
        sendJsonError(400, "daily_summary must be 0 or 1");
        return;
      }
    }
    // Any unknown field is ignored
  }

  const char* error = settingsCheck(next);
  if (error != nullptr) {
    sendJsonError(400, error);
    return;
  }

  Settings previous = settings; // Kept so a failed save restores the old values
  settings = next;
  if (!settingsSave()) {
    settings = previous;
    sendJsonError(500, "could not save the settings");
    return;
  }

  sendSettingsJson(200);
}

// Serves GET /api/pump with the pump object of the readings
static void handlePumpGet() {
  sendPumpJson(200);
}

// Serves POST /api/pump: mode, run and stop are applied in this order
static void handlePumpPost() {

  // Changes the mode first, so a manual run is possible in the same request
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "auto") {
      pumpSetMode(PUMP_AUTO);
    } else if (mode == "manual") {
      pumpSetMode(PUMP_MANUAL);
    } else {
      sendJsonError(400, "mode must be auto or manual");
      return;
    }
  }

  // Starts a timed manual run; the pump rules refuse it with 409
  if (server.hasArg("run")) {
    long seconds = 0;
    if (!parseLongStrict(server.arg("run"), &seconds) || seconds < 0 || seconds > 65535) {
      sendJsonError(400, "run must be a number of seconds between 0 and 65535");
      return;
    }
    const char* error = nullptr;
    if (!pumpStartManual((uint16_t)seconds, &error)) {
      sendJsonError(409, error != nullptr ? error : "the pump did not start");
      return;
    }
  }

  // Stops any run immediately, automatic or manual
  if (server.hasArg("stop") && server.arg("stop") == "1") {
    pumpStop();
  }

  sendPumpJson(200);
}

// Answers any unknown route with a short text
static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ------------------------------------------------------------ public API ---

// Registers every route and starts the server
void webBegin() {
  server.on("/", HTTP_GET, handleLegacyPage); // Old page, kept until the dashboard exists
  server.on("/api/readings", HTTP_GET, handleReadings);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/pump", HTTP_GET, handlePumpGet);
  server.on("/api/pump", HTTP_POST, handlePumpPost);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Server started");
}

// Handles any client that is talking to the server at that moment
void webLoop() {
  server.handleClient();
}
