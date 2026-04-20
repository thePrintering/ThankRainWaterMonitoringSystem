#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <Arduino.h>

void initWiFiProvisioning();
bool isWiFiProvisioningAPActive();
void startProvisioningAP(unsigned long now,
                         bool &networkServicesStarted,
                         void (*startNetworkServices)());
void stopProvisioningAP();
void checkProvisioningAPTimeout(unsigned long now);
String getProvisioningAPSsid();
String getProvisioningAPPassword();
String getProvisioningSessionToken();
void invalidateProvisioningSessionToken();
int getProvisioningAPClientCount();
unsigned long getProvisioningAPRemainingTime(unsigned long now);

#endif // WIFI_PROVISIONING_H
