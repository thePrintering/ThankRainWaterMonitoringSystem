# HTTP API Reference

Base URL example: `http://<esp32-ip>`

## Overview

All endpoints are served from `webserver_config.cpp` with the following features:
- JSON responses for read operations
- Multipart file upload support via `/fm/upload`
- WebSocket channel for live data at `/ws`
- Logging with severity tags: `INFO`, `WARN`, `ERROR`
- File manager duplicate-file protection (requires `overwrite=1` to replace)

## Endpoints

| Method | Endpoint | Parameters / Body | Description |
|---|---|---|---|
| GET | `/` | None | Returns the main web page (`dashboard-index.html`). |
| GET | `/settings` | None | Returns the settings web page (`settings-index.html`). |
| GET | `/file-manager` | None | Returns the file manager web page (`file-manager-index.html`). |
| POST | `/fm/upload` | Multipart file upload, optional `dir`, optional `overwrite=1` | Uploads a file to SD card; rejects duplicates unless overwrite is explicitly requested. |
| POST | `/reboot` | None | Reboots the ESP32. |
| GET | `/download` | `file=<filename>` (query) | Downloads a file from SD card as CSV. |
| GET | `/list-files` | None | Returns the cached JSON array of files used by the file manager. |
| GET | `/list-all-files` | Optional `max=<n>` | Returns the cached storage inventory for diagnostics and debugging. |
| GET | `/get-settings` | None | Returns current firmware settings as JSON. |
| POST | `/save-settings` | JSON body | Saves settings, reinitializes time sync, and forces next measure. |
| POST | `/load-default-settings` | None | Deletes stored settings file and reboots with defaults. |
| GET | `/time-status` | None | Returns time synchronization and timezone diagnostic JSON. |
| GET | `/sd-status` | None | Returns SD card diagnostics (ready state, card type, used/free bytes, required files). |
| GET | `/measure-status` | None | Returns measurement diagnostics (sensor state, last CSV line, timing, and SD context used by the dashboard). |
| GET | `/wifi/current` | None | Returns the current configured STA SSID used by provisioning. |
| GET | `/wifi/token` | None | Returns the current provisioning session token. |
| GET | `/wifi/state` | None | Returns provisioning connection diagnostics while AP mode is active. |
| GET | `/wifi/scan` | None | Starts or polls the async WiFi scan used by the provisioning portal. |
| POST | `/wifi/provision` | JSON body | Saves STA credentials, re-enables WiFi if needed, and triggers a fresh connection attempt. |
| POST | `/wifi/ap/stop` | None | Stops the provisioning AP and closes the captive portal. |
| GET | `/logs` | None | Returns severity-tagged `log.txt` content from SD card. |
| POST | `/delete-log` | None | Deletes `log.txt` from SD card. |
| GET | `/csv` | `file=<filename.csv>` (query) | Reads and returns one CSV file content. |
| GET | `/monthly-summary` | `year=YYYY&month=MM` (query) | Downloads the monthly summary CSV stored under `/history/YYYY/monthsummarie/avg-MM.csv` with one averaged row per day. |
| GET | `/delete` | `file=<filename.csv>` (query) | Deletes one CSV file from SD card. |
<!-- `/upload-csv` removed: use `/fm/upload` (file-manager) for uploads -->

## WebSocket

- Endpoint: `/ws`
- Usage: clients send `getReadings` to request current data; server pushes JSON readings/history to connected clients.
- The web dashboard uses this channel for the live measurement table and chart, then falls back to `/measure-status` and `/sd-status` for system indicators.

## WiFi Provisioning API

The provisioning portal uses the WiFi endpoints below while the captive AP is active.

- `/wifi/scan` returns `202` while a background scan is still running. Poll the endpoint again until it returns `200` with the `networks` array.
- `/wifi/state` exposes current connection status, retry timing, and diagnostics such as `hint`, `attempt`, `lastDisconnectReason`, `stateElapsedMs`, and `connectTimeoutMs`.
- `/wifi/provision` expects JSON with `ssid`, `password`, and `token`. After a successful save, the firmware starts a fresh connection attempt.
- `/wifi/ap/stop` closes the temporary provisioning AP.
- `/wifi/current` and `/wifi/token` are used by the portal to restore the current form state after reload.
- `/fm/upload` is used by the file manager; pass `overwrite=1` to replace an existing file.
- `/logs` returns the current `log.txt` journal with `INFO`, `WARN`, and `ERROR` tags added by firmware.

## cURL Examples

Set your device address once or use the mDNS name:

```bash
ESP="http://192.168.1.50"
ESP="http://citerne.local"
```

Open pages:

