// Mock of the smart garden web API, so the dashboard can be previewed in a
// browser on a PC without an ESP32. Plain Node.js (v18 or newer), only built in
// modules. The JSON shapes, validation texts and pump rules copy the firmware.

const http = require('http');
const fs = require('fs');
const path = require('path');

// dashboard.h is read on every request to "/" and the path is relative to this
// script, so the server works from any current directory
const DASHBOARD_PATH = path.join(__dirname, '..', '..', 'firmware', 'esp32_smart_garden', 'dashboard.h');

const HISTORY_CAPACITY = 180; // Samples kept, same as historyCapacity in config.h
const HISTORY_INTERVAL_S = 60; // Reported interval, fixed even in --fast mode
const CONTRACT_ROUTES = ['/api/readings', '/api/history', '/api/settings', '/api/pump'];

// ---------------------------------------------------------------- helpers ---

function clamp(value, low, high) {
  return Math.min(high, Math.max(low, value));
}

// Reads dashboard.h and returns the text between R"rawliteral( and )rawliteral"
function readDashboard() {
  let content;
  try {
    content = fs.readFileSync(DASHBOARD_PATH, 'utf8');
  } catch (error) {
    return { exists: false, html: null, terminatorCount: 0 };
  }

  const terminators = content.split(')rawliteral"').length - 1;
  const match = content.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/);
  return { exists: true, html: match ? match[1] : null, terminatorCount: terminators };
}

// Defaults from config.h and the settings section of TASKS.md
function defaultSettings() {
  return {
    soilDry: 30,
    soilWet: 45,
    tempMin: 18,
    tempMax: 26,
    humMin: 50,
    humMax: 70,
    maxPumpSeconds: 30,
    pauseSeconds: 60,
    dailySummary: false,
    summaryHour: 20
  };
}

// Copies the defaults into the live settings object
function applyDefaults(settings) {
  Object.assign(settings, defaultSettings());
}

// Validation copied from settingsCheck in settings.cpp
function settingsCheck(s) {
  if (Number.isNaN(s.soilDry) || Number.isNaN(s.soilWet)) {
    return 'soil_dry and soil_wet must be numbers';
  }
  if (Number.isNaN(s.tempMin) || Number.isNaN(s.tempMax)) {
    return 'temp_min and temp_max must be numbers';
  }
  if (Number.isNaN(s.humMin) || Number.isNaN(s.humMax)) {
    return 'hum_min and hum_max must be numbers';
  }
  if (s.soilDry < 0 || s.soilDry > 100 || s.soilWet < 0 || s.soilWet > 100) {
    return 'soil_dry and soil_wet must be between 0 and 100';
  }
  if (s.soilDry + 5 > s.soilWet) {
    return 'soil_dry must be at least 5 below soil_wet';
  }
  if (s.tempMin < -10 || s.tempMin > 60 || s.tempMax < -10 || s.tempMax > 60) {
    return 'temp_min and temp_max must be between -10 and 60';
  }
  if (s.tempMin + 2 > s.tempMax) {
    return 'temp_min must be at least 2 below temp_max';
  }
  if (s.humMin < 0 || s.humMin > 100 || s.humMax < 0 || s.humMax > 100) {
    return 'hum_min and hum_max must be between 0 and 100';
  }
  if (s.humMin + 5 > s.humMax) {
    return 'hum_min must be at least 5 below hum_max';
  }
  if (s.maxPumpSeconds < 5 || s.maxPumpSeconds > 120) {
    return 'max_pump_s must be between 5 and 120';
  }
  if (s.pauseSeconds < 10 || s.pauseSeconds > 600) {
    return 'pause_s must be between 10 and 600';
  }
  if (s.summaryHour > 23) {
    return 'summary_hour must be between 0 and 23';
  }
  return null;
}

// Accepts only a full valid number, like strtod with a full string check
function parseFloatStrict(text) {
  if (typeof text !== 'string' || text.trim().length === 0) {
    return null;
  }
  if (!/^\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?\s*$/.test(text)) {
    return null;
  }
  const value = Number(text);
  return Number.isFinite(value) ? value : null;
}

// Accepts only a full valid whole number, like strtol with a full string check
function parseLongStrict(text) {
  if (typeof text !== 'string' || !/^\s*[+-]?\d+\s*$/.test(text)) {
    return null;
  }
  return Number(text);
}

