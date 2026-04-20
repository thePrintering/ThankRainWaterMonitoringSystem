#include "wifi_config.h"
#include "display.h"               // for TFT_* color constants
#include "settings.h"
#include "constants.h"
#include "storage.h"
#include "wifi_provisioning.h"
#include <ESPmDNS.h>
#include <Preferences.h>

WiFiClass Wifi;
WiFiState wifiState = WIFI_IDLE;
unsigned long wifiStateTimestamp = 0;
bool wifiEverConnected = false;
int wifiQualityRSSI = 0;

// Runtime credentials come from provisioned NVS values.
const char* ssid = "";
const char* password = "";

namespace {
Preferences wifiPrefs;
String currentConfiguredSsid;
String currentConfiguredPassword;
String wifiProvisioningHint;
int wifiConnectAttemptCounter = 0;
int wifiLastDisconnectReason = 0;

bool isWiFiAuthFailureReason(int reason) {
  return reason == 15 || reason == 23 || reason == 202 || reason == 204;
}

bool isWiFiSsidNotFoundReason(int reason) {
  return reason == 201;
}

bool isWiFiNonRetryableReason(int reason) {
  return isWiFiAuthFailureReason(reason) || isWiFiSsidNotFoundReason(reason);
}

void applyRuntimeCredentials(const String &newSsid, const String &newPassword) {
  currentConfiguredSsid = newSsid;
  currentConfiguredPassword = newPassword;
  ssid = currentConfiguredSsid.c_str();
  password = currentConfiguredPassword.c_str();
}
}

bool loadProvisionedWiFiCredentials(String &outSsid, String &outPassword) {
  if (!wifiPrefs.begin("wifi", true)) {
    return false;
  }

  outSsid = wifiPrefs.getString("ssid", "");
  outPassword = wifiPrefs.getString("pass", "");
  wifiPrefs.end();
  return outSsid.length() > 0;
}

bool saveProvisionedWiFiCredentials(const String &newSsid, const String &newPassword) {
  if (newSsid.length() == 0) {
    return false;
  }

  if (!wifiPrefs.begin("wifi", false)) {
    return false;
  }

  const bool okSsid = wifiPrefs.putString("ssid", newSsid) > 0;
  const bool okPass = wifiPrefs.putString("pass", newPassword) >= 0;
  wifiPrefs.end();

  if (!okSsid || !okPass) {
    return false;
  }

  applyRuntimeCredentials(newSsid, newPassword);
  wifiProvisioningHint = "";
  return true;
}

bool clearProvisionedWiFiCredentials() {
  if (!wifiPrefs.begin("wifi", false)) {
    return false;
  }

  const bool ok = wifiPrefs.clear();
  wifiPrefs.end();

  applyRuntimeCredentials("", "");
  wifiProvisioningHint = "";
  return ok;
}

void forceWiFiReconnect() {
  // Abort any in-flight scan before reconnect attempt (AP+STA scan/connect contention).
  WiFi.scanDelete();

  const bool keepProvisioningAP = isWiFiProvisioningAPActive();
  WiFi.mode(keepProvisioningAP ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, true);

  // Start a fresh retry cycle after provisioning.
  settings.wifi.retryDelay.value = WIFI_RETRY_DELAY_MIN;
  wifiConnectAttemptCounter = 0;
  wifiProvisioningHint = "";
  wifiLastDisconnectReason = 0;
  wifiState = WIFI_START_CONNECT;
  wifiStateTimestamp = millis();
}

String getCurrentWiFiSSID() {
  return String(ssid ? ssid : "");
}

String getWiFiProvisioningHint() {
  return wifiProvisioningHint;
}

int getWiFiConnectAttemptCounter() {
  return wifiConnectAttemptCounter;
}

int getWiFiLastDisconnectReason() {
  return wifiLastDisconnectReason;
}

bool mdnsRunning = false;
static bool wifiRadioDisabled = false;
bool networkServicesStarted = false;

