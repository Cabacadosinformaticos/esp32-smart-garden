# Dashboard preview (mock API)

Preview the smart garden dashboard in a browser on a PC, without an ESP32.
Plain Node.js (v18 or newer), only built in modules, no `npm install`.

## Run the preview

```
node tools/preview/server.js
```

Open http://localhost:8080. The server reads
`firmware/esp32_smart_garden/dashboard.h` on every request to `/`, so editing
the header and refreshing the page shows the change. If `dashboard.h` does not
exist yet it answers 503.

Options:

- `--port 8080` port to listen on (default 8080)
- `--scenario normal|alert` alert makes the dashboard show warnings and alerts
  (temperature 29, tank empty, soil 22, air humidity 41)
- `--fast` adds a history sample every 2 seconds instead of every 60, handy to
  watch the chart move

## Check the API contract

```
node tools/preview/check.js
```

Starts the mock server on a free port and checks the dashboard page and the
API: field types, history lengths, settings validation, the pump rules and the
alert scenario. Prints one line per check and a summary; exit code 1 when any
check fails. While `dashboard.h` does not exist yet the dashboard checks are
reported as SKIPPED.

## API endpoints

The mock follows the contract in `TASKS.md`:

- `GET /api/readings`
- `GET /api/history`
- `GET /api/settings`, `POST /api/settings`
- `GET /api/pump`, `POST /api/pump`
- `GET /` serves the dashboard

Unknown routes answer 404 "Not found".