// ------------------------------------------------------------- formatting ---

function number1(value) {
  return value === null || value === undefined || Number.isNaN(value) ? 'null' : value.toFixed(1);
}

function pumpObject(pump) {
  return {
    running: pump.running,
    mode: pump.mode,
    manual_remaining_s: pump.manualRemainingSeconds,
    blocked_remaining_s: pump.blockedRemainingSeconds,
    reason: pump.reason
  };
}

function readingsJson(state) {
  const r = state.readings;
  const pump = pumpObject(state.pumpStatus());
  const parts = [
    '"uptime_s":' + state.uptimeSeconds(),
    '"wifi_rssi":' + state.rssi,
    '"ip":"' + state.ip + '"',
    '"heap_free":' + state.heapFree,
    '"heap_min":' + state.heapMin,
    '"temperature":' + (r.dhtValid ? number1(r.temperature) : 'null'),
    '"humidity":' + (r.dhtValid ? number1(r.humidity) : 'null'),
    '"dht_ok":' + (r.dhtValid ? 'true' : 'false'),
    '"light":' + number1(r.light),
    '"soil":' + number1(r.soil),
    '"soil_ok":' + (r.soilValid ? 'true' : 'false'),
    '"tank_empty":' + (r.tankEmpty ? 'true' : 'false')
  ];
  return '{' + parts.join(',') + ',"pump":' + JSON.stringify(pump) + '}';
}

function historyJson(state) {
  const series = { temperature: [], humidity: [], soil: [], light: [], pump: [] };
  for (const sample of state.history) {
    series.temperature.push(sample.temperature === null ? 'null' : sample.temperature.toFixed(1));
    series.humidity.push(sample.humidity === null ? 'null' : sample.humidity.toFixed(1));
    series.soil.push(sample.soil === null ? 'null' : sample.soil.toFixed(1));
    series.light.push(String(sample.light));
    series.pump.push(String(sample.pump));
  }
  const parts = [
    '"interval_s":' + HISTORY_INTERVAL_S,
    '"count":' + state.history.length,
    '"temperature":[' + series.temperature.join(',') + ']',
    '"humidity":[' + series.humidity.join(',') + ']',
    '"soil":[' + series.soil.join(',') + ']',
    '"light":[' + series.light.join(',') + ']',
    '"pump":[' + series.pump.join(',') + ']'
  ];
  return '{' + parts.join(',') + '}';
}

function settingsJson(settings) {
  return JSON.stringify({
    soil_dry: settings.soilDry,
    soil_wet: settings.soilWet,
    temp_min: settings.tempMin,
    temp_max: settings.tempMax,
    hum_min: settings.humMin,
    hum_max: settings.humMax,
    max_pump_s: settings.maxPumpSeconds,
    pause_s: settings.pauseSeconds,
    daily_summary: settings.dailySummary,
    summary_hour: settings.summaryHour
  });
}

// ------------------------------------------------------------------ device ---

// Simulated wall clock for the preview day, in seconds since midnight. The
// preview starts mid afternoon, so the default scenario is a bright garden and
// the light stays high for the first screenshots.
const SIM_START_SECONDS = 14 * 3600 + 30 * 60;

// Sunrise and sunset for the light arc, in seconds since midnight
const SUNRISE_SECONDS = 6 * 3600 + 30 * 60;
const SUNSET_SECONDS = 21 * 3600;