void initWifi() {
  wifiState = WIFI_IDLE;
  wifiEverConnected = false;
  wifiStateTimestamp = 0;
  networkServicesStarted = false;
  wifiRadioDisabled = false;
  wifiProvisioningHint = "";
  wifiConnectAttemptCounter = 0;
  wifiLastDisconnectReason = 0;

  initWiFiProvisioning();

  String provisionedSsid;
  String provisionedPassword;
  if (loadProvisionedWiFiCredentials(provisionedSsid, provisionedPassword)) {
    applyRuntimeCredentials(provisionedSsid, provisionedPassword);
    Serial.println("[WiFi] Using provisioned SSID from NVS: " + provisionedSsid);
    LOGI("[WiFi] Using provisioned SSID from NVS: " + provisionedSsid);
  } else {
    applyRuntimeCredentials("", "");
    settings.wifi.enabled = false;
    Serial.println("[WiFi] No provisioned credentials found. WiFi disabled until AP provisioning.");
    LOGW("[WiFi] No provisioned credentials found. WiFi disabled until AP provisioning.");
  }

  // Retry delay starts low and then increases with exponential backoff.
  settings.wifi.retryDelay.value = WIFI_RETRY_DELAY_MIN;

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("[WiFi] event " + String(event));
    if (event == (WiFiEvent_t)SYSTEM_EVENT_STA_DISCONNECTED) {
      const int reason = info.wifi_sta_disconnected.reason;
      wifiLastDisconnectReason = reason;
      Serial.printf("[WiFi] Reason: %d\n", reason);
      LOGW("[WiFi] Reason: " + String(reason));

      if (isWiFiAuthFailureReason(reason)) {
        wifiProvisioningHint = "Mdp WiFi faux -> /portal";
      } else if (isWiFiSsidNotFoundReason(reason)) {
        wifiProvisioningHint = "SSID introuvable -> /portal";
      }

      if (isWiFiNonRetryableReason(reason)) {
        // Credential errors are deterministic; keep ERROR state until new credentials are provisioned.
        if (wifiState == WIFI_START_CONNECT || wifiState == WIFI_CONNECTING || wifiState == WIFI_WAIT_RETRY) {
          wifiState = WIFI_ERROR;
          wifiStateTimestamp = millis();
          settings.wifi.retryDelay.value = WIFI_RETRY_DELAY_MIN;
        }
      }
    }
  });
}

void startMDNS() {
  if (mdnsRunning) return;

  if (MDNS.begin(mdnsName)) {
    MDNS.addService("http", "tcp", 80);
    mdnsRunning = true;
    Serial.println("[mDNS] mDNS actif : http://" + String(mdnsName) + ".local");
    LOGI("[mDNS] mDNS actif : http://" + String(mdnsName) + ".local");
  } else {
    Serial.println("[mDNS] mDNS failed");
    LOGE("[mDNS] mDNS failed");
  }
}

void stopMDNS() {
  if (!mdnsRunning) return;

  MDNS.end();
  mdnsRunning = false;
  Serial.println("[mDNS] mDNS stopped");
  LOGI("[mDNS] mDNS stopped");
}

const char* getWiFiStateString() {
  switch (wifiState) {
    case WIFI_DISABLED:
      return "Disabled";
    case WIFI_IDLE:
      return "Idle";
    case WIFI_START_CONNECT:
      return "Start";
    case WIFI_CONNECTING:
      return "Connecting";
    case WIFI_CONNECTED:
      return "Connected";
    case WIFI_WAIT_RETRY:
      return "Retry";
    case WIFI_ERROR:
      return "Error";
    default:
      return "Unknown";
  }
}

uint16_t getWiFiColor() {
  if (wifiState == WIFI_CONNECTED) return TFT_GREEN;
  if (wifiState == WIFI_CONNECTING) return TFT_ORANGE;
  if (wifiState == WIFI_WAIT_RETRY) return TFT_RED;
  return TFT_DARKGREY;
}

int rssiToQuality(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -70) return 3;
  if (rssi >= -85) return 2;
  return 1;
}

int getSmoothedRSSI() {
  static int rssiAvg = -100;
  int rssi = WiFi.RSSI();

  if (rssi == 0) return rssiAvg;

  rssiAvg = (rssiAvg * 4 + rssi) / 5;
  return rssiAvg;
}

