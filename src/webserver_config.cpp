#include "webserver_config.h"
#include "settings.h"
#include "storage.h"
#include "time_utils.h"
#include "constants.h"
#include "ota.h"
#include "wifi_provisioning.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <SD.h>
#include <time.h>

AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");
// mdnsRunning is defined in wifi_config.cpp

// rebootTimer is defined in main.cpp
extern unsigned long lastWsUpdate;

namespace {

String listFilesJsonCache = "[]";
bool listFilesCacheReady = false;
File fmListCursorDir;
String fmListCursorPath = "";
int fmListCursorIndex = 0;
bool fmListCursorValid = false;
unsigned long wifiScanStartedAtMs = 0;

struct FmUploadContext {
  bool rejected = false;
  int statusCode = 500;
  String error = "upload failed";
  String storagePath;
  String baseName;
  size_t bytesWritten = 0;
};

bool isCaptivePortalModeActive() {
  return isWiFiProvisioningAPActive();
}

void sendCaptivePortalRedirect(AsyncWebServerRequest *request) {
  const String redirectUrl = "http://" + WiFi.softAPIP().toString() + "/portal";
  request->redirect(redirectUrl);
}

String normalizeCsvFileName(const String &name) {
  if (name.length() > 0 && name[0] == '/') {
    return name.substring(1);
  }
  return name;
}

bool isDailyCsvFileName(const String &name) {
  const String normalized = normalizeCsvFileName(name);
  if (normalized.length() != 14) {
    return false;
  }

  return isDigit(normalized[0]) && isDigit(normalized[1]) &&
         isDigit(normalized[2]) && isDigit(normalized[3]) &&
         normalized[4] == '-' &&
         isDigit(normalized[5]) && isDigit(normalized[6]) &&
         normalized[7] == '-' &&
         isDigit(normalized[8]) && isDigit(normalized[9]) &&
         normalized.substring(10) == ".csv";
}

bool parseStrictInt(const String &value, int &out) {
  if (value.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) {
      return false;
    }
  }

  out = value.toInt();
  return true;
}

String normalizePathForCsv(const String &path) {
  String name = path;
  if (name.length() > 0 && name[0] == '/') {
    name = name.substring(1);
  }

  int slashIndex = name.lastIndexOf('/');
  if (slashIndex >= 0) {
    name = name.substring(slashIndex + 1);
  }

  return name;
}

bool isDailyCsvPath(const String &path) {
  String name = normalizePathForCsv(path);
  if (name.length() != 14 || !name.endsWith(".csv")) {
    return false;
  }

  return isDigit(name[0]) && isDigit(name[1]) && isDigit(name[2]) && isDigit(name[3]) &&
         name[4] == '-' &&
         isDigit(name[5]) && isDigit(name[6]) &&
         name[7] == '-' &&
         isDigit(name[8]) && isDigit(name[9]);
}

String buildDailyHistoryStoragePath(const String &dailyName) {
  String name = normalizePathForCsv(dailyName);
  if (!isDailyCsvPath(name)) {
    return String();
  }

  int year = name.substring(0, 4).toInt();
  int month = name.substring(5, 7).toInt();
  if (year <= 0 || month < 1 || month > 12) {
    return String();
  }

  char pathBuf[48];
  snprintf(pathBuf, sizeof(pathBuf), "/history/%04d/%04d-%02d/%s", year, year, month, name.c_str());
  return String(pathBuf);
}

String resolveDailyCsvStoragePath(const String &fileName) {
  String name = normalizePathForCsv(fileName);
  return buildDailyHistoryStoragePath(name);
}

File openSDReadWithRetry(const char *path) {
  if (!ensureSDReady()) {
    return File();
  }

  File f = SD.open(path, FILE_READ);
  if (f) {
    return f;
  }

  // Mirror storage.cpp behavior: force a remount and retry once.
  sdOK = false;
  if (!ensureSDReady()) {
    return File();
  }

  return SD.open(path, FILE_READ);
}

void appendDailyCsvFilesFromYearDir(File yearDir,
                                    AsyncResponseStream *response,
                                    bool &first,
                                    int &count) {
  int scannedEntries = 0;
  while (true) {
    if ((++scannedEntries & 0x0F) == 0) {
      delay(0);
    }

    File file = yearDir.openNextFile();
    if (!file) {
      break;
    }

    if (file.isDirectory()) {
      file.close();
      continue;
    }

    String name = file.name();
    if (isDailyCsvPath(name)) {
      String normalizedDaily = normalizePathForCsv(name);
      if (!first) {
        response->print(",");
      }
      response->print("\"");
      response->print(normalizedDaily);
      response->print("\"");
      first = false;
      count++;
    }

    file.close();
  }
}

void appendDailyCsvFilesFromHistory(AsyncResponseStream *response,
                                    bool &first,
                                    int &count) {
  File historyRoot = openSDReadWithRetry("/history");
  if (!historyRoot) {
    return;
  }

  int scannedEntries = 0;
  while (true) {
    if ((++scannedEntries & 0x0F) == 0) {
      delay(0);
    }

    File yearEntry = historyRoot.openNextFile();
    if (!yearEntry) {
      break;
    }

    if (!yearEntry.isDirectory()) {
      yearEntry.close();
      continue;
    }

    appendDailyCsvFilesFromYearDir(yearEntry, response, first, count);
    yearEntry.close();
  }

  historyRoot.close();
}

