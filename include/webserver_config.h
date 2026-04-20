#ifndef WEBSERVER_CONFIG_H
#define WEBSERVER_CONFIG_H

#include <ESPAsyncWebServer.h>
#include "sensor.h"
#include <Ticker.h>
#include "wifi_config.h"    // bring startMDNS()

// =================== WEBSERVER OBJECTS ===================
extern AsyncWebServer server;
extern AsyncWebSocket webSocket;
extern bool mdnsRunning;
extern Ticker rebootTimer;

// =================== FUNCTION DECLARATIONS ===================
void initWebServer();
void initWebSocket();
void updateWebSocket();
void notifyWebSocketClients(SensorData &data);
void startNetworkServices();
void relanceNetworkServices();

// wifi helper from wifi.cpp
void startMDNS();

#endif // WEBSERVER_CONFIG_H
