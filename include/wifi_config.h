#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>

// =================== WIFI STATE MACHINE ===================
enum WiFiState {
  WIFI_DISABLED,
  WIFI_IDLE,
  WIFI_START_CONNECT,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_WAIT_RETRY,
  WIFI_ERROR
};

// =================== GLOBAL VARIABLES ===================
extern WiFiClass Wifi;
extern WiFiState wifiState;
extern unsigned long wifiStateTimestamp;
extern bool wifiEverConnected;
extern int wifiQualityRSSI;

extern const char* ssid;
extern const char* password;

// =================== FUNCTION DECLARATIONS ===================
void initWifi();
void updateWifi();
void startMDNS();
void stopMDNS();

// Utility functions
const char* getWiFiStateString();
uint16_t getWiFiColor();
int rssiToQuality(int rssi);
int getSmoothedRSSI();

// Provisioning helpers
bool saveProvisionedWiFiCredentials(const String &newSsid, const String &newPassword);
bool loadProvisionedWiFiCredentials(String &outSsid, String &outPassword);
bool clearProvisionedWiFiCredentials();
void forceWiFiReconnect();
String getCurrentWiFiSSID();
String getWiFiProvisioningHint();
int getWiFiConnectAttemptCounter();
int getWiFiLastDisconnectReason();

#endif // WIFI_CONFIG_H
