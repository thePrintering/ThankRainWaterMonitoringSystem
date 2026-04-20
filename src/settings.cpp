#include "settings.h"
#include "constants.h"
#include "settings_schema.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <SD.h>

Settings settings;

void loadSettings() {
  // Keep firmware operational with defaults if SD is unavailable at boot.
  if (!ensureSDReady()) {
    Serial.println("[SD] indisponible -> valeurs par defaut");
    return;
  }

  if (!sdExists("/settings.json")) {
    Serial.println("settings.json absent -> valeurs par defaut");
    return;
  }

  File f = SD.open("/settings.json", FILE_READ);
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    // Invalid JSON should not block startup; defaults remain active.
    Serial.println("JSON invalide");
    return;
  }

  applySettingsFromJson(settings, doc);

  Serial.println("Settings charges");
}

void saveSettings() {
  // Save is explicit and synchronous so web/settings UI gets immediate persistence.
  if (!ensureSDReady()) {
    Serial.println("[SD] indisponible, impossible de sauvegarder settings.json");
    return;
  }

  JsonDocument doc;

  exportSettingsToStorageJson(settings, doc);

  if (sdExists("/settings.json")) {
    removeSDFile("/settings.json");
  }

  File f = SD.open("/settings.json", FILE_WRITE);
  if (!f) {
    Serial.println("[SD] Impossible d'ouvrir settings.json");
    return;
  }

  serializeJsonPretty(doc, f);
  f.flush();
  f.close();

  Serial.println("[SD] Settings sauvegardes sur SD");
}
