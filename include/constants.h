
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>
#include "time_utils.h"
#include "settings_schema.h"

// =================== VERSION INFO ===================
#define VERSION "1.0.0"
#define AUTHOR "KylRuch"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// =================== SENSOR CONFIG ===================
#define RXD2 16
#define TXD2 21
#define COM_ULTRASONIC 0x55
#define SENSOR_OFFSET_MM 390.0
// Spike and prediction retry limits are now configurable via runtime settings
#define SENSOR_SPIKE_RETRY_DELAY_MS 2000

// Enable detailed UART debug logs that are written to SD via `LOGI`/`LOGW`.
// To enable, either uncomment the following line or pass -DSENSOR_UART_DEBUG in build flags.
// #define SENSOR_UART_DEBUG

// =================== DISPLAY CONFIG ===================
#define SCREEN_W 170
#define SCREEN_H 320

enum DisplayPage {
	NONE_PAGE_NUM = -1,
	GAUGE_PAGE_NUM,
	GRAPHE_PAGE_NUM,
	DATA_PAGE_NUM,
	INFO_PAGE_NUM,
	RESTART_ACTION_PAGE,
	AP_ACTION_PAGE,
	FACTORY_RESET_ACTION_PAGE
};

enum DisplayMode {
	STANDBY_MODE_NUM,
	AUTO_MODE_NUM,
	MANU_MODE_NUM,
	SETTING_MODE_NUM
};

// =================== MEASUREMENT STATUS ===================
#define MESURE_READY 1
#define ANY_MESURE_READY 0
#define MESURE_ISSUS -1

// =================== HISTORY BUFFER ===================
#define HISTORY_SIZE 120
#define FILTER_SAMPLES 5

#define PREDICTION_HISTORY_SIZE 5
// IQR filter is now runtime-configurable via settings.sensor.enableIqrFilter

// =================== BUTTON CONFIG ===================
#define B1_PIN 0
#define B2_PIN 14

// =================== SD CARD CONFIG ===================
#define SD_SCK   12
#define SD_MOSI  11
#define SD_MISO  13

// =================== LOGGING CONFIG ===================
#define LOG_ARCHIVE_MAX_COUNT 30
#define SD_CS    10
#define LOG_MAX_SIZE (300 * 1024) // 300 ko

#define LOGI(msg) logSD(getCurrentDateTime() + " | INFO | " + msg)
#define LOGW(msg) logSD(getCurrentDateTime() + " | WARN | " + msg)
#define LOGE(msg) logSD(getCurrentDateTime() + " | ERROR | " + msg)
#define LOG(msg) LOGI(msg)

// =================== WIFI CONFIG ===================
#define WIFI_RETRY_DELAY_MIN 2000
const uint8_t WIFI_AP_PASSWORD_LENGTH = 8;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const unsigned long WIFI_MAX_RETRY_DELAY = 60000;
const unsigned long WIFI_AP_INACTIVITY_TIMEOUT = 10UL * 60UL * 1000UL;

// =================== WEBSERVER CONFIG ===================
extern const char* mdnsName;

#endif // CONSTANTS_H
