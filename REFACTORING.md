# Refactoring & Architecture Guide

Code organization, module responsibilities, and design decisions for the ThankRain firmware.

## File Organization

### Header Files (include/)
- **constants.h** - Global constants, page/mode enums, pin assignments, and shared build metadata
- **settings.h** - Settings structures and configuration management
- **sensor.h** - Sensor data structures and ultrasonic sensor functions
- **display.h** - TFT display functions, pages, icons, and mode/page globals
- **wifi_config.h** - WiFi state machine, diagnostics, and reconnect helpers
- **wifi_provisioning.h** - Manual AP provisioning lifecycle and AP credential helpers
- **webserver_config.h** - Web server, WebSocket, and API helpers
- **ota.h** - OTA update initialization, progress UI, and error handling
- **storage.h** - SD card operations, diagnostics, safe file management, and structured log writing
- **time_utils.h** - NTP time synchronization utilities
- **menu.h** - Menu system structures, button handling, and restart action flow

### Implementation Files (src/)
- **main.cpp** - Entry point (setup/loop only) - ~120 lines
- **sensor.cpp** - Sensor acquisition and processing
- **display_init.cpp** - Display initialization and hardware setup
- **display_icons.cpp** - Icon drawing functions (WiFi, sensor status)
- **display_pages.cpp** - Page drawing functions (gauge, graph, data, info, menu)
- **display_control.cpp** - Page switching, mode management, restart screen, and main update logic
- **wifi_config.cpp** - WiFi state machine, retry/backoff behavior, and diagnostics
- **wifi_provisioning.cpp** - Setup AP provisioning logic (manual AP)
- **webserver_config.cpp** - Web server endpoints, API handlers, and dashboard responses
- **ota.cpp** - OTA update setup, display progress, and error confirmation
- **storage.cpp** - SD card file operations, hardened recovery helpers, and serialized log writes
- **settings.cpp** - Settings loading/saving to JSON
- **time_utils.cpp** - Time and NTP functions
- **input.cpp** - Centralized button input helpers
- **menu.cpp** - Menu navigation, button handling, and restart confirmation

## Display Module Organization

The display functionality has been split into specialized sub-modules (the original monolithic display.cpp has been removed):

- **display_init.cpp**: Hardware initialization, brightness control, welcome screen
- **display_icons.cpp**: WiFi signal strength and sensor status indicators  
- **display_pages.cpp**: Individual page rendering (gauge, graph, data, info, menu)
- **display_control.cpp**: Page switching logic, mode management, restart screen, main update loop, and global timing variables

Global display timing variables (`lastPageSwitch`, `lastPressButtonTime`) are declared in `display.h` and defined in `display_control.cpp` for use across display and menu modules.

This organization makes it easy to:
- Modify individual pages without affecting others
- Add new icon types or page layouts
- Test display components independently
- Maintain consistent UI patterns across pages

## Compilation

Simply run `platformio run` - PlatformIO will automatically compile all source files in `src/`.

## Adding New Features

- **New display page?** → Add to `display_pages.cpp`
- **New display icon?** → Add to `display_icons.cpp`
- **New display control logic?** → Add to `display_control.cpp`
- **New web endpoint?** → Add to `webserver_config.cpp`
- **New sensor type?** → Extend `sensor.cpp`
- **New WiFi STA functionality?** → Update `wifi_config.cpp`
- **New setup AP/provisioning behavior?** → Update `wifi_provisioning.cpp`

## Global Extern Variables

Key global variables accessible across modules:
- `unsigned long now` - Current milliseconds (updated each loop)
- `Settings settings` - All application settings
- `UltrasonicState ultrasonicState` - Sensor state
- `SensorData sensorData` - Latest sensor reading
- `WiFiState wifiState` - WiFi connection state
- `DisplayPage page`, `DisplayMode mode` - Display page and mode

## WiFi Responsibilities (Current)

- `wifi_config.cpp`: STA lifecycle, retry/backoff state machine, connection diagnostics, mDNS handling, and network service orchestration.
- `wifi_provisioning.cpp`: setup AP lifecycle, manual AP toggle support, AP SSID/password generation, and provisioning session token handling.

## Logging Model

- `LOGI`, `LOGW`, and `LOGE` write `INFO`, `WARN`, and `ERROR` entries into `log.txt`
- `LOG(...)` now maps to `LOGI(...)` for compatibility with older call sites
- `logSD()` serializes concurrent writes with a mutex so AP, WiFi, OTA, and SD events are not dropped
- File Manager uploads reject existing files unless the request includes `overwrite=1`
- This split keeps connection-state logic independent from provisioning behavior

## SD Reliability Layer (2026 Update)

`storage.cpp` now provides a hardened SD access layer for professional deployments.

Core functions available in `storage.h`:
- `ensureSDReady()` - Verifies SD state, performs periodic health checks, and remounts when needed.
- `sdExists(path)` - Safe existence check through the readiness layer.
- `removeSDFile(path)` - Safe delete wrapper with remount retry.
- `getSDCardTypeName()` - Returns human-readable card type.
- `getSDTotalBytes()` / `getSDUsedBytes()` - Capacity metrics for diagnostics.

Internal helper in `storage.cpp`:
- `openSDFile(path, mode)` remains internal to the storage module and is not exposed in `storage.h`.

HTTP diagnostics:
- New endpoint `/sd-status` in `webserver_config.cpp` returns SD readiness, card type, usage, and required-file presence.

## Web Dashboard Updates

- The main dashboard now shows live WiFi, sensor, and SD status chips.
- The TFT info page shows SD readiness and storage usage percentage.
- OTA error handling now stops on a confirmation screen before rebooting or leaving the error flow.
- The restart menu action now shows a confirmation screen before rebooting.
- The file manager upload flow confirms before overwriting an existing file.

## Web UI Naming Convention (Current)

- Dashboard: `dashboard-index.html`, `dashboard-script.js`, `dashboard-style.css`
- File Manager: `file-manager-index.html`, `file-manager-script.js`, `file-manager-style.css`
- Settings: `settings-index.html`, `settings-script.js`, `settings-style.css`
- Shared base styles: `base-style.css` (imported by page-specific style files)