```bash
curl "$ESP/"
curl "$ESP/settings"
```
- The main dashboard and settings pages are available whenever the web server is reachable (LAN IP or temporary AP), while `/portal` and the `/wifi/*` endpoints are only used for AP provisioning.

Device control:

```bash
curl -X POST "$ESP/reboot"
curl -X POST "$ESP/load-default-settings"
```
| GET | `/portal` | None | Returns the AP provisioning portal (`portal-index.html`). |

Settings read/write:

```bash
curl "$ESP/get-settings"

curl -X POST "$ESP/save-settings" \
| POST | `/save-settings` | JSON body | Saves settings, reinitializes time sync, and resets the measurement cycle. |
  -d '{
    "tank": {"capacite_L": {"value": 3000}, "height_mm": {"value": 1900}},
    "wifi": {
      "enabled": true,
      "retryDelay": {"value": 5000}
    },
    "display": {
      "bright": {"value": 185},
      "standbyBright": {"value": 35},
      "standbyInterval": {"value": 10},
      "standbyActived": true,
      "pageInterval": {"value": 8}
    },
    "sensor": {"offset_mm": {"value": 390}, "sampleCount": {"value": 3}, "sampleMethod": {"value": 1}, "sampleInterval": {"value": 80}},
    "mesure": {"measurePeriod": {"value": 1800000}},
    "button": {"debounceTime": {"value": 40}, "longPressTime": {"value": 2000}},
    "timeSource": {"tzString": "CET-1CEST,M3.5.0,M10.5.0"}
  }'
```

Status and diagnostics:

```bash
curl "$ESP/time-status"
curl "$ESP/sd-status"
curl "$ESP/measure-status"
curl "$ESP/logs"
```

SD file listing:

```bash
curl "$ESP/list-files"
```

CSV download/read:

```bash
curl -OJ "$ESP/download?file=2026-03-21.csv"
curl "$ESP/csv?file=2026-03-21.csv"
```

Delete files:

```bash
- Ranged sensor fields use min/max/step validation rather than a fixed preset list:
  - `sensor.sampleCount.value`
  - `sensor.spikeThreshold_mm.value`
curl "$ESP/delete?file=2026-03-21.csv"
curl -X POST "$ESP/delete-log"
```

- Setup AP mode is triggered via menu action (`AP Manuel`) and is temporary (not persisted to settings).
- The AP SSID and password are generated per device and shown on the TFT when AP starts.
- The portal displays live provisioning state, including deterministic failures such as bad password or missing SSID.
- AP auto-disables after 10 minutes of inactivity (no connected station) or when exiting from the menu credentials screen.
- AP mode still allows access to the main web pages while STA attempts normal connection.
- If `wifi.enabled=false`, HTTP and WebSocket endpoints are unavailable because the radio stack is disabled.
File manager upload with overwrite:

```bash
curl -X POST "$ESP/fm/upload?overwrite=1" -F "file=@./my-data.csv"
```

## Quick API Test Sequence (5 Steps)

Use this after flashing to validate the main HTTP API end-to-end.

1. Check basic connectivity and UI files.

```bash
curl -f "$ESP/" >/dev/null && echo "OK /"
curl -f "$ESP/settings" >/dev/null && echo "OK /settings"
```

2. Check settings endpoint returns JSON.

```bash
curl -s "$ESP/get-settings" | head
```

3. Check time and measure diagnostics.

```bash
curl -s "$ESP/time-status" | head
curl -s "$ESP/sd-status" | head
curl -s "$ESP/measure-status" | head
```

4. Check SD and logs endpoints.

```bash
curl -s "$ESP/list-files"
curl -s "$ESP/logs" | head
```

5. Check CSV read path with one known filename.

```bash
curl -s "$ESP/csv?file=2026-03-21.csv" | head
```

## One-Shot API Check Script (PASS/FAIL)

The shell snippet below uploads a temporary `test.csv`, validates `/csv` and `/download`, then deletes the file.

## Upload/Delete `test.csv` Validation

This script uploads a temporary `test.csv`, confirms it exists in `/list-files`, deletes it via `/delete`, then confirms it is gone.