void appendDailyCsvFilesFromYearDirToJson(File yearDir,
                                          String &json,
                                          bool &first,
                                          int &count) {
  int scannedEntries = 0;
  while (true) {
    if ((++scannedEntries & 0x0F) == 0) {
      delay(0);
    }

    File file = yearDir.openNextFile();
    if (!file) {
      break;
    }

    if (file.isDirectory()) {
      appendDailyCsvFilesFromYearDirToJson(file, json, first, count);
      file.close();
      continue;
    }

    String name = normalizePathForCsv(file.name());
    if (isDailyCsvPath(name)) {
      if (!first) {
        json += ",";
      }
      json += "\"";
      json += name;
      json += "\"";
      first = false;
      count++;
    }

    file.close();
  }
}

void buildListFilesCacheFromHistory() {
  String json = "[";
  bool first = true;
  int count = 0;

  File historyRoot = openSDReadWithRetry("/history");
  if (historyRoot) {
    int scannedEntries = 0;
    while (true) {
      if ((++scannedEntries & 0x0F) == 0) {
        delay(0);
      }

      File yearEntry = historyRoot.openNextFile();
      if (!yearEntry) {
        break;
      }

      if (!yearEntry.isDirectory()) {
        yearEntry.close();
        continue;
      }

      appendDailyCsvFilesFromYearDirToJson(yearEntry, json, first, count);
      yearEntry.close();
    }

    historyRoot.close();
  }

  json += "]";
  listFilesJsonCache = json;
  listFilesCacheReady = true;
  Serial.printf("[WebServer] list-files cache init: %d file(s)\n", count);
}

bool listFilesCacheContains(const String &dailyName) {
  String token = "\"" + dailyName + "\"";
  return listFilesJsonCache.indexOf(token) >= 0;
}

void addDailyToListFilesCache(const String &dailyPathOrName) {
  String dailyName = normalizePathForCsv(dailyPathOrName);
  if (!isDailyCsvPath(dailyName) || listFilesCacheContains(dailyName)) {
    return;
  }

  if (listFilesJsonCache == "[]") {
    listFilesJsonCache = "[\"" + dailyName + "\"]";
    return;
  }

  int closingBracket = listFilesJsonCache.lastIndexOf(']');
  if (closingBracket < 0) {
    listFilesJsonCache = "[\"" + dailyName + "\"]";
    return;
  }

  listFilesJsonCache.remove(closingBracket);
  listFilesJsonCache += ",\"" + dailyName + "\"]";
}

void removeDailyFromListFilesCache(const String &dailyPathOrName) {
  String dailyName = normalizePathForCsv(dailyPathOrName);
  if (!isDailyCsvPath(dailyName) || listFilesJsonCache.length() <= 2) {
    return;
  }

  String token = "\"" + dailyName + "\"";
  int pos = listFilesJsonCache.indexOf(token);
  if (pos < 0) {
    return;
  }

  int tokenLen = token.length();
  bool hasCommaAfter = (pos + tokenLen < listFilesJsonCache.length()) &&
                       (listFilesJsonCache[pos + tokenLen] == ',');
  bool hasCommaBefore = (pos > 0) && (listFilesJsonCache[pos - 1] == ',');

  int removeStart = pos;
  int removeLen = tokenLen;

  if (hasCommaAfter) {
    removeLen += 1;
  } else if (hasCommaBefore) {
    removeStart -= 1;
    removeLen += 1;
  }

  listFilesJsonCache.remove(removeStart, removeLen);

  if (listFilesJsonCache == "[") {
    listFilesJsonCache = "[]";
  }
}

void printJsonEscapedString(AsyncResponseStream *response, const String &value) {
  response->print("\"");
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '\\' || c == '"') {
      response->print('\\');
      response->print(c);
    } else if (c == '\n') {
      response->print("\\n");
    } else if (c == '\r') {
      response->print("\\r");
    } else if (c == '\t') {
      response->print("\\t");
    } else {
      response->print(c);
    }
  }
  response->print("\"");
}

String normalizeDirectoryPath(const String &raw) {
  String dir = raw;
  dir.trim();
  if (dir.length() == 0) {
    return String("/");
  }

  if (!dir.startsWith("/")) {
    dir = "/" + dir;
  }

  while (dir.length() > 1 && dir.endsWith("/")) {
    dir.remove(dir.length() - 1);
  }

  return dir;
}

bool normalizeSafeSdPath(const String &raw, String &normalized) {
  String path = normalizeDirectoryPath(raw);
  if (path.indexOf("..") >= 0) {
    return false;
  }

  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }

  normalized = path;
  return true;
}

String joinSdPath(const String &dir, const String &name) {
  if (dir == "/") {
    return "/" + name;
  }
  return dir + "/" + name;
}

bool isProtectedFmPath(const String &path) {
  return path == "/" ||
         path == "/history";
}

void fmResetListCursor() {
  if (fmListCursorDir) {
    fmListCursorDir.close();
  }
  fmListCursorPath = "";
  fmListCursorIndex = 0;
  fmListCursorValid = false;
}

bool fmPrepareListCursor(const String &dirPath, int offset) {
  if (!fmListCursorValid || fmListCursorPath != dirPath || offset < fmListCursorIndex) {
    fmResetListCursor();
    fmListCursorDir = openSDReadWithRetry(dirPath.c_str());
    if (!fmListCursorDir || !fmListCursorDir.isDirectory()) {
      fmResetListCursor();
      return false;
    }
    fmListCursorPath = dirPath;
    fmListCursorIndex = 0;
    fmListCursorValid = true;
  }

  int skipped = 0;
  while (fmListCursorIndex < offset) {
    File entry = fmListCursorDir.openNextFile();
    if (!entry) {
      // End reached before offset: keep cursor at end for cheap empty responses.
      fmListCursorIndex = offset;
      return true;
    }
    entry.close();
    fmListCursorIndex++;

    if ((++skipped & 0x07) == 0) {
      delay(0);
    }
  }

  return true;
}

