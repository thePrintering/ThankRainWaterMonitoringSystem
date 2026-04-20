#include "display.h"
#include "wifi_config.h"
#include "sensor.h"

// Draw WiFi signal strength indicator with 4 concentric arcs
// Quality ranges from 0 (disconnected) to 4 (excellent signal)
void drawWiFiArcsIcone(int x, int y, int wifiQualityRSSI, uint16_t colorA, uint16_t colorB, uint16_t bg) {
  if (wifiState == WIFI_DISABLED) {
    return;
  }

  // Convert RSSI to signal quality (1-4 bars)
  int quality = 0;
  if (Wifi.status() == WL_CONNECTED) {
    quality = rssiToQuality(wifiQualityRSSI);
  }

  // Draw background circle if specified
  if (bg) {
    sprite.fillCircle(x + 10, y + 10, 10, bg);
  }

  int cx = x + 10;
  int cy = y + 15;

  // Arcs drawn from 135° to 225° (quarter-circle to the upper-left)
  uint32_t startAngle = 135;
  uint32_t endAngle = 225;

  // Draw 1-4 arcs depending on signal quality
  // Higher quality = more arcs filled with colorA (active color)
  switch (quality) {
    case 1:
      // Low signal: only inner arc is active, outer arcs are grayed
      sprite.fillCircle(cx, cy, 2, colorA);
      sprite.drawSmoothArc(cx, cy, 5, 4, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 8, 7, startAngle, endAngle, colorB, bg, true);
      sprite.drawSmoothArc(cx, cy, 11, 10, startAngle, endAngle, colorB, bg, true);
      break;
    case 2:
      // Medium-low signal: 2 arcs active
      sprite.fillCircle(cx, cy, 2, colorA);
      sprite.drawSmoothArc(cx, cy, 5, 4, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 8, 7, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 11, 10, startAngle, endAngle, colorB, bg, true);
      break;
    case 3:
      // Medium-high signal: 3 arcs active
      sprite.fillCircle(cx, cy, 2, colorA);
      sprite.drawSmoothArc(cx, cy, 5, 4, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 8, 7, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 11, 10, startAngle, endAngle, colorA, bg, true);
      break;
    case 4:
      // Strong signal: all 4 arcs active
      sprite.fillCircle(cx, cy, 2, colorA);
      sprite.drawSmoothArc(cx, cy, 5, 4, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 8, 7, startAngle, endAngle, colorA, bg, true);
      sprite.drawSmoothArc(cx, cy, 11, 10, startAngle, endAngle, colorA, bg, true);
      break;
    default:
      // Disconnected: all arcs grayed + red "X" overlay
      sprite.fillCircle(cx, cy, 2, colorB);
      sprite.drawSmoothArc(cx, cy, 5, 4, startAngle, endAngle, colorB, bg, true);
      sprite.drawSmoothArc(cx, cy, 8, 7, startAngle, endAngle, colorB, bg, true);
      sprite.drawSmoothArc(cx, cy, 11, 10, startAngle, endAngle, colorB, bg, true);
      sprite.drawLine(x, y + 20, x + 19, y, colorA);
      sprite.drawLine(x + 1, y + 20, x + 20, y, colorA);
      sprite.drawCircle(x + 10, y + 10, 10, TFT_RED);
  }
}

// Draw ultrasonic sensor connection status indicator
// Similar to WiFi icon but uses vertical (180°) orientation
void drawWaterLevelSensorStatusIcon(int x, int y, bool connected, uint16_t colorA, uint16_t colorB, uint16_t bg) {
  // Draw background circle if specified
  if (bg) {
    sprite.fillCircle(x + 10, y + 10, 10, bg);
  }

  int cx = x + 10;
  int cy = y + 5;

  // Arcs at bottom (315° to 45°, opposite of WiFi)
  uint32_t startAngle = 315;
  uint32_t endAngle = 45;

  if (connected) {
    // Sensor OK: draw filled dot and active arcs
    sprite.fillCircle(cx, cy, 2, colorA);
    sprite.fillRect(cx - 5, cy - 2, 10, 2, colorA);
    sprite.drawSmoothArc(cx, cy, 5, 5, startAngle, endAngle, colorA, bg, true);
    sprite.drawSmoothArc(cx, cy, 8, 8, startAngle, endAngle, colorA, bg, true);
    sprite.drawSmoothArc(cx, cy, 11, 11, startAngle, endAngle, colorA, bg, true);
  } else {
    // Sensor error: gray arcs + red "X"
    sprite.fillCircle(cx, cy, 2, colorB);
    sprite.fillRect(cx - 5, cy - 2, 10, 2, colorB);
    sprite.drawSmoothArc(cx, cy, 5, 5, startAngle, endAngle, colorB, bg, true);
    sprite.drawSmoothArc(cx, cy, 8, 8, startAngle, endAngle, colorB, bg, true);
    sprite.drawSmoothArc(cx, cy, 11, 11, startAngle, endAngle, colorB, bg, true);
    sprite.drawLine(x, y + 20, x + 19, y, colorA);
    sprite.drawLine(x + 1, y + 20, x + 20, y, colorA);
    sprite.drawCircle(x + 10, y + 10, 10, TFT_RED);
  }
}