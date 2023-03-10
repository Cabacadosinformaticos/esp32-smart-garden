// Checks the mock API and the dashboard page without a browser. Starts the mock
// server from server.js on a free port, runs every check once and prints one
// line per check plus a summary. Exit code 1 when any check failed.

const vm = require('vm');
const { createMockServer, readDashboard } = require('./server');

const CONTRACT_ROUTES = ['/api/readings', '/api/history', '/api/settings', '/api/pump'];

let passed = 0;
let failed = 0;
let skipped = 0;

function ok(name) {
  passed++;
  console.log('ok   - ' + name);
}

function fail(name, detail) {
  failed++;
  console.log('FAIL - ' + name + (detail ? ' (' + detail + ')' : ''));
}

function skip(name, reason) {
  skipped++;
  console.log('SKIP - ' + name + ' (' + reason + ')');
}

// --------------------------------------------------------- http helpers ---

async function request(base, route, options = {}) {
  const response = await fetch(base + route, options);
  const text = await response.text();
  let json = null;
  try {
    json = JSON.parse(text);
  } catch (error) {
    json = null;
  }
  return { status: response.status, text, json, headers: response.headers };
}

function formBody(fields) {
  return {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams(fields).toString()
  };
}

// ------------------------------------------------------ dashboard checks ---

function checkDashboard() {
  const dashboard = readDashboard();

  if (!dashboard.exists) {
    skip('dashboard extracted', 'dashboard.h does not exist yet');
    skip('dashboard size below 15000 bytes', 'dashboard.h does not exist yet');
    skip('no second )rawliteral" terminator', 'dashboard.h does not exist yet');
    skip('page script blocks parse', 'dashboard.h does not exist yet');
    skip('element ids referenced by the script exist', 'dashboard.h does not exist yet');
    skip('page api urls match the contract', 'dashboard.h does not exist yet');
    return;
  }

  if (dashboard.html === null) {
    fail('dashboard extracted', 'R"rawliteral( ... )rawliteral" not found');
    skip('dashboard size below 15000 bytes', 'no extracted dashboard');
    skip('no second )rawliteral" terminator', 'no extracted dashboard');
    skip('page script blocks parse', 'no extracted dashboard');
    skip('element ids referenced by the script exist', 'no extracted dashboard');
    skip('page api urls match the contract', 'no extracted dashboard');
    return;
  }

  const html = dashboard.html;
  ok('dashboard extracted (' + html.length + ' bytes)');

  const byteLength = Buffer.byteLength(html, 'utf8');
  if (byteLength < 15000) {
    ok('dashboard size below 15000 bytes (' + byteLength + ')');
  } else {
    fail('dashboard size below 15000 bytes', byteLength + ' bytes');
  }

  if (dashboard.terminatorCount === 1) {
    ok('no second )rawliteral" terminator');
  } else {
    fail('no second )rawliteral" terminator', dashboard.terminatorCount + ' occurrences');
  }

  checkScripts(html);
  checkElementIds(html);
  checkApiUrls(html);
}

// Parses every inline script block with the vm module
function checkScripts(html) {
  const blocks = [];
  const pattern = /<script\b([^>]*)>([\s\S]*?)<\/script>/gi;
  let match;
  while ((match = pattern.exec(html)) !== null) {
    const attributes = match[1];
    const code = match[2];
    if (/\bsrc\s*=/.test(attributes) || code.trim().length === 0) {
      continue;
    }
    blocks.push(code);
  }

  if (blocks.length === 0) {
    fail('page script blocks parse', 'no inline script block found');
    return;
  }

  for (let i = 0; i < blocks.length; i++) {
    try {
      new vm.Script(blocks[i]);
    } catch (error) {
      fail('page script blocks parse', 'block ' + (i + 1) + ': ' + error.message);
      return;
    }
  }
  ok('page script blocks parse (' + blocks.length + ' blocks)');
}