void updateWifi() {
  extern void startNetworkServices();
  extern void relanceNetworkServices();
  extern unsigned long now;

  if (!settings.wifi.enabled) {
    if (!wifiRadioDisabled || wifiState != WIFI_DISABLED) {
      Serial.println("[WiFi] Disabled from settings");
      LOGI("[WiFi] Disabled from settings");

      stopMDNS();
      stopProvisioningAP();
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);

      wifiState = WIFI_DISABLED;
      wifiStateTimestamp = now;
      wifiQualityRSSI = 0;
      wifiRadioDisabled = true;
    }
    return;
  }

  checkProvisioningAPTimeout(now);

  if (wifiRadioDisabled) {
    Serial.println("[WiFi] Re-enabled from settings");
    LOGI("[WiFi] Re-enabled from settings");

    wifiRadioDisabled = false;
    wifiState = WIFI_IDLE;
    wifiStateTimestamp = now;
    settings.wifi.retryDelay.value = WIFI_RETRY_DELAY_MIN;
  }

  wl_status_t status = WiFi.status();
  const bool keepProvisioningAP = isWiFiProvisioningAPActive();

  switch (wifiState) {

    case WIFI_IDLE: {
      Serial.println("[WiFi] INIT");
      LOGI("[WiFi] INIT");

      // Clean STA start to avoid stale credentials/session leftovers.
      WiFi.mode(keepProvisioningAP ? WIFI_AP_STA : WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(!keepProvisioningAP, true);

      wifiStateTimestamp = now;
      wifiState = WIFI_START_CONNECT;
      break;
    }

    case WIFI_START_CONNECT:
      Serial.println("[WiFi] Connecting...");
      LOGI("[WiFi] Connecting SSID=" + String(ssid));

      if (ssid == nullptr || strlen(ssid) == 0) {
        wifiProvisioningHint = "SSID vide -> /portal";
        wifiStateTimestamp = now;
        wifiState = WIFI_WAIT_RETRY;
        break;
      }

      // Ensure no scan remains active before STA connect attempt.
      WiFi.scanDelete();

      wifiConnectAttemptCounter++;

      WiFi.begin(ssid, password);

      wifiStateTimestamp = now;
      wifiState = WIFI_CONNECTING;
      break;

    case WIFI_CONNECTING:
      if (status == WL_CONNECTED) {
        Serial.println("[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
        LOGI("[WiFi] Connected!");
        LOGI("[WiFi] IP: " + WiFi.localIP().toString());

        // Start web/OTA services once, then only refresh dynamic network services on reconnect.
        if (!networkServicesStarted) {
          startNetworkServices();
          networkServicesStarted = true;
        } else {
          relanceNetworkServices();
        }

        wifiEverConnected = true;
        settings.wifi.retryDelay.value = WIFI_RETRY_DELAY_MIN;
        wifiProvisioningHint = "";

        wifiState = WIFI_CONNECTED;
      } else if (now - wifiStateTimestamp >= WIFI_CONNECT_TIMEOUT) {
        Serial.println("[WiFi] Connection timeout");
        LOGW("[WiFi] Connection timeout");

        WiFi.disconnect(false, true);

        wifiStateTimestamp = now;
        wifiState = WIFI_WAIT_RETRY;
      }
      break;

    case WIFI_CONNECTED:
      if (status != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection!");
        LOGW("[WiFi] Lost connection!");

        stopMDNS();

        wifiStateTimestamp = now;
        wifiState = WIFI_WAIT_RETRY;
      } else {
        wifiQualityRSSI = getSmoothedRSSI();
      }
      break;

    case WIFI_WAIT_RETRY:
      if (now - wifiStateTimestamp >= settings.wifi.retryDelay.value) {
        Serial.print("[WiFi] Retry connection (delay=");
        Serial.print(settings.wifi.retryDelay.value);
        Serial.println("ms)");
        LOGI("[WiFi] Retry connection (delay=" + settings.wifi.retryDelay.value + "ms)");

        // Exponential backoff prevents rapid retry loops on unstable networks.
        settings.wifi.retryDelay.value = min(settings.wifi.retryDelay.value * 2, WIFI_MAX_RETRY_DELAY);

        wifiState = WIFI_START_CONNECT;
      }
      break;

    case WIFI_ERROR:
      break;
  }
}