```bash
ESP="http://citerne.local"
TEST_FILE="test.csv"
TMP_FILE="./${TEST_FILE}"
G='\033[0;32m'; R='\033[0;31m'; Z='\033[0m'

echo -e "time,distance,percent,volume\n2026-03-21T12:00:00,123.4,56,1200.5" > "$TMP_FILE"

pass() { echo -e "${G}PASS${Z} $1"; }
fail() { echo -e "${R}FAIL${Z} $1"; }

# 1) Upload test.csv
upload_code=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$ESP/fm/upload" -F "file=@${TMP_FILE}")
[[ "$upload_code" == 2* || "$upload_code" == 3* ]] && pass "upload /fm/upload [$upload_code]" || fail "upload /fm/upload [$upload_code]"

# 2) Check file appears in /list-files
if curl -s "$ESP/list-files" | grep -q "\"${TEST_FILE}\""; then
  pass "${TEST_FILE} found in /list-files"
else
  fail "${TEST_FILE} missing in /list-files"
fi

# 3) Delete test.csv
delete_code=$(curl -s -o /dev/null -w "%{http_code}" "$ESP/delete?file=${TEST_FILE}")
[[ "$delete_code" == 2* || "$delete_code" == 3* ]] && pass "delete /delete?file=${TEST_FILE} [$delete_code]" || fail "delete /delete?file=${TEST_FILE} [$delete_code]"

# 4) Check file no longer appears in /list-files
if curl -s "$ESP/list-files" | grep -q "\"${TEST_FILE}\""; then
  fail "${TEST_FILE} still present after delete"
else
  pass "${TEST_FILE} removed from /list-files"
fi

rm -f "$TMP_FILE"
```

## Notes

- Preset-backed fields are value-only in firmware settings structures:
  - `wifi.retryDelay.value`
  - `mesure.measurePeriod.value`
  - `mesure.predictionMethod.value`
  - `display.pageInterval.value`
  - `display.bright.value`
  - `display.standbyBright.value`
  - `sensor.sampleMethod.value`
  - `sensor.sampleInterval.value`
- Preset definitions and normalization helpers are centralized in:
  - `include/settings_schema.h`
  - `src/settings_schema.cpp`
- For those fields, `/get-settings` returns `choices` arrays (label/value pairs). Clients should use these values directly.
- Typical preset values:
  - `wifi.retryDelay`: `1000, 2000, 5000, 10000, 30000, 60000`
  - `mesure.measurePeriod`: `10000, 30000, 60000, 300000, 900000, 1800000, 3600000`
  - `display.pageInterval`: `1, 2, 5, 8, 10, 15, 30` (seconds)
  - `mesure.predictionMethod`: `0=Moyenne, 1=Lagrange, 2=Spline cubique, 3=Mediane`
  - `sensor.sampleMethod`: `0=average, 1=median`
  - `sensor.sampleInterval`: `40, 80, 120, 200, 300, 500, 750, 1000, 1500, 2000` (milliseconds)
  - `display.bright` / `display.standbyBright`: PWM values `10..255` from preset list

- WiFi settings include:
  - `wifi.enabled`: global WiFi ON/OFF.
  - `wifi.retryDelay.value`: retry backoff delay.
- Setup AP mode is triggered via menu action ("AP Manuel") and is temporary (not persisted to settings).
- The AP SSID and password are generated per device and shown on the TFT when AP starts.
- The portal displays live provisioning state, including deterministic failures such as bad password or missing SSID.
- AP auto-disables after 10 minutes of inactivity (no connected station) or when exiting from menu credentials screen.
- AP mode still allows access to web settings while STA attempts normal connection.
- If `wifi.enabled=false`, HTTP and WebSocket endpoints are unavailable because the radio stack is disabled.

- Most file operations require the SD card to be available (`sdOK == true`).
- Firmware now auto-recovers SD operations with health checks and automatic remount attempts (`ensureSDReady`).
- Web UI files currently served from LittleFS are:
  - `dashboard-index.html` + `dashboard-script.js` + `dashboard-style.css`
  - `file-manager-index.html` + `file-manager-script.js` + `file-manager-style.css`
  - `settings-index.html` + `settings-script.js` + `settings-style.css`
  - shared `base-style.css` imported by page-specific styles
- Canonical storage layout is:
  - daily CSV: `/history/YYYY/YYYY-MM/YYYY-MM-DD.csv`
  - monthly summary CSV: `/history/YYYY/monthsummarie/avg-MM.csv`
- File names are sanitized to reject `..` traversal.
- For endpoints expecting query parameters (`/download`, `/csv`, `/delete`), requests without required parameters return `400`.
- The file manager upload endpoint returns `409` when a target file already exists and `overwrite=1` was not supplied.
- The web dashboard serves local assets with a build-date cache-busting tag so mobile browsers refresh CSS/JS after a new build.

## `/sd-status` Response Fields

- `sdOK`: current SD ready state.
- `cardType`: `MMC`, `SDSC`, `SDHC`, or `NONE`.
- `totalBytes`, `usedBytes`, `freeBytes`: file-system capacity details.
- `usagePercent`: storage usage percentage.
- `hasSettings`, `hasLog`, `hasDailyFile`: required file checks.
- `dailyFile`: filename expected for today.