void syncListFilesCacheWithCurrentDailyFile() {
  addDailyToListFilesCache(getDailyLogFile());
}

void appendAllFilesRecursive(File dir, AsyncResponseStream *response, bool &first) {
  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }

    if (file.isDirectory()) {
      // Depth-first traversal keeps parent folder subtree grouped together.
      appendAllFilesRecursive(file, response, first);
      file.close();
      continue;
    }

    String name = file.name();
    if (!first) {
      response->print(",");
    }
    response->print("\"");
    response->print(name);
    response->print("\"");
    first = false;

    file.close();
  }
}

} // namespace

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    // Incoming websocket payload is not guaranteed to have room for a trailing '\0'.
    String message;
    message.reserve(len);
    for (size_t i = 0; i < len; i++) {
      message += static_cast<char>(data[i]);
    }

    if (message == "getReadings") {
      Serial.print("[WebSocket] Received from WebSocket : ");
      Serial.println(message);
      notifyWebSocketClients(sensorData);
    }
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      Serial.printf("[WebSocket] client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    }
    case WS_EVT_DISCONNECT: {
      Serial.printf("[WebSocket] client #%u disconnected\n", client->id());
      break;
    }
    case WS_EVT_DATA: {
      handleWebSocketMessage(arg, data, len);
      break;
    }
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void notifyWebSocketClients(SensorData &data) {
  String json = "{";
  json += "\"time\":\"" + data.time + "\",";
  json += "\"distance\":" + String(data.distance, 1) + ",";
  json += "\"percent\":" + String(data.percent) + ",";
  json += "\"volume\":" + String(data.volume, 1) + ",";
  json += "\"history\":[";
  
  extern SensorData history[120];
  extern int historyIndex;
  extern bool historyFilled;
  
  int count = historyFilled ? HISTORY_SIZE : historyIndex;
  for (int i = 0; i < count; i++) {
    int idx = historyFilled ? (historyIndex + i) % HISTORY_SIZE : i;
    json += "{";
    json += "\"time\":\"" + history[idx].time + "\",";
    json += "\"distance\":" + String(history[idx].distance, 1) + ",";
    json += "\"percent\":" + String(history[idx].percent) + ",";
    json += "\"volume\":" + String(history[idx].volume, 1);
    json += "}";
    if (i < count - 1) json += ",";
  }
  json += "]}";

  webSocket.textAll(json);

  Serial.println("[WebSocket] ENVOI ___________________________>");
}

void initWebServer() {
  // Global CORS headers so browser fetches from dashboard clients are always accepted.
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(204);
      return;
    }

    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }

    request->send(404, "text/plain", "Not found");
  });

  // Build once at startup so /list-files never scans SD in async request context.
  buildListFilesCacheFromHistory();

  // Static UI pages served from LittleFS.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive / requete");
    if (isCaptivePortalModeActive()) {
      request->send(LittleFS, "/portal-index.html", "text/html");
      return;
    }
    request->send(LittleFS, "/dashboard-index.html", "text/html");
  });

  server.on("/portal", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive /portal requete");
    if (!isCaptivePortalModeActive()) {
      request->send(404, "text/plain", "Not found");
      return;
    }
    request->send(LittleFS, "/portal-index.html", "text/html");
  });

  server.on("/wifi/current", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isCaptivePortalModeActive()) {
      request->send(404, "application/json", "{\"ssid\":\"\"}");
      return;
    }
    JsonDocument doc;
    doc["ssid"] = getCurrentWiFiSSID();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/wifi/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isCaptivePortalModeActive()) {
      request->send(404, "application/json", "{\"state\":\"Unknown\",\"connected\":false}");
      return;
    }

    JsonDocument doc;
    doc["state"] = getWiFiStateString();
    doc["connected"] = (wifiState == WIFI_CONNECTED);
    doc["ssid"] = getCurrentWiFiSSID();
    doc["apActive"] = isWiFiProvisioningAPActive();
    int attempt = getWiFiConnectAttemptCounter();
    if (attempt <= 0 &&
        (wifiState == WIFI_START_CONNECT || wifiState == WIFI_CONNECTING || wifiState == WIFI_WAIT_RETRY)) {
      attempt = 1;
    }
    doc["attempt"] = attempt;
    doc["retryDelayMs"] = settings.wifi.retryDelay.value;
    doc["hint"] = getWiFiProvisioningHint();
    doc["lastDisconnectReason"] = getWiFiLastDisconnectReason();

    const unsigned long nowMs = millis();
    const unsigned long stateElapsedMs = nowMs - wifiStateTimestamp;
    doc["stateElapsedMs"] = stateElapsedMs;

    if (wifiState == WIFI_WAIT_RETRY) {
      const unsigned long delayMs = settings.wifi.retryDelay.value;
      doc["waitBeforeRetryMs"] = (stateElapsedMs >= delayMs) ? 0UL : (delayMs - stateElapsedMs);
    } else {
      doc["waitBeforeRetryMs"] = 0;
    }

    if (wifiState == WIFI_CONNECTING) {
      doc["connectTimeoutMs"] = WIFI_CONNECT_TIMEOUT;
    } else {
      doc["connectTimeoutMs"] = 0;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/wifi/token", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isCaptivePortalModeActive()) {
      request->send(404, "application/json", "{\"token\":\"\"}");
      return;
    }
    JsonDocument doc;
    doc["token"] = getProvisioningSessionToken();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isCaptivePortalModeActive()) {
      request->send(404, "application/json", "{\"networks\":[]}");
      return;
    }

    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    doc["running"] = false;

    const int scanState = WiFi.scanComplete();

    if (scanState == WIFI_SCAN_RUNNING) {
      // If a scan hangs unusually long, reset scan state and let next request restart.
      if (wifiScanStartedAtMs > 0 && (millis() - wifiScanStartedAtMs) > 15000UL) {
        WiFi.scanDelete();
        wifiScanStartedAtMs = 0;
      } else {
        doc["running"] = true;
        doc["waitMs"] = 350;
        String runningJson;
        serializeJson(doc, runningJson);
        request->send(202, "application/json", runningJson);
        return;
      }
    }

    if (scanState >= 0) {
      for (int i = 0; i < scanState; ++i) {
        JsonObject n = arr.add<JsonObject>();
        n["ssid"] = WiFi.SSID(i);
        n["rssi"] = WiFi.RSSI(i);
        n["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      }

      WiFi.scanDelete();
      wifiScanStartedAtMs = 0;

      String readyJson;
      serializeJson(doc, readyJson);
      request->send(200, "application/json", readyJson);
      return;
    }

    // No scan ready: start async scan and ask client to poll.
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    wifiScanStartedAtMs = millis();
    doc["running"] = true;
    doc["waitMs"] = 350;
    String startJson;
    serializeJson(doc, startJson);
    request->send(202, "application/json", startJson);
  });

  server.on("/wifi/provision", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
              if (!isCaptivePortalModeActive()) {
                LOGW("[WiFi] Provision reject: AP inactive");
                request->send(404, "application/json", "{\"ok\":false,\"error\":\"not in ap mode\"}");
                return;
              }

              JsonDocument doc;
              const DeserializationError err = deserializeJson(doc, data, len);
              if (err) {
                LOGW("[WiFi] Provision reject: invalid json");
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
                return;
              }

              const String providedToken = doc["token"] | "";
              if (providedToken.length() == 0 || providedToken != getProvisioningSessionToken()) {
                LOGW("[WiFi] Provision reject: invalid token");
                request->send(403, "application/json", "{\"ok\":false,\"error\":\"invalid token\"}");
                return;
              }

              String newSsid = doc["ssid"] | "";
              String newPassword = doc["password"] | "";
              newSsid.trim();

              if (newSsid.length() == 0) {
                LOGW("[WiFi] Provision reject: empty ssid");
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
                return;
              }

              if (newPassword.length() > 0 && newPassword.length() < 8) {
                LOGW("[WiFi] Provision reject: password too short");
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"password too short\"}");
                return;
              }

              if (!saveProvisionedWiFiCredentials(newSsid, newPassword)) {
                LOGE("[WiFi] Provision error: failed to save credentials");
                request->send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
                return;
              }

              LOGI("[WiFi] Provision accepted: SSID='" + newSsid + "', passLen=" + String(newPassword.length()));

              // If WiFi had been auto-disabled (empty NVS case), re-enable it now.
              settings.wifi.enabled = true;
              saveSettings();

              // Keep token valid for the whole AP session so user can retry credentials.
              forceWiFiReconnect();
              LOGI("[WiFi] Provision connect attempt started");
              request->send(200, "application/json", "{\"ok\":true,\"connecting\":true,\"apStillActive\":true}");
            });

  server.on("/wifi/ap/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isCaptivePortalModeActive()) {
      request->send(404, "application/json", "{\"ok\":false,\"error\":\"not in ap mode\"}");
      return;
    }

    stopProvisioningAP();
    request->send(200, "application/json", "{\"ok\":true,\"apStillActive\":false}");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive /settings requete");
    request->send(LittleFS, "/settings-index.html", "text/html");
  });

  // Captive portal probe endpoints for major clients (Android/iOS/Windows).
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(204);
  });

  server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(204);
  });

  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(200, "text/plain", "Microsoft NCSI");
  });

  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(200, "text/plain", "Microsoft Connect Test");
  });

  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCaptivePortalModeActive()) {
      sendCaptivePortalRedirect(request);
      return;
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/file-manager", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive /file-manager requete");
    request->send(LittleFS, "/file-manager-index.html", "text/html");
  });

  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive /reboot requete");
    LOGI("[WebServer] Receive /reboot requete");
    request->send(200, "text/plain", "Rebooting");

    webSocket.closeAll();
    rebootTimer.once_ms(200, []() {
      ESP.restart();
    });
  });

  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    // File API: download one SD file by name.
    Serial.println("[WebServer] Requête /download");

    if (!ensureSDReady()) {
      request->send(503, "text/plain", "Carte SD absente");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Paramètre 'file' manquant");
      return;
    }

    String filename = request->getParam("file")->value();

    if (filename.indexOf("..") >= 0) {
      request->send(400, "text/plain", "Nom de fichier invalide");
      return;
    }

    String path = "/" + filename;

    // Accept plain daily names by resolving to /history/YYYY/MM/YYYY-MM-DD.csv.
    if (!sdExists(path) && isDailyCsvPath(filename)) {
      path = resolveDailyCsvStoragePath(filename);
    }

    if (!sdExists(path)) {
      request->send(404, "text/plain", "Fichier introuvable");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "Erreur ouverture fichier");
      return;
    }

    request->send(file, filename, "text/csv", true);
  });

  server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *req) {
    // File API: return cached list to keep request handler non-blocking.
    Serial.println("[WebServer] Receive /list-files requete");
    syncListFilesCacheWithCurrentDailyFile();
    req->send(200, "application/json", listFilesCacheReady ? listFilesJsonCache : "[]");
  });

  server.on("/list-all-files", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Diagnostics API: cache-only storage view (no SD traversal in request context).
    syncListFilesCacheWithCurrentDailyFile();

    int maxFiles = 300;
    if (request->hasParam("max")) {
      int requested = 0;
      if (parseStrictInt(request->getParam("max")->value(), requested)) {
        if (requested < 1) {
          requested = 1;
        }
        if (requested > 1200) {
          requested = 1200;
        }
        maxFiles = requested;
      }
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"source\":\"history-cache\",\"max\":");
    response->print(maxFiles);
    response->print(",\"roots\":[\"/history\"],\"years\":[");

    bool firstYear = true;
    bool firstFile = true;
    bool hasMore = false;
    int yearCount = 0;
    int fileCount = 0;
    String seenYears = "|";

    int cursor = 0;
    while (true) {
      int q1 = listFilesJsonCache.indexOf('"', cursor);
      if (q1 < 0) {
        break;
      }
      int q2 = listFilesJsonCache.indexOf('"', q1 + 1);
      if (q2 < 0) {
        break;
      }

      String token = listFilesJsonCache.substring(q1 + 1, q2);
      cursor = q2 + 1;
      if (!isDailyCsvPath(token)) {
        continue;
      }

      String dailyName = normalizePathForCsv(token);
      String year = dailyName.substring(0, 4);
      String marker = "|" + year + "|";
      if (seenYears.indexOf(marker) >= 0) {
        continue;
      }

      if (!firstYear) {
        response->print(",");
      }
      printJsonEscapedString(response, year);
      firstYear = false;
      seenYears += year + "|";
      yearCount++;
    }

    response->print("],\"files\":[");

    cursor = 0;
    while (true) {
      int q1 = listFilesJsonCache.indexOf('"', cursor);
      if (q1 < 0) {
        break;
      }
      int q2 = listFilesJsonCache.indexOf('"', q1 + 1);
      if (q2 < 0) {
        break;
      }

      String token = listFilesJsonCache.substring(q1 + 1, q2);
      cursor = q2 + 1;
      if (!isDailyCsvPath(token)) {
        continue;
      }

      if (fileCount >= maxFiles) {
        hasMore = true;
        break;
      }

      String dailyName = normalizePathForCsv(token);
      String year = dailyName.substring(0, 4);
      String fullPath = "/history/" + year + "/" + dailyName;

      if (!firstFile) {
        response->print(",");
      }
      printJsonEscapedString(response, fullPath);
      firstFile = false;
      fileCount++;

      if ((fileCount & 0x1F) == 0) {
        delay(0);
      }
    }

    response->print("],\"yearCount\":");
    response->print(yearCount);
    response->print(",\"fileCount\":");
    response->print(fileCount);
    response->print(",\"hasMore\":");
    response->print(hasMore ? "true" : "false");
    response->print("}");

    request->send(response);
  });

  server.on("/fm/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    // File manager API: lightweight one-directory listing with pagination.
    if (!ensureSDReady()) {
      request->send(503, "application/json", "{\"error\":\"sd not ready\"}");
      return;
    }

    String dirPath = "/";
    if (request->hasParam("dir")) {
      if (!normalizeSafeSdPath(request->getParam("dir")->value(), dirPath)) {
        request->send(400, "application/json", "{\"error\":\"invalid dir\"}");
        return;
      }
    }

    int offset = 0;
    if (request->hasParam("offset")) {
      int requested = 0;
      if (parseStrictInt(request->getParam("offset")->value(), requested)) {
        if (requested < 0) {
          requested = 0;
        }
        if (requested > 50000) {
          requested = 50000;
        }
        offset = requested;
      }
    }

    int limit = 40;
    if (request->hasParam("limit")) {
      int requested = 0;
      if (parseStrictInt(request->getParam("limit")->value(), requested)) {
        if (requested < 1) {
          requested = 1;
        }
        if (requested > 120) {
          requested = 120;
        }
        limit = requested;
      }
    }

    if (!fmPrepareListCursor(dirPath, offset)) {
      request->send(404, "application/json", "{\"error\":\"dir not found\"}");
      return;
    }

    Serial.printf("[FM] /fm/list dir=%s offset=%d limit=%d\n", dirPath.c_str(), offset, limit);

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"dir\":");
    printJsonEscapedString(response, dirPath);
    response->print(",\"offset\":");
    response->print(offset);
    response->print(",\"limit\":");
    response->print(limit);
    response->print(",\"entries\":[");

    bool first = true;
    int count = 0;
    bool hasMore = false;
    int scannedEntries = 0;

    while (true) {
      if ((++scannedEntries & 0x0F) == 0) {
        delay(0);
      }

      File entry = fmListCursorDir.openNextFile();
      if (!entry) {
        fmListCursorValid = false;
        fmListCursorPath = "";
        break;
      }

      if (count >= limit) {
        hasMore = true;
        entry.close();
        break;
      }

      String rawName = entry.name();
      String name = normalizePathForCsv(rawName);
      if (name.length() == 0) {
        entry.close();
        continue;
      }
      String fullPath = joinSdPath(dirPath, name);

      if (!first) {
        response->print(",");
      }
      response->print("{\"name\":");
      printJsonEscapedString(response, name);
      response->print(",\"path\":");
      printJsonEscapedString(response, fullPath);
      response->print(",\"type\":");
      printJsonEscapedString(response, entry.isDirectory() ? "dir" : "file");
      response->print(",\"size\":");
      response->print((unsigned long long)entry.size());
      response->print("}");

      first = false;
      count++;
      fmListCursorIndex++;
      entry.close();
    }

    String parent = "/";
    if (dirPath != "/") {
      int lastSlash = dirPath.lastIndexOf('/');
      if (lastSlash > 0) {
        parent = dirPath.substring(0, lastSlash);
      }
    }

    response->print("],\"count\":");
    response->print(count);
    response->print(",\"visibleFrom\":");
    response->print(count > 0 ? (offset + 1) : 0);
    response->print(",\"visibleTo\":");
    response->print(offset + count);
    response->print(",\"totalChildren\":");
    response->print(hasMore ? -1 : (offset + count));
    response->print(",\"hasMore\":");
    response->print(hasMore ? "true" : "false");
    response->print(",\"nextOffset\":");
    response->print(offset + count);
    response->print(",\"parent\":");
    printJsonEscapedString(response, parent);
    response->print("}");

    Serial.printf("[FM] /fm/list -> count=%d hasMore=%s\n", count, hasMore ? "true" : "false");
    if (!hasMore) {
      // Avoid keeping an open directory handle when we reached the end.
      fmResetListCursor();
    }

    request->send(response);
  });

  server.on("/fm/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "sd not ready");
      return;
    }

    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "missing path");
      return;
    }

    String path;
    if (!normalizeSafeSdPath(request->getParam("path")->value(), path)) {
      request->send(400, "text/plain", "invalid path");
      return;
    }

    if (!sdExists(path)) {
      request->send(404, "text/plain", "not found");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
      request->send(400, "text/plain", "not a file");
      return;
    }

    String filename = path;
    int slashIndex = filename.lastIndexOf('/');
    if (slashIndex >= 0) {
      filename = filename.substring(slashIndex + 1);
    }

    request->send(file, filename, "application/octet-stream", true);
  });

  server.on("/fm/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    fmResetListCursor();
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "sd not ready");
      return;
    }

    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "missing path");
      return;
    }

    String path;
    if (!normalizeSafeSdPath(request->getParam("path")->value(), path) || path == "/") {
      request->send(400, "text/plain", "invalid path");
      return;
    }

    if (isProtectedFmPath(path)) {
      request->send(403, "text/plain", "protected path");
      return;
    }

    bool ok = removeSDFile(path);

    if (!ok) {
      request->send(500, "text/plain", "delete failed");
      return;
    }

    String baseName = normalizePathForCsv(path);
    if (isDailyCsvPath(baseName)) {
      removeDailyFromListFilesCache(baseName);
      rebuildMonthlySummaryForDailyFile(baseName);
      pruneDailyHistoryDirectoriesForFile(baseName);
    }

    request->send(200, "text/plain", "OK");
  });

  server.on("/fm/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
    fmResetListCursor();
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "sd not ready");
      return;
    }

    if (!request->hasParam("path", true)) {
      request->send(400, "text/plain", "missing path");
      return;
    }

    String path;
    if (!normalizeSafeSdPath(request->getParam("path", true)->value(), path) || path == "/") {
      request->send(400, "text/plain", "invalid path");
      return;
    }

    if (SD.exists(path)) {
      request->send(200, "text/plain", "OK");
      return;
    }

    bool ok = SD.mkdir(path.c_str());
    if (!ok) {
      sdOK = false;
      if (ensureSDReady()) {
        ok = SD.mkdir(path.c_str());
      }
    }

    request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "mkdir failed");
  });

  server.on("/fm/upload", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              fmResetListCursor();

              FmUploadContext *ctx = static_cast<FmUploadContext *>(request->_tempObject);
              if (!ctx) {
                request->send(400, "text/plain", "missing file");
                return;
              }

              if (ctx->rejected) {
                request->send(ctx->statusCode, "text/plain", ctx->error);
                delete ctx;
                request->_tempObject = nullptr;
                return;
              }

              request->send(200, "text/plain", "Upload termine");
              delete ctx;
              request->_tempObject = nullptr;
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
              FmUploadContext *ctx = static_cast<FmUploadContext *>(request->_tempObject);
              if (!ctx) {
                ctx = new FmUploadContext();
                request->_tempObject = ctx;
              }

              if (index == 0) {
                if (!ensureSDReady()) {
                  ctx->rejected = true;
                  ctx->statusCode = 503;
                  ctx->error = "sd not ready";
                  return;
                }

                String dirPath = "/";
                if (request->hasParam("dir")) {
                  String requested = request->getParam("dir")->value();
                  if (!normalizeSafeSdPath(requested, dirPath)) {
                    ctx->rejected = true;
                    ctx->statusCode = 400;
                    ctx->error = "invalid dir";
                    return;
                  }
                }

                ctx->baseName = normalizePathForCsv(filename);
                if (ctx->baseName.length() == 0) {
                  ctx->rejected = true;
                  ctx->statusCode = 400;
                  ctx->error = "invalid file name";
                  return;
                }

                ctx->storagePath = joinSdPath(dirPath, ctx->baseName);

                bool overwrite = false;
                if (request->hasParam("overwrite")) {
                  overwrite = request->getParam("overwrite")->value() == "1";
                }

                if (sdExists(ctx->storagePath) && !overwrite) {
                  ctx->rejected = true;
                  ctx->statusCode = 409;
                  ctx->error = "file exists";
                  return;
                }

                if (overwrite && sdExists(ctx->storagePath) && !removeSDFile(ctx->storagePath)) {
                  ctx->rejected = true;
                  ctx->statusCode = 500;
                  ctx->error = "cannot overwrite existing file";
                  return;
                }
              }

              if (ctx->rejected) {
                return;
              }

              File file = SD.open(ctx->storagePath, index == 0 ? FILE_WRITE : FILE_APPEND);
              if (!file) {
                ctx->rejected = true;
                ctx->statusCode = 500;
                ctx->error = "open failed";
                return;
              }

              const size_t written = file.write(data, len);
              file.close();

              if (written != len) {
                ctx->rejected = true;
                ctx->statusCode = 500;
                ctx->error = "write failed";
                return;
              }

              ctx->bytesWritten += written;

              if (final && isDailyCsvPath(ctx->baseName)) {
                addDailyToListFilesCache(ctx->baseName);
                rebuildMonthlySummaryForDailyFile(ctx->baseName);
              }

              if (final) {
                Serial.printf("[FM] /fm/upload done path=%s bytes=%u\n", ctx->storagePath.c_str(), (unsigned)ctx->bytesWritten);
              }
            });

  server.on("/get-settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Settings API: expose in-memory settings currently used by firmware.
    JsonDocument doc;
    exportSettingsToApiJson(settings, doc);

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/time-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Diagnostics API: timezone + local/UTC clock state.
    JsonDocument doc;
    struct tm localTm;
    struct tm utcTm;
    char localBuf[25] = "--/--/---- --:--:--";
    char utcBuf[25] = "--/--/---- --:--:--";
    char localIsoBuf[40] = "----/--/--T--:--:--";
    char utcIsoBuf[40] = "----/--/--T--:--:--Z";

    bool hasLocalTime = getLocalTime(&localTm, 1500);
    time_t nowEpoch = time(nullptr);

    if (hasLocalTime) {
      strftime(localBuf, sizeof(localBuf), "%d/%m/%Y %H:%M:%S", &localTm);
      strftime(localIsoBuf, sizeof(localIsoBuf), "%Y-%m-%dT%H:%M:%S%z", &localTm);
      gmtime_r(&nowEpoch, &utcTm);
      strftime(utcBuf, sizeof(utcBuf), "%d/%m/%Y %H:%M:%S", &utcTm);
      strftime(utcIsoBuf, sizeof(utcIsoBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
      doc["isDst"] = localTm.tm_isdst;
    } else {
      doc["isDst"] = -1;
    }

    const char* tzEnv = getenv("TZ");

    doc["synced"] = hasLocalTime;
    doc["epoch"] = (long long)nowEpoch;
    doc["local"] = localBuf;
    doc["localIso"] = localIsoBuf;
    doc["utc"] = utcBuf;
    doc["utcIso"] = utcIsoBuf;
    doc["tzSetting"] = settings.timeSource.tzString;
    doc["tzEnv"] = tzEnv ? tzEnv : "";

    String json;
    serializeJsonPretty(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/sd-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    // Snapshot SD state once, then build a complete diagnostic payload for UI/tools.
    bool ready = ensureSDReady();
    uint64_t totalBytes = ready ? getSDTotalBytes() : 0;
    uint64_t usedBytes = ready ? getSDUsedBytes() : 0;

    doc["sdOK"] = ready;
    doc["cardType"] = ready ? getSDCardTypeName() : "NONE";
    doc["totalBytes"] = (long long)totalBytes;
    doc["usedBytes"] = (long long)usedBytes;
    doc["freeBytes"] = (long long)(totalBytes >= usedBytes ? (totalBytes - usedBytes) : 0);
    doc["usagePercent"] = (totalBytes > 0) ? ((double)usedBytes * 100.0 / (double)totalBytes) : 0.0;
    doc["hasSettings"] = ready && sdExists("/settings.json");
    doc["hasLog"] = ready && sdExists("/log.txt");
    doc["dailyFile"] = getDailyLogFile();
    doc["hasDailyFile"] = ready && sdExists(getDailyLogFile());

    String json;
    serializeJsonPretty(doc, json);
    request->send(ready ? 200 : 503, "application/json", json);
  });

  server.on("/measure-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Diagnostics API: latest sensor data + SD write context.
    JsonDocument doc;

    String fileName = getDailyLogFile();
    String lastLine = "";
    bool fileOpened = false;

    // fileOpened indicates real open/read success, not only SD presence.
    if (ensureSDReady() && sdExists(fileName)) {
      File f = SD.open(fileName, FILE_READ);
      if (f) {
        fileOpened = true;
        while (f.available()) {
          String line = f.readStringUntil('\n');
          line.trim();
          if (line.length() > 0) {
            lastLine = line;
          }
        }
        f.close();
      }
    }

    doc["sdOK"] = ensureSDReady();
    doc["sdCardType"] = getSDCardTypeName();
    doc["sdTotalBytes"] = (long long)getSDTotalBytes();
    doc["sdUsedBytes"] = (long long)getSDUsedBytes();
    doc["dailyFile"] = fileName;
    // Useful to diagnose cases where file exists but cannot be opened (permissions/media fault).
    doc["fileOpened"] = fileOpened;
    doc["currentLocal"] = getCurrentDateTime();
    doc["sensorDataTime"] = sensorData.time;
    doc["lastCsvLine"] = lastLine;
    doc["sensorConnected"] = ultrasonicState.sensorState;
    doc["measurePeriodMs"] = settings.mesure.measurePeriod.value;
    doc["lastUpdateTimeMs"] = lastUpdateTime;

    extern unsigned long now;
    doc["nowMs"] = now;
    doc["lastUpdateAgeSec"] = (now >= lastUpdateTime) ? ((now - lastUpdateTime) / 1000UL) : 0;

    String json;
    serializeJsonPretty(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!ensureSDReady()) {
      request->send(500, "text/plain", "SD not ready");
      return;
    }

    if (!sdExists("/log.txt")) {
      request->send(404, "text/plain", "log.txt not found");
      return;
    }

    File file = SD.open("/log.txt", FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "open failed");
      return;
    }

    request->send(file, "log.txt", "text/plain", false);
  });
  server.on("/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    // File API: raw CSV read endpoint.
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "sd not ready");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "missing file");
      return;
    }

    String filename = request->getParam("file")->value();

    if (filename.indexOf("..") >= 0 || !filename.endsWith(".csv")) {
      request->send(400, "text/plain", "invalid filename");
      return;
    }

    String path = "/" + filename;

    // Accept plain daily names by resolving to /history/YYYY/MM/YYYY-MM-DD.csv.
    if (!sdExists(path) && isDailyCsvPath(filename)) {
      path = resolveDailyCsvStoragePath(filename);
    }

    if (!sdExists(path)) {
      request->send(404, "text/plain", "not found");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "open failed");
      return;
    }

    // ✅ LA LIGNE CORRECTE
    request->send(file, filename, "text/csv", false);
  });

  server.on("/monthly-summary", HTTP_GET, [](AsyncWebServerRequest *request) {
    // File API: download one monthly summary CSV (avg-YYYY-MM.csv).
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "sd not ready");
      return;
    }

    if (!request->hasParam("year") || !request->hasParam("month")) {
      request->send(400, "text/plain", "missing year/month");
      return;
    }

    int year = 0;
    int month = 0;
    if (!parseStrictInt(request->getParam("year")->value(), year) || year < 2000 || year > 2200) {
      request->send(400, "text/plain", "invalid year");
      return;
    }

    if (!parseStrictInt(request->getParam("month")->value(), month) || month < 1 || month > 12) {
      request->send(400, "text/plain", "invalid month");
      return;
    }

    const String summaryFile = String("avg-") + (month < 10 ? "0" : "") + String(month) + ".csv";
    char summaryPathBuf[48];
    snprintf(summaryPathBuf, sizeof(summaryPathBuf), "/history/%04d/monthsummarie/avg-%02d.csv", year, month);
    const String summaryPath = summaryPathBuf;

    if (!sdExists(summaryPath)) {
      request->send(404, "text/plain", "monthly summary not found");
      return;
    }

    File file = SD.open(summaryPath, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "open failed");
      return;
    }

    request->send(file, summaryFile, "text/csv", false);
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    // File API: delete one CSV by name.
    Serial.println("[WebServer] Receive /delete requete");
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "SD absente");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Paramètre manquant");
      return;
    }

    String filename = request->getParam("file")->value();

    // Sécurité minimale
    if (filename.indexOf("..") >= 0 || !filename.endsWith(".csv")) {
      request->send(400, "text/plain", "Nom de fichier invalide");
      return;
    }

    String path = "/" + filename;

    // Accept plain daily names by resolving to /history/YYYY/MM/YYYY-MM-DD.csv.
    if (!sdExists(path) && isDailyCsvPath(filename)) {
      path = resolveDailyCsvStoragePath(filename);
    }

    if (!sdExists(path)) {
      request->send(404, "text/plain", "Fichier introuvable");
      return;
    }

    if (!removeSDFile(path)) {
      request->send(500, "text/plain", "Echec suppression");
      return;
    }

    removeDailyFromListFilesCache(filename);
    rebuildMonthlySummaryForDailyFile(filename);
    if (isDailyCsvPath(filename)) {
      pruneDailyHistoryDirectoriesForFile(filename);
    }

    Serial.println("[SD] Supprime : " + path);
    request->send(200, "text/plain", "OK");
  });

  server.on("/load-default-settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Settings API: remove persisted JSON and reboot on defaults.

    Serial.println("[WebServer] /load-default-settings reçu");

    if (ensureSDReady() && sdExists("/settings.json")) {
      removeSDFile("/settings.json");
      Serial.println("[Settings] settings.json supprime");
    } else {
      Serial.println("[Settings] Aucun fichier à supprimer");
    }

    request->send(200, "text/plain", "Default settings loaded, rebooting");

    webSocket.closeAll();

    rebootTimer.once_ms(200, [](){
      ESP.restart();
    });
  });
  // /upload-csv handler removed — use /fm/upload instead.

  server.on("/delete-log", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[WebServer] Receive /delete-logs requete");
    if (!ensureSDReady()) {
      request->send(503, "text/plain", "SD absente");
      return;
    }

    if (!sdExists("/log.txt")) {
      request->send(404, "text/plain", "Fichier log.txt introuvable");
      return;
    }

    if (!removeSDFile("/log.txt")) {
      request->send(500, "text/plain", "Echec suppression log.txt");
      return;
    }

    Serial.println("[SD] Supprime : log.txt");
    request->send(200, "text/plain", "OK");
  });

  server.on("/save-settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Settings API: apply JSON patch to runtime settings then persist on SD.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);

    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    applySettingsFromJson(settings, doc);

    saveSettings();
    initNTP_Time();
    resetMeasureCycle(ultrasonicState);
    request->send(200, "text/plain", "OK");
  });

  server.on("/force-measure", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Diagnostics API: trigger an immediate sensor read+SD write outside of regular cycle.
    Serial.println("[WebServer] Receive /force-measure requete");
    LOGI("[WebServer] Receive /force-measure requete");
    resetMeasureCycle(ultrasonicState);
    request->send(200, "text/plain", "OK");
  });

  server.serveStatic("/", LittleFS, "/");

  server.begin();
  Serial.println("[WebServer] Serveur web démarré");
}

void initWebSocket() {
  webSocket.onEvent(onWebSocketEvent);
  server.addHandler(&webSocket);

  Serial.println("[WebSocket] WebSocket démarré");
  LOGI("[WebSocket] WebSocket demarre");
}

void updateWebSocket() {
  webSocket.cleanupClients();
}

void startNetworkServices() {
  initWebServer();
  initWebSocket();
  initNTP_Time();
  resetMeasureCycle(ultrasonicState);
  initOTA(); // ajout
  startMDNS();
}

void relanceNetworkServices() {
  initNTP_Time();
  resetMeasureCycle(ultrasonicState);
  startMDNS();
}
