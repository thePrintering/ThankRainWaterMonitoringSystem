#ifndef OTA_H
#define OTA_H

// Initialize OTA update support.
void initOTA();

// Handle OTA updates. Call regularly from loop().
void updateOTA();

// True while OTA UI overlay (error/progress) should keep control of TFT rendering.
bool isOTAOverlayActive();

#endif // OTA_H