// Best effort check that every referenced element id exists in the HTML
function checkElementIds(html) {
  const ids = new Set();
  const patterns = [
    /getElementById\(\s*['"]([A-Za-z0-9_-]+)['"]\s*\)/g,
    /\$\(\s*['"]([A-Za-z0-9_-]+)['"]\s*\)/g,
    /\bel\(\s*['"]([A-Za-z0-9_-]+)['"]\s*\)/g
  ];
  for (const pattern of patterns) {
    let match;
    while ((match = pattern.exec(html)) !== null) {
      ids.add(match[1]);
    }
  }

  const missing = [];
  for (const id of ids) {
    const present = html.includes('id="' + id + '"') ||
      html.includes("id='" + id + "'") ||
      new RegExp('id\\s*=\\s*["\']?' + id + '["\'\\s>]').test(html);
    if (!present) {
      missing.push(id);
    }
  }

  if (missing.length === 0) {
    ok('element ids referenced by the script exist (' + ids.size + ' ids)');
  } else {
    fail('element ids referenced by the script exist', 'missing: ' + missing.join(', '));
  }
}

// Every /api/... url mentioned in the page must be a contract route
function checkApiUrls(html) {
  const found = new Set();
  const pattern = /\/api\/[A-Za-z0-9_\/-]*/g;
  let match;
  while ((match = pattern.exec(html)) !== null) {
    found.add(match[0].replace(/\/$/, ''));
  }

  const unknown = [];
  for (const url of found) {
    if (!CONTRACT_ROUTES.includes(url)) {
      unknown.push(url);
    }
  }

  if (unknown.length === 0) {
    ok('page api urls match the contract (' + found.size + ' urls)');
  } else {
    fail('page api urls match the contract', 'unknown: ' + unknown.join(', '));
  }
}

// ------------------------------------------------------------ api checks ---

function isNumber(value) {
  return typeof value === 'number' && Number.isFinite(value);
}

async function checkReadings(base) {
  const response = await request(base, '/api/readings');
  const json = response.json;

  if (response.status !== 200 || json === null) {
    fail('GET /api/readings fields and types', 'status ' + response.status);
    return;
  }

  const problems = [];
  if (!isNumber(json.uptime_s)) problems.push('uptime_s');
  if (!isNumber(json.wifi_rssi)) problems.push('wifi_rssi');
  if (typeof json.ip !== 'string') problems.push('ip');
  if (!isNumber(json.heap_free)) problems.push('heap_free');
  if (!isNumber(json.heap_min)) problems.push('heap_min');
  if (!(json.temperature === null || isNumber(json.temperature))) problems.push('temperature');
  if (!(json.humidity === null || isNumber(json.humidity))) problems.push('humidity');
  if (typeof json.dht_ok !== 'boolean') problems.push('dht_ok');
  if (!isNumber(json.light)) problems.push('light');
  if (!isNumber(json.soil)) problems.push('soil');
  if (typeof json.soil_ok !== 'boolean') problems.push('soil_ok');
  if (typeof json.tank_empty !== 'boolean') problems.push('tank_empty');

  const pump = json.pump;
  if (typeof pump !== 'object' || pump === null) {
    problems.push('pump');
  } else {
    if (typeof pump.running !== 'boolean') problems.push('pump.running');
    if (pump.mode !== 'auto' && pump.mode !== 'manual') problems.push('pump.mode');
    if (!isNumber(pump.manual_remaining_s)) problems.push('pump.manual_remaining_s');
    if (!isNumber(pump.blocked_remaining_s)) problems.push('pump.blocked_remaining_s');
    if (typeof pump.reason !== 'string') problems.push('pump.reason');
  }

  if (problems.length === 0) {
    ok('GET /api/readings fields and types');
  } else {
    fail('GET /api/readings fields and types', 'bad: ' + problems.join(', '));
  }

  if (response.headers.get('cache-control') === 'no-store') {
    ok('readings send Cache-Control no-store');
  } else {
    fail('readings send Cache-Control no-store', String(response.headers.get('cache-control')));
  }
}

async function checkHistory(base) {
  const response = await request(base, '/api/history');
  const json = response.json;

  if (response.status !== 200 || json === null) {
    fail('GET /api/history series lengths', 'status ' + response.status);
    return;
  }

  if (!isNumber(json.count) || !isNumber(json.interval_s)) {
    fail('GET /api/history series lengths', 'count or interval_s missing');
    return;
  }

  const names = ['temperature', 'humidity', 'soil', 'light', 'pump'];
  const problems = [];
  for (const name of names) {
    if (!Array.isArray(json[name])) {
      problems.push(name + ' is not an array');
    } else if (json[name].length !== json.count) {
      problems.push(name + ' has ' + json[name].length + ' of ' + json.count);
    }
  }

  if (json.count !== 180) {
    problems.push('count is ' + json.count + ', expected 180');
  }
  if (json.interval_s !== 60) {
    problems.push('interval_s is ' + json.interval_s + ', expected 60');
  }

  if (problems.length === 0) {
    ok('GET /api/history series lengths (' + json.count + ' samples)');
  } else {
    fail('GET /api/history series lengths', problems.join(', '));
  }
}

async function checkSettings(base) {
  const defaults = await request(base, '/api/settings');
  const expected = {
    soil_dry: 30, soil_wet: 70, temp_min: 18, temp_max: 26, hum_min: 50, hum_max: 70,
    max_pump_s: 30, pause_s: 60, daily_summary: false, summary_hour: 20
  };
  let good = defaults.status === 200 && defaults.json !== null;
  if (good) {
    for (const [key, value] of Object.entries(expected)) {
      if (defaults.json[key] !== value) {
        good = false;
      }
    }
  }
  if (good) {
    ok('GET /api/settings returns the defaults');
  } else {
    fail('GET /api/settings returns the defaults');
  }

  const valid = await request(base, '/api/settings', formBody({ soil_dry: 40 }));
  const afterValid = await request(base, '/api/settings');
  if (valid.status === 200 && afterValid.json && afterValid.json.soil_dry === 40) {
    ok('POST /api/settings applies a valid change');
  } else {
    fail('POST /api/settings applies a valid change', 'status ' + valid.status);
  }

  const rejected = await request(base, '/api/settings', formBody({ soil_dry: 90 }));
  const afterRejected = await request(base, '/api/settings');
  if (rejected.status === 400 && afterRejected.json && afterRejected.json.soil_dry === 40) {
    ok('POST /api/settings rejects soil_dry above soil_wet with 400');
  } else {
    fail('POST /api/settings rejects soil_dry above soil_wet with 400', 'status ' + rejected.status);
  }

  const notNumber = await request(base, '/api/settings', formBody({ soil_dry: 'abc' }));
  if (notNumber.status === 400) {
    ok('POST /api/settings rejects soil_dry=abc with 400');
  } else {
    fail('POST /api/settings rejects soil_dry=abc with 400', 'status ' + notNumber.status);
  }

  const reset = await request(base, '/api/settings', formBody({ reset: 1 }));
  const afterReset = await request(base, '/api/settings');
  let resetGood = reset.status === 200 && afterReset.json !== null;
  if (resetGood) {
    for (const [key, value] of Object.entries(expected)) {
      if (afterReset.json[key] !== value) {
        resetGood = false;
      }
    }
  }
  if (resetGood) {
    ok('POST /api/settings reset=1 restores the defaults');
  } else {
    fail('POST /api/settings reset=1 restores the defaults');
  }
}

async function checkPump(base) {
  const manual = await request(base, '/api/pump', formBody({ mode: 'manual' }));
  const run = await request(base, '/api/pump', formBody({ run: 5 }));
  if (manual.status === 200 && run.status === 200 && run.json &&
      run.json.running === true && run.json.manual_remaining_s > 0) {
    ok('POST /api/pump mode=manual and run=5 start a manual run');
  } else {
    fail('POST /api/pump mode=manual and run=5 start a manual run', 'status ' + run.status);
  }

  const tooLong = await request(base, '/api/pump', formBody({ run: 999 }));
  if (tooLong.status === 409) {
    ok('POST /api/pump run=999 is refused with 409');
  } else {
    fail('POST /api/pump run=999 is refused with 409', 'status ' + tooLong.status);
  }

  const stop = await request(base, '/api/pump', formBody({ stop: 1 }));
  if (stop.status === 200 && stop.json && stop.json.running === false) {
    ok('POST /api/pump stop=1 stops the pump');
  } else {
    fail('POST /api/pump stop=1 stops the pump', 'status ' + stop.status);
  }

  const badMode = await request(base, '/api/pump', formBody({ mode: 'bogus' }));
  if (badMode.status === 400) {
    ok('POST /api/pump mode=bogus is refused with 400');
  } else {
    fail('POST /api/pump mode=bogus is refused with 400', 'status ' + badMode.status);
  }
}

async function checkAlertScenario() {
  const mock = createMockServer({ scenario: 'alert' });
  const port = await mock.start(0);
  const base = 'http://127.0.0.1:' + port;
  try {
    const run = await request(base, '/api/pump', formBody({ mode: 'manual', run: 5 }));
    const error = run.json && run.json.error;
    if (run.status === 409 && error === 'tank is empty') {
      ok('alert scenario refuses run=5 with 409 tank is empty');
    } else {
      fail('alert scenario refuses run=5 with 409 tank is empty', 'status ' + run.status);
    }
  } finally {
    await mock.stop();
  }
}

async function main() {
  console.log('Smart garden preview checks');
  console.log('');

  checkDashboard();

  const mock = createMockServer({ scenario: 'normal' });
  const port = await mock.start(0);
  const base = 'http://127.0.0.1:' + port;
  try {
    await checkReadings(base);
    await checkHistory(base);
    await checkSettings(base);
    await checkPump(base);
  } finally {
    await mock.stop();
  }

  await checkAlertScenario();

  console.log('');
  console.log('Summary: ' + passed + ' passed, ' + failed + ' failed, ' + skipped + ' skipped');
  if (failed > 0) {
    process.exit(1);
  }
}

main().catch((error) => {
  console.error('Check run failed: ' + error.message);
  process.exit(1);
});