// Small mulberry32 pseudo random generator, so the noise is the same on every
// run. Returns a number in [0, 1).
function pseudoRandom(seed) {
  let t = (seed + 0x6D2B79F5) | 0;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

// Light for one moment of the simulated day: a smooth arc between sunrise and
// sunset, with a few short cloud dips that stay the same on every run.
function lightFor(dayPosition) {
  if (dayPosition < SUNRISE_SECONDS || dayPosition > SUNSET_SECONDS) {
    return 0;
  }
  const arc = Math.sin((Math.PI * (dayPosition - SUNRISE_SECONDS)) /
    (SUNSET_SECONDS - SUNRISE_SECONDS));
  let light = 85 * arc;
  const cloud = pseudoRandom(Math.floor(dayPosition / 60));
  if (cloud > 0.97) {
    light -= 8 + 250 * (cloud - 0.97);
  }
  return clamp(light, 0, 100);
}

// Environment generator: a slow day curve for the light and gentle drifts for
// temperature and air humidity, driven by the simulated clock. The alert
// scenario pins its own live values.
function environmentFor(scenario, simSeconds) {
  const dayPosition = ((simSeconds % 86400) + 86400) % 86400;
  const light = lightFor(dayPosition);

  if (scenario === 'alert') {
    return { temperature: 29, humidity: 41, light };
  }

  // Slow daily drift: temperature climbs from 23.0 to 24.4 across the three
  // hours before 14:30, air humidity moves the other way.
  const drift = Math.sin((2 * Math.PI * (dayPosition - 46800)) / 86400);
  const seed = Math.floor(simSeconds / 60);
  return {
    temperature: 23.7 + 1.8 * drift + 0.2 * (pseudoRandom(seed) - 0.5),
    humidity: 59 - 7.84 * drift + 0.4 * (pseudoRandom(seed + 7919) - 0.5),
    light
  };
}

// Creates the simulated device and its HTTP server
function createMockServer(options = {}) {
  const scenario = options.scenario === 'alert' ? 'alert' : 'normal';
  const fast = Boolean(options.fast);
  const processStart = Date.now();

  const settings = defaultSettings();
  const readings = {
    soil: scenario === 'alert' ? 22 : 45,
    soilValid: true,
    temperature: 0,
    humidity: 0,
    dhtValid: true,
    light: 0,
    tankEmpty: scenario === 'alert'
  };
  const pump = {
    running: false,
    mode: 'auto',
    manualRun: false,
    manualRunSeconds: 0,
    startTime: 0,
    blockedUntil: 0,
    reason: 'idle'
  };
  const history = [];
  const state = {
    readings,
    history,
    rssi: -58,
    ip: '192.168.1.50',
    heapFree: 205000,
    heapMin: 187000,
    uptimeSeconds: () => Math.floor((Date.now() - processStart) / 1000),
    pumpStatus
  };

  let simTimer = null;
  let historyTimer = null;

  function uptimeMs() {
    return Date.now() - processStart;
  }

  // Turns the pump off and records the reason, like stopRun in pump.cpp
  function stopRun(reason) {
    pump.running = false;
    pump.manualRun = false;
    pump.manualRunSeconds = 0;
    pump.reason = reason;
  }

  // Same state machine as pumpUpdate in pump.cpp
  function pumpUpdate() {
    if (!pump.running) {
      if (pump.mode === 'auto' && readings.soilValid && readings.soil < settings.soilDry &&
          !readings.tankEmpty && uptimeMs() - pump.blockedUntil >= 0) {
        pump.running = true;
        pump.manualRun = false;
        pump.startTime = uptimeMs();
        pump.reason = 'soil dry';
      }
      return;
    }

    if (readings.tankEmpty) {
      stopRun('tank empty');
      return;
    }
    if (!pump.manualRun && !readings.soilValid) {
      stopRun('soil sensor invalid');
      return;
    }
    if (!pump.manualRun && readings.soil > settings.soilWet) {
      stopRun('soil wet enough');
      return;
    }
    if (pump.manualRun && uptimeMs() - pump.startTime >= pump.manualRunSeconds * 1000) {
      stopRun('manual run finished');
      return;
    }
    if (uptimeMs() - pump.startTime >= settings.maxPumpSeconds * 1000) {
      const automatic = !pump.manualRun;
      stopRun('max run time');
      if (automatic) {
        pump.blockedUntil = uptimeMs() + settings.pauseSeconds * 1000;
      }
    }
  }

  // Same refusal rules and error texts as pumpStartManual in pump.cpp
  function pumpStartManual(seconds) {
    if (readings.tankEmpty) {
      return { ok: false, error: 'tank is empty' };
    }
    if (seconds === 0) {
      return { ok: false, error: 'seconds must be at least 1' };
    }
    if (seconds > settings.maxPumpSeconds) {
      return { ok: false, error: 'run time above the maximum of ' + settings.maxPumpSeconds + ' s' };
    }
    pump.running = true;
    pump.manualRun = true;
    pump.manualRunSeconds = seconds;
    pump.startTime = uptimeMs();
    pump.reason = 'manual run';
    return { ok: true };
  }

  // Same behaviour as pumpSetMode in pump.cpp
  function pumpSetMode(mode) {
    if (mode === 'manual' && pump.running && !pump.manualRun) {
      stopRun('stopped');
    }
    pump.mode = mode;
  }

  function pumpStop() {
    if (!pump.running) {
      return;
    }
    stopRun('stopped');
  }

  // Remaining seconds computed from the clock, like pumpStatus in pump.cpp
  function pumpStatus() {
    let manualRemainingSeconds = 0;
    if (pump.running && pump.manualRun) {
      const total = pump.manualRunSeconds * 1000;
      const elapsed = uptimeMs() - pump.startTime;
      if (elapsed < total) {
        manualRemainingSeconds = Math.ceil((total - elapsed) / 1000);
      }
    }
    let blockedRemainingSeconds = 0;
    if (pump.blockedUntil - uptimeMs() > 0) {
      blockedRemainingSeconds = Math.ceil((pump.blockedUntil - uptimeMs()) / 1000);
    }
    return {
      running: pump.running,
      mode: pump.mode,
      manualRemainingSeconds,
      blockedRemainingSeconds,
      reason: pump.reason
    };
  }

  // A little noise on the heap numbers, so the dashboard sees them move
  function refreshHeap() {
    state.heapFree = 205000 + Math.round(Math.random() * 800) - 400;
    state.heapMin = 187000 + Math.round(Math.random() * 600) - 300;
  }

  // One control cycle per second, like the sketch main loop
  function tick() {
    const env = environmentFor(scenario, SIM_START_SECONDS + state.uptimeSeconds());
    readings.temperature = env.temperature;
    readings.humidity = env.humidity;
    readings.light = env.light;

    if (pump.running) {
      readings.soil = clamp(readings.soil + 2, 0, 100);
    } else {
      readings.soil = clamp(readings.soil - 0.02, 0, 100);
    }
    readings.soilValid = readings.soil >= 0 && readings.soil <= 100;
    readings.tankEmpty = scenario === 'alert';

    refreshHeap();
    pumpUpdate();
  }

  // Adds the current readings to the history ring
  function addHistorySample() {
    history.push({
      temperature: readings.dhtValid ? readings.temperature : null,
      humidity: readings.dhtValid ? readings.humidity : null,
      soil: readings.soilValid ? readings.soil : null,
      light: Math.round(readings.light),
      pump: pump.running ? 1 : 0
    });
    if (history.length > HISTORY_CAPACITY) {
      history.shift();
    }
  }

  // Soil saw tooth for the normal history: the soil dries by about 0.12 to
  // 0.20 each sample, the pump runs for three samples and refills it, then it
  // dries again. The last sample lands on the value the live readings start
  // from, so the live soil continues the same curve.
  function soilSawTooth() {
    const dryMean = 0.16;
    const pumpRise = 12;
    const pumpSamples = 3;
    const trough = 31; // Pump starts just below the default soil_dry of 30
    const lastValue = 52;

    const steps = [];
    for (let i = 0; i < HISTORY_CAPACITY; i++) {
      steps.push(dryMean + 0.08 * (pseudoRandom(1000 + i) - 0.5));
    }

    function simulate(start) {
      const values = [];
      const flags = [];
      let soil = start;
      let pumpLeft = 0;
      for (let i = 0; i < HISTORY_CAPACITY; i++) {
        if (pumpLeft > 0) {
          soil += pumpRise;
          flags.push(1);
          pumpLeft--;
        } else {
          soil -= steps[i];
          flags.push(0);
          if (soil < trough) {
            pumpLeft = pumpSamples;
          }
        }
        values.push(soil);
      }
      return { values, flags };
    }

    // Two passes: the first learns the total drying, the second starts high
    // enough that the last sample is lastValue.
    let run = simulate(50);
    run = simulate(50 + (lastValue - run.values[HISTORY_CAPACITY - 1]));
    return run;
  }

  // Soil for the alert history: no pump because the tank is empty, the soil
  // just dries from a healthy value down to the alert reading.
  function soilAlert() {
    const values = [];
    const flags = [];
    for (let i = 0; i < HISTORY_CAPACITY; i++) {
      const progress = i / (HISTORY_CAPACITY - 1);
      values.push(45 - 23 * progress + 0.2 * (pseudoRandom(4000 + i) - 0.5));
      flags.push(0);
    }
    return { values, flags };
  }

  // Fills the ring with a plausible past of the last three hours. It always
  // starts from the fixed preview clock, so every run draws the same picture.
  function prefillHistory() {
    const soilRun = scenario === 'alert' ? soilAlert() : soilSawTooth();

    for (let i = 0; i < HISTORY_CAPACITY; i++) {
      const simSeconds = SIM_START_SECONDS +
        (i - (HISTORY_CAPACITY - 1)) * HISTORY_INTERVAL_S;
      const env = environmentFor(scenario, simSeconds);
      const progress = i / (HISTORY_CAPACITY - 1);

      let temperature = env.temperature;
      let humidity = env.humidity;
      if (scenario === 'alert') {
        // Temperature climbs to the alert value near the end of the window
        temperature = 26.5 + 2.5 * progress * progress +
          0.2 * (pseudoRandom(2000 + i) - 0.5);
        humidity = 44 - 3 * progress + 0.4 * (pseudoRandom(3000 + i) - 0.5);
      }

      history.push({
        temperature,
        humidity,
        soil: soilRun.values[i],
        light: Math.round(env.light),
        pump: soilRun.flags[i]
      });
    }

    // The live readings continue smoothly from the last history sample
    readings.soil = soilRun.values[HISTORY_CAPACITY - 1];
  }

  // ----------------------------------------------------------- http layer ---

  function send(res, code, contentType, body) {
    res.writeHead(code, {
      'Content-Type': contentType,
      'Cache-Control': 'no-store',
      'Content-Length': Buffer.byteLength(body)
    });
    res.end(body);
  }

  function sendJson(res, code, body) {
    send(res, code, 'application/json', body);
  }

  function sendError(res, code, text) {
    sendJson(res, code, JSON.stringify({ error: text }));
  }

  function readBody(req) {
    return new Promise((resolve) => {
      const chunks = [];
      req.on('data', (chunk) => chunks.push(chunk));
      req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    });
  }

  // Query args first, then body args, like the Arduino WebServer collects them
  function collectArgs(url, body) {
    const args = [];
    for (const [name, value] of url.searchParams) {
      args.push([name, value]);
    }
    if (body) {
      for (const [name, value] of new URLSearchParams(body)) {
        args.push([name, value]);
      }
    }
    return args;
  }

  function findArg(args, name) {
    for (const [key, value] of args) {
      if (key === name) {
        return value;
      }
    }
    return null;
  }

  function hasArg(args, name) {
    return args.some(([key]) => key === name);
  }

  // POST /api/settings, same rules and error texts as handleSettingsPost
  function handleSettingsPost(res, args) {
    if (findArg(args, 'reset') === '1') {
      applyDefaults(settings);
      sendJson(res, 200, settingsJson(settings));
      return;
    }

    const next = Object.assign({}, settings);

    for (const [name, value] of args) {
      if (name === 'soil_dry' || name === 'soil_wet' || name === 'temp_min' ||
          name === 'temp_max' || name === 'hum_min' || name === 'hum_max') {
        const number = parseFloatStrict(value);
        if (number === null) {
          sendError(res, 400, name + ' is not a number');
          return;
        }
        if (name === 'soil_dry') next.soilDry = number;
        else if (name === 'soil_wet') next.soilWet = number;
        else if (name === 'temp_min') next.tempMin = number;
        else if (name === 'temp_max') next.tempMax = number;
        else if (name === 'hum_min') next.humMin = number;
        else next.humMax = number;
      } else if (name === 'max_pump_s' || name === 'pause_s' || name === 'summary_hour') {
        const integer = parseLongStrict(value);
        if (integer === null) {
          sendError(res, 400, name + ' is not a number');
          return;
        }
        if (integer < 0 || integer > 65535) {
          sendError(res, 400, name + ' is out of range');
          return;
        }
        if (name === 'max_pump_s') next.maxPumpSeconds = integer;
        else if (name === 'pause_s') next.pauseSeconds = integer;
        else next.summaryHour = integer;
      } else if (name === 'daily_summary') {
        if (value === '0') next.dailySummary = false;
        else if (value === '1') next.dailySummary = true;
        else {
          sendError(res, 400, 'daily_summary must be 0 or 1');
          return;
        }
      }
      // Any unknown field is ignored
    }

    const error = settingsCheck(next);
    if (error !== null) {
      sendError(res, 400, error);
      return;
    }
    Object.assign(settings, next);
    sendJson(res, 200, settingsJson(settings));
  }

  // POST /api/pump, mode then run then stop, like handlePumpPost
  function handlePumpPost(res, args) {
    if (hasArg(args, 'mode')) {
      const mode = findArg(args, 'mode');
      if (mode === 'auto' || mode === 'manual') {
        pumpSetMode(mode);
      } else {
        sendError(res, 400, 'mode must be auto or manual');
        return;
      }
    }

    if (hasArg(args, 'run')) {
      const seconds = parseLongStrict(findArg(args, 'run'));
      if (seconds === null || seconds < 0 || seconds > 65535) {
        sendError(res, 400, 'run must be a number of seconds between 0 and 65535');
        return;
      }
      const result = pumpStartManual(seconds);
      if (!result.ok) {
        sendError(res, 409, result.error);
        return;
      }
    }

    if (findArg(args, 'stop') === '1') {
      pumpStop();
    }

    sendJson(res, 200, JSON.stringify(pumpObject(pumpStatus())));
  }

  async function handler(req, res) {
    const url = new URL(req.url, 'http://localhost');
    const route = url.pathname;
    let body = null;
    if (req.method === 'POST') {
      body = await readBody(req);
    }
    const args = collectArgs(url, body);

    if (req.method === 'GET' && route === '/') {
      const dashboard = readDashboard();
      if (!dashboard.exists || dashboard.html === null) {
        send(res, 503, 'text/plain', 'dashboard.h is not available yet');
        return;
      }
      send(res, 200, 'text/html', dashboard.html);
      return;
    }

    if (req.method === 'GET' && route === '/api/readings') {
      sendJson(res, 200, readingsJson(state));
      return;
    }

    if (req.method === 'GET' && route === '/api/history') {
      sendJson(res, 200, historyJson(state));
      return;
    }

    if (req.method === 'GET' && route === '/api/settings') {
      sendJson(res, 200, settingsJson(settings));
      return;
    }

    if (req.method === 'POST' && route === '/api/settings') {
      handleSettingsPost(res, args);
      return;
    }

    if (req.method === 'GET' && route === '/api/pump') {
      sendJson(res, 200, JSON.stringify(pumpObject(pumpStatus())));
      return;
    }

    if (req.method === 'POST' && route === '/api/pump') {
      handlePumpPost(res, args);
      return;
    }

    send(res, 404, 'text/plain', 'Not found');
  }

  const server = http.createServer((req, res) => {
    handler(req, res).catch(() => {
      sendError(res, 500, 'internal error');
    });
  });

  // Starts the timers and the listener, resolves with the real port
  function start(port = 0) {
    return new Promise((resolve, reject) => {
      prefillHistory();
      tick();
      simTimer = setInterval(tick, 1000);
      historyTimer = setInterval(addHistorySample, fast ? 2000 : HISTORY_INTERVAL_S * 1000);
      server.once('error', reject);
      server.listen(port, () => resolve(server.address().port));
    });
  }

  function stop() {
    clearInterval(simTimer);
    clearInterval(historyTimer);
    return new Promise((resolve) => server.close(resolve));
  }

  return { server, start, stop, scenario, state, settings };
}

// ------------------------------------------------------------- cli start ---

function parseArgs(argv) {
  const options = { port: 8080, scenario: 'normal', fast: false };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--port' && argv[i + 1]) {
      options.port = Number(argv[i + 1]);
      i++;
    } else if (argv[i] === '--scenario' && argv[i + 1]) {
      options.scenario = argv[i + 1];
      i++;
    } else if (argv[i] === '--fast') {
      options.fast = true;
    }
  }
  return options;
}

if (require.main === module) {
  const options = parseArgs(process.argv.slice(2));
  const mock = createMockServer(options);
  mock.start(options.port).then((port) => {
    console.log('Smart garden preview on http://localhost:' + port +
      ' (scenario ' + mock.scenario + (options.fast ? ', fast history' : '') + ')');
  }).catch((error) => {
    console.error('Could not start the preview server: ' + error.message);
    process.exit(1);
  });
}

module.exports = { createMockServer, readDashboard, DASHBOARD_PATH, CONTRACT_ROUTES };
