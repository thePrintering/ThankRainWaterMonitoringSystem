#include "wifi_provisioning.h"

#include <WiFi.h>
#include <esp_system.h>

#include "constants.h"
#include "display.h"
#include "settings.h"
#include "storage.h"

// Private state variables
namespace {
  bool provisioningAPStarted = false;
  unsigned long lastProvisioningAPClientSeenMs = 0;
  int lastProvisioningStaCount = -1;
  String provisioningApSsid;
  String provisioningApPassword;
  String provisioningSessionToken;

  // Temporary test override: keep short AP password visible as requested.
  // Note: WPA2 requires >= 8 chars, so startup code falls back to open AP when shorter.
  constexpr bool kUseTemporaryTestApPassword = false;
}

// Helper functions
String buildProvisioningSsid() {
  uint64_t chipId = ESP.getEfuseMac();
  char ssidBuf[32];
  snprintf(ssidBuf, sizeof(ssidBuf), "TRWM-%02X%02X", (uint8_t)(chipId >> 8), (uint8_t)chipId);
  return String(ssidBuf);
}

String buildProvisioningPassword() {
  if (kUseTemporaryTestApPassword) {
    return String("12345678");
  }

  static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  static constexpr int prefixLength = 5; // "TRWM-"
  char passBuf[prefixLength + WIFI_AP_PASSWORD_LENGTH + 1];

  // New password at every AP activation: "TRWM-" + random chars.
  for (int i = 0; i < WIFI_AP_PASSWORD_LENGTH; ++i) {
    passBuf[prefixLength + i] = charset[esp_random() % (sizeof(charset) - 1)];
  }

  passBuf[0] = 'T';
  passBuf[1] = 'R';
  passBuf[2] = 'W';
  passBuf[3] = 'M';
  passBuf[4] = '-';
  passBuf[prefixLength + WIFI_AP_PASSWORD_LENGTH] = '\0';
  return String(passBuf);
}

String buildProvisioningSessionToken() {
  char tokenBuf[17];
  for (size_t i = 0; i < sizeof(tokenBuf) - 1; ++i) {
    const uint8_t value = static_cast<uint8_t>(esp_random() & 0x0F);
    tokenBuf[i] = (value < 10) ? static_cast<char>('0' + value) : static_cast<char>('a' + (value - 10));
  }
  tokenBuf[sizeof(tokenBuf) - 1] = '\0';
  return String(tokenBuf);
}

// Public API implementations
void initWiFiProvisioning() {
  provisioningAPStarted = false;
  lastProvisioningAPClientSeenMs = 0;
  lastProvisioningStaCount = -1;
  provisioningApSsid = "";
  provisioningApPassword = "";
  provisioningSessionToken = "";
}

bool isWiFiProvisioningAPActive() {
  return provisioningAPStarted;
}

void startProvisioningAP(unsigned long now,
                         bool &networkServicesStarted,
                         void (*startNetworkServices)()) {
  if (provisioningAPStarted) {
    LOGW("[WiFi] Provisioning AP start ignored: already active");
    return;
  }

  LOGI("[WiFi] Provisioning AP start requested");

  provisioningApSsid = buildProvisioningSsid();
  provisioningApPassword = buildProvisioningPassword();
  provisioningSessionToken = buildProvisioningSessionToken();

  WiFi.mode(WIFI_AP_STA);
  bool apStartedOk = false;
  if (provisioningApPassword.length() >= 8) {
    apStartedOk = WiFi.softAP(provisioningApSsid.c_str(), provisioningApPassword.c_str(), 1, false, 1);
  } else {
    // Short passphrase requested for tests: start AP in open mode to avoid startup failure.
    apStartedOk = WiFi.softAP(provisioningApSsid.c_str(), "", 1, false, 1);
  }

  if (!apStartedOk) {
    Serial.println("[WiFi] ERROR: failed to start provisioning AP");
    LOGE("[WiFi] Failed to start provisioning AP");
    return;
  }

  Serial.println("[WiFi] Provisioning AP started: " + provisioningApSsid);
  Serial.println("[WiFi] AP IP: " + WiFi.softAPIP().toString());
  if (provisioningApPassword.length() < 8) {
    Serial.println("[WiFi] AP started in OPEN mode (test password override too short for WPA2)");
    LOGW("[WiFi] AP started in OPEN mode (test password override too short for WPA2)");
  }
  LOGI("[WiFi] Provisioning AP started: " + provisioningApSsid);
  LOGI("[WiFi] AP IP: " + WiFi.softAPIP().toString());

  showAPCredentialsScreen(provisioningApSsid, provisioningApPassword);

  provisioningAPStarted = true;
  lastProvisioningAPClientSeenMs = now;
  lastProvisioningStaCount = 0;
  
  if (!networkServicesStarted) {
    startNetworkServices();
    networkServicesStarted = true;
  }
}

void stopProvisioningAP() {
  if (!provisioningAPStarted) {
    LOGW("[WiFi] Provisioning AP stop ignored: already inactive");
    return;
  }

  LOGI("[WiFi] Provisioning AP stop requested");

  WiFi.softAPdisconnect(true);
  provisioningAPStarted = false;
  lastProvisioningStaCount = -1;
  provisioningSessionToken = "";
  Serial.println("[WiFi] Provisioning AP stopped");
  LOGI("[WiFi] Provisioning AP stopped");
}

void checkProvisioningAPTimeout(unsigned long now) {
  if (!provisioningAPStarted) {
    return;
  }

  const int staCount = WiFi.softAPgetStationNum();
  if (staCount != lastProvisioningStaCount) {
    if (staCount > lastProvisioningStaCount) {
      LOGI("[WiFi] Provisioning AP client connected (count=" + String(staCount) + ")");
    } else {
      LOGI("[WiFi] Provisioning AP client disconnected (count=" + String(staCount) + ")");
    }
    lastProvisioningStaCount = staCount;
  }

  if (staCount > 0) {
    lastProvisioningAPClientSeenMs = now;
    return;
  }

  if (lastProvisioningAPClientSeenMs > 0 &&
      (now - lastProvisioningAPClientSeenMs) >= WIFI_AP_INACTIVITY_TIMEOUT) {
    Serial.println("[WiFi] Provisioning AP auto-off after inactivity");
    LOGW("[WiFi] Provisioning AP auto-off after inactivity");
    stopProvisioningAP();
  }
}

String getProvisioningAPSsid() {
  return provisioningApSsid;
}

String getProvisioningAPPassword() {
  return provisioningApPassword;
}

String getProvisioningSessionToken() {
  return provisioningSessionToken;
}

void invalidateProvisioningSessionToken() {
  provisioningSessionToken = "";
}

int getProvisioningAPClientCount() {
  if (!provisioningAPStarted) {
    return 0;
  }
  return WiFi.softAPgetStationNum();
}

unsigned long getProvisioningAPRemainingTime(unsigned long now) {
  if (!provisioningAPStarted || lastProvisioningAPClientSeenMs == 0) {
    return WIFI_AP_INACTIVITY_TIMEOUT / 1000;
  }

  const unsigned long elapsed = now - lastProvisioningAPClientSeenMs;
  if (elapsed >= WIFI_AP_INACTIVITY_TIMEOUT) {
    return 0;
  }

  return (WIFI_AP_INACTIVITY_TIMEOUT - elapsed) / 1000;
}
