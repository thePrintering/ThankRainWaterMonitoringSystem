#include "storage.h"
#include "time_utils.h"
#include "constants.h"
#include "settings.h"
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

SPIClass spiSD(FSPI);

File openSDFile(const String &path, const char *mode);

namespace {

constexpr uint8_t SD_MOUNT_RETRIES = 3;
constexpr unsigned long SD_RETRY_BACKOFF_MS = 5000UL;
constexpr unsigned long SD_HEALTH_CHECK_MS = 10000UL;
constexpr const char *MONTHLY_SUMMARY_HEADER = "date,percent,volume,distance,count";

bool scanLatestDailyCsvRecursive(File dir, String &latestName);
String getDailyHistoryFilePath(int year, int month, int day);
String normalizeFileName(const String &name);
bool removeSDFileOnce(const String &path);
bool removeSDPathRecursive(const String &path);

bool sdBusInitialized = false;
unsigned long lastMountAttemptMs = 0;
unsigned long lastHealthCheckMs = 0;
SemaphoreHandle_t logMutex = nullptr;

const char *cardTypeToName(uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

bool mountSD(bool forceLog) {
  // Initialize SPI bus only once; remounts should not recreate the bus every time.
  if (!sdBusInitialized) {
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdBusInitialized = true;
  }

  for (uint8_t attempt = 1; attempt <= SD_MOUNT_RETRIES; attempt++) {
    if (SD.begin(SD_CS, spiSD)) {
      sdOK = true;
      lastMountAttemptMs = millis();
      lastHealthCheckMs = millis();

      uint8_t cardType = SD.cardType();
      uint64_t totalBytes = SD.totalBytes();
      uint64_t usedBytes = SD.usedBytes();

      if (forceLog || attempt > 1) {
        Serial.printf("[SD] Carte SD prete (type=%s total=%lluMB used=%lluMB)\n",
                      cardTypeToName(cardType),
                      (unsigned long long)(totalBytes / (1024ULL * 1024ULL)),
                      (unsigned long long)(usedBytes / (1024ULL * 1024ULL)));
      }

      return true;
    }

    delay(120);
  }

  sdOK = false;
  lastMountAttemptMs = millis();
  if (forceLog) {
    Serial.println("[SD] Carte SD non detectee");
  }
  return false;
}

bool quickHealthCheck() {
  // A root open/close is a cheap way to verify SD stack remains responsive.
  File root = SD.open("/");
  if (!root) {
    return false;
  }
  root.close();
  return true;
}

bool ensureFileExists(const char *path, const char *headerLine = nullptr) {
  if (sdExists(path)) {
    return true;
  }

  File file = openSDFile(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[SD] Echec creation fichier %s\n", path);
    return false;
  }

  if (headerLine != nullptr) {
    // CSV files are created with explicit header so parsers can skip first line safely.
    file.println(headerLine);
  }

  file.close();
  return true;
}

bool isDailyCsvFileName(const String &name) {
  if (name.length() != 14) {
    return false;
  }

  return isDigit(name[0]) && isDigit(name[1]) && isDigit(name[2]) && isDigit(name[3]) &&
         name[4] == '-' &&
         isDigit(name[5]) && isDigit(name[6]) &&
         name[7] == '-' &&
         isDigit(name[8]) && isDigit(name[9]) &&
         name.substring(10) == ".csv";
}

bool parseDailyCsvDateParts(const String &name, int &year, int &month, int &day) {
  if (!isDailyCsvFileName(name)) {
    return false;
  }

  year = name.substring(0, 4).toInt();
  month = name.substring(5, 7).toInt();
  day = name.substring(8, 10).toInt();

  if (year < 2000 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }

  return true;
}

bool scanLatestDailyCsvRecursive(File dir, String &latestName) {
  int scannedEntries = 0;
  while (true) {
    if ((++scannedEntries & 0x0F) == 0) {
      delay(0);
    }

    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    if (entry.isDirectory()) {
      scanLatestDailyCsvRecursive(entry, latestName);
      entry.close();
      continue;
    }

    String name = entry.name();
    if (name.length() > 0 && name[0] == '/') {
      name = name.substring(1);
    }
    int slashIndex = name.lastIndexOf('/');
    if (slashIndex >= 0) {
      name = name.substring(slashIndex + 1);
    }
    if (isDailyCsvFileName(name) && (latestName.isEmpty() || name > latestName)) {
      latestName = name;
    }

    entry.close();
  }

  return true;
}

bool findLatestDailyCsvDateOnSd(int &year, int &month, int &day) {
  File historyRoot = openSDFile("/history", FILE_READ);
  if (!historyRoot) {
    return false;
  }

  String latestName = "";
  scanLatestDailyCsvRecursive(historyRoot, latestName);
  historyRoot.close();

  if (latestName.isEmpty()) {
    return false;
  }

  return parseDailyCsvDateParts(latestName, year, month, day);
}

String normalizeFileName(const String &name) {
  if (name.length() > 0 && name[0] == '/') {
    return name.substring(1);
  }
  return name;
}

String getDailyHistoryDirectoryPath(int year, int month) {
  char path[32];
  snprintf(path, sizeof(path), "/history/%04d/%04d-%02d", year, year, month);
  return String(path);
}

String getLegacyDailyHistoryYearDirectoryPath(int year) {
  char path[24];
  snprintf(path, sizeof(path), "/history/%04d", year);
  return String(path);
}

String getMonthlySummaryDirectoryPath(int year) {
  char path[40];
  snprintf(path, sizeof(path), "/history/%04d/monthsummarie", year);
  return String(path);
}

String getMonthlySummaryFilePath(int year, int month) {
  char path[48];
  snprintf(path, sizeof(path), "/history/%04d/monthsummarie/avg-%02d.csv", year, month);
  return String(path);
}

String getDailyHistoryFilePath(int year, int month, int day) {
  char path[48];
  snprintf(path, sizeof(path), "/history/%04d/%04d-%02d/%04d-%02d-%02d.csv", year, year, month, year, month, day);
  return String(path);
}

String resolveDailyHistoryFilePath(int year, int month, int day) {
  return getDailyHistoryFilePath(year, month, day);
}

String resolveMonthlySummaryFilePath(int year, int month) {
  return getMonthlySummaryFilePath(year, month);
}

bool isMonthlySummaryCsvFileName(const String &name, int &month) {
  if (name.length() != 10) {
    return false;
  }

  if (!name.startsWith("avg-") || !name.endsWith(".csv")) {
    return false;
  }

  if (!isDigit(name[4]) || !isDigit(name[5])) {
    return false;
  }

  month = name.substring(4, 6).toInt();
  return month >= 1 && month <= 12;
}


bool findNextDailyCsvFile(const String &after, String &nextFile) {
  File root = openSDFile("/", FILE_READ);
  if (!root) {
    return false;
  }

  String best = "";
  while (true) {
    File file = root.openNextFile();
    if (!file) {
      break;
    }

    String normalized = normalizeFileName(file.name());
    if (!file.isDirectory() && isDailyCsvFileName(normalized) && normalized > after) {
      if (best.isEmpty() || normalized < best) {
        best = normalized;
      }
    }

    file.close();
  }

  root.close();

  if (best.isEmpty()) {
    return false;
  }

  nextFile = best;
  return true;
}

bool parseCsvLine(const String &line, SensorData &out) {
  int p1 = line.indexOf(',');
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);

  if (p1 < 0 || p2 < 0 || p3 < 0) {
    return false;
  }

  out.time = line.substring(0, p1);
  out.percent = line.substring(p1 + 1, p2).toFloat();
  out.volume = line.substring(p2 + 1, p3).toFloat();
  out.distance = line.substring(p3 + 1).toFloat();

  return out.time.length() > 0;
}

bool parseCsvMeasurementLine(const String &line, SensorData &out) {
  // Ignore empty lines and CSV header rows.
  if (line.length() == 0 || line.startsWith("datetime,")) {
    return false;
  }

  return parseCsvLine(line, out);
}

bool parseDailyDateFromPath(const String &path, String &dateKey, String &monthKey) {
  String normalized = normalizeFileName(path);
  int slashIndex = normalized.lastIndexOf('/');
  if (slashIndex >= 0) {
    normalized = normalized.substring(slashIndex + 1);
  }

  if (!isDailyCsvFileName(normalized)) {
    return false;
  }

  dateKey = normalized.substring(0, 10);
  monthKey = normalized.substring(0, 7);
  return true;
}

bool ensureMonthlySummaryDirectory(int year) {
  if (!ensureSDReady()) {
    return false;
  }

  if (!SD.exists("/history")) {
    SD.mkdir("/history");
  }

  String summaryDir = getMonthlySummaryDirectoryPath(year);
  if (!SD.exists(summaryDir)) {
    SD.mkdir(summaryDir);
  }

  return SD.exists(summaryDir);
}

bool ensureDailyHistoryDirectory(int year, int month) {
  if (!ensureSDReady()) {
    return false;
  }

  if (!SD.exists("/history")) {
    SD.mkdir("/history");
  }

  String yearDir = getLegacyDailyHistoryYearDirectoryPath(year);
  if (!SD.exists(yearDir)) {
    SD.mkdir(yearDir);
  }

  String monthDir = getDailyHistoryDirectoryPath(year, month);
  if (!SD.exists(monthDir)) {
    SD.mkdir(monthDir);
  }

  return SD.exists(monthDir);
}

bool isDirectoryEmpty(const String &path) {
  File dir = openSDFile(path, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return false;
  }

  File entry = dir.openNextFile();
  bool empty = !entry;
  if (entry) {
    entry.close();
  }
  dir.close();
  return empty;
}

void pruneHistoryDirectoriesIfEmpty(int year, int month) {
  if (!ensureSDReady()) {
    return;
  }

  String monthDir = getDailyHistoryDirectoryPath(year, month);
  String yearDir = getLegacyDailyHistoryYearDirectoryPath(year);

  if (SD.exists(monthDir) && isDirectoryEmpty(monthDir)) {
    SD.rmdir(monthDir.c_str());
  }

  if (SD.exists(yearDir) && isDirectoryEmpty(yearDir)) {
    SD.rmdir(yearDir.c_str());
  }

  String summaryDir = getMonthlySummaryDirectoryPath(year);
  if (SD.exists(summaryDir) && isDirectoryEmpty(summaryDir)) {
    SD.rmdir(summaryDir.c_str());
  }
}

String getMonthlySummaryFilePathFromKey(const String &monthKey) {
  int year = monthKey.substring(0, 4).toInt();
  int month = monthKey.substring(5, 7).toInt();
  return getMonthlySummaryFilePath(year, month);
}

bool parseMonthlySummaryLine(const String &line,
                             String &date,
                             float &percent,
                             float &volume,
                             float &distance,
                             int &count) {
  int p1 = line.indexOf(',');
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);
  int p4 = line.indexOf(',', p3 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) {
    return false;
  }

  date = line.substring(0, p1);
  percent = line.substring(p1 + 1, p2).toFloat();
  volume = line.substring(p2 + 1, p3).toFloat();
  distance = line.substring(p3 + 1, p4).toFloat();
  count = line.substring(p4 + 1).toInt();
  return date.length() == 10;
}

bool computeDailyAverageWithCount(const char *path, SensorData &avg, int &count) {
  avg = {0, 0, 0};
  count = 0;

  if (!sdExists(path)) {
    return false;
  }

  File file = openSDFile(path, FILE_READ);
  if (!file) {
    return false;
  }

  if (file.available()) {
    file.readStringUntil('\n');
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    SensorData data;
    if (!parseCsvMeasurementLine(line, data)) {
      continue;
    }

    avg.percent += data.percent;
    avg.volume += data.volume;
    avg.distance += data.distance;
    count++;
  }

  file.close();

  if (count <= 0) {
    avg = {0, 0, 0};
    return true;
  }

  avg.percent /= count;
  avg.volume /= count;
  avg.distance /= count;
  return true;
}

void writeMonthlySummaryRow(File &file,
                            const String &date,
                            const SensorData &avg,
                            int count) {
  file.print(date);
  file.print(',');
  file.print(avg.percent, 2);
  file.print(',');
  file.print(avg.volume, 2);
  file.print(',');
  file.print(avg.distance, 2);
  file.print(',');
  file.println(count);
}

bool upsertMonthlySummaryDay(const String &summaryPath,
                             const String &dateKey,
                             const SensorData &avg,
                             int count) {
  String tmpPath = summaryPath + ".tmp";

  File source = sdExists(summaryPath) ? openSDFile(summaryPath, FILE_READ) : File();
  File target = openSDFile(tmpPath, FILE_WRITE);
  if (!target) {
    if (source) source.close();
    return false;
  }

  target.println(MONTHLY_SUMMARY_HEADER);

  bool found = false;
  if (source) {
    if (source.available()) {
      source.readStringUntil('\n');
    }

    while (source.available()) {
      String line = source.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        continue;
      }

      String rowDate;
      float rowPercent = 0.0f;
      float rowVolume = 0.0f;
      float rowDistance = 0.0f;
      int rowCount = 0;

      if (!parseMonthlySummaryLine(line, rowDate, rowPercent, rowVolume, rowDistance, rowCount)) {
        continue;
      }

      if (rowDate == dateKey) {
        writeMonthlySummaryRow(target, dateKey, avg, count);
        found = true;
      } else {
        SensorData rowAvg;
        rowAvg.percent = rowPercent;
        rowAvg.volume = rowVolume;
        rowAvg.distance = rowDistance;
        writeMonthlySummaryRow(target, rowDate, rowAvg, rowCount);
      }
    }

    source.close();
  }

  if (!found) {
    writeMonthlySummaryRow(target, dateKey, avg, count);
  }

  target.close();

  if (sdExists(summaryPath) && !removeSDFile(summaryPath)) {
    removeSDFile(tmpPath);
    return false;
  }

  if (!SD.rename(tmpPath, summaryPath)) {
    removeSDFile(tmpPath);
    return false;
  }

  return true;
}

bool rebuildMonthlySummaryForMonth(int year, int month) {
  if (month < 1 || month > 12) {
    return false;
  }

  String summaryPath = getMonthlySummaryFilePath(year, month);
  if (!ensureMonthlySummaryDirectory(year)) {
    LOGE("[SD] Rebuild monthly summary abort: ensureMonthlySummaryDirectory failed year=" + String(year));
    return false;
  }

  String summaryDir = getMonthlySummaryDirectoryPath(year);
  if (!SD.exists(summaryDir)) {
    SD.mkdir(summaryDir);
  }

  String tmpPath = summaryPath + ".tmp";
  File out = openSDFile(tmpPath, FILE_WRITE);
  if (!out) {
    LOGE("[SD] Rebuild monthly summary abort: cannot open tmp file " + tmpPath);
    return false;
  }

  out.println(MONTHLY_SUMMARY_HEADER);
  int writtenRows = 0;

  for (int day = 1; day <= 31; day++) {
    String dailyPath = resolveDailyHistoryFilePath(year, month, day);
    if (!sdExists(dailyPath)) {
      continue;
    }

    SensorData avg;
    int count = 0;
    if (!computeDailyAverageWithCount(dailyPath.c_str(), avg, count) || count <= 0) {
      continue;
    }

    char dateKey[11];
    snprintf(dateKey, sizeof(dateKey), "%04d-%02d-%02d", year, month, day);
    writeMonthlySummaryRow(out, String(dateKey), avg, count);
    writtenRows++;
  }

  out.close();

  if (writtenRows == 0) {
    removeSDFile(tmpPath);
    removeSDFile(summaryPath);
    return true;
  }

  if (sdExists(summaryPath) && !removeSDFile(summaryPath)) {
    LOGE("[SD] Rebuild monthly summary abort: cannot remove existing summary " + summaryPath);
    removeSDFile(tmpPath);
    return false;
  }

  if (!SD.rename(tmpPath, summaryPath)) {
    LOGE("[SD] Rebuild monthly summary abort: rename failed " + tmpPath + " -> " + summaryPath);
    removeSDFile(tmpPath);
    return false;
  }

  return true;
}

void updateMonthlySummaryFromDailyFile(const String &dailyPath) {
  String dateKey;
  String monthKey;
  if (!parseDailyDateFromPath(dailyPath, dateKey, monthKey)) {
    return;
  }

  SensorData avg;
  int count = 0;
  if (!computeDailyAverageWithCount(dailyPath.c_str(), avg, count) || count <= 0) {
    return;
  }

  int year = monthKey.substring(0, 4).toInt();
  ensureMonthlySummaryDirectory(year);
  String summaryDir = getMonthlySummaryDirectoryPath(year);
  if (!SD.exists(summaryDir)) {
    SD.mkdir(summaryDir);
  }
  String summaryPath = getMonthlySummaryFilePathFromKey(monthKey);
  upsertMonthlySummaryDay(summaryPath, dateKey, avg, count);
}

void pushRecentMeasurement(SensorData (&buffer)[HISTORY_SIZE], int &count, const SensorData &value) {
  if (count < HISTORY_SIZE) {
    buffer[count++] = value;
  }
}

void pushLastMeasurement(SensorData (&buffer)[HISTORY_SIZE], int &count, int &start, const SensorData &value) {
  if (count < HISTORY_SIZE) {
    buffer[count++] = value;
    return;
  }

  buffer[start] = value;
  start = (start + 1) % HISTORY_SIZE;
}

void readMeasurementsFromFile(const String &path,
                              SensorData (&buffer)[HISTORY_SIZE],
                              int &count,
                              int &start) {
  if (!sdExists(path)) {
    return;
  }

  File file = openSDFile(path, FILE_READ);
  if (!file) {
    return;
  }

  if (file.available()) {
    file.readStringUntil('\n');
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    SensorData data;
    if (parseCsvMeasurementLine(line, data)) {
      pushLastMeasurement(buffer, count, start, data);
    }
  }

  file.close();
}

void readMeasurementsFromFileReverse(const String &path,
                                     SensorData (&buffer)[HISTORY_SIZE],
                                     int &count) {
  if (count >= HISTORY_SIZE || !sdExists(path)) {
    return;
  }

  File file = openSDFile(path, FILE_READ);
  if (!file) {
    return;
  }

  long pos = static_cast<long>(file.size()) - 1;
  String reversedLine = "";

  auto processReversedLine = [&](String &lineBuffer) {
    if (lineBuffer.length() == 0 || count >= HISTORY_SIZE) {
      lineBuffer = "";
      return;
    }

    String line = "";
    line.reserve(lineBuffer.length());
    for (int i = lineBuffer.length() - 1; i >= 0; i--) {
      line += lineBuffer[i];
    }

    line.trim();

    SensorData data;
    if (parseCsvMeasurementLine(line, data)) {
      pushRecentMeasurement(buffer, count, data);
    }

    lineBuffer = "";
  };

  while (pos >= 0 && count < HISTORY_SIZE) {
    if (!file.seek(pos)) {
      break;
    }

    int byteRead = file.read();
    if (byteRead < 0) {
      break;
    }

    char c = static_cast<char>(byteRead);
    pos--;

    if (c == '\n') {
      processReversedLine(reversedLine);
      continue;
    }

    if (c != '\r') {
      reversedLine += c;
    }
  }

  processReversedLine(reversedLine);
  file.close();
}

} // namespace

bool sdOK = false;

bool ensureSDReady() {
  const unsigned long nowMs = millis();

  if (!sdOK) {
    // Avoid hammering SD.begin() continuously when card is missing.
    if (nowMs - lastMountAttemptMs < SD_RETRY_BACKOFF_MS) {
      return false;
    }
    return mountSD(true);
  }

  // Health-check only every few seconds to keep normal operations lightweight.
  if (nowMs - lastHealthCheckMs < SD_HEALTH_CHECK_MS) {
    return true;
  }

  lastHealthCheckMs = nowMs;
  if (quickHealthCheck()) {
    return true;
  }

  Serial.println("[SD] Health check failed, tentative de remount");
  sdOK = false;
  return mountSD(true);
}

bool sdExists(const String &path) {
  if (!ensureSDReady()) {
    return false;
  }

  bool exists = SD.exists(path);
  if (!exists && !quickHealthCheck()) {
    sdOK = false;
  }
  return exists;
}

File openSDFile(const String &path, const char *mode) {
  if (!ensureSDReady()) {
    return File();
  }

  File file = SD.open(path, mode);
  if (file) {
    return file;
  }

  // A failed open can indicate a transient SD bus/card issue; force one remount and retry once.
  sdOK = false;
  if (!ensureSDReady()) {
    return File();
  }

  return SD.open(path, mode);
}

bool removeSDFile(const String &path) {
  if (!ensureSDReady()) {
    return false;
  }

  if (!SD.exists(path)) {
    return true;
  }

  return removeSDPathRecursive(path);
}

namespace {

bool removeSDFileOnce(const String &path) {
  if (SD.remove(path)) {
    return true;
  }

  sdOK = false;
  if (!ensureSDReady()) {
    return false;
  }

  return SD.remove(path);
}

bool removeSDPathRecursive(const String &path) {
  File entry = openSDFile(path, FILE_READ);
  if (!entry) {
    return false;
  }

  if (!entry.isDirectory()) {
    entry.close();
    return removeSDFileOnce(path);
  }

  while (true) {
    File child = entry.openNextFile();
    if (!child) {
      break;
    }

    String childPath = child.name();
    if (!childPath.startsWith("/")) {
      childPath = path;
      if (!childPath.endsWith("/")) {
        childPath += "/";
      }
      childPath += child.name();
    }

    bool childIsDir = child.isDirectory();
    child.close();

    bool ok = childIsDir ? removeSDPathRecursive(childPath) : removeSDFileOnce(childPath);
    if (!ok) {
      entry.close();
      return false;
    }
  }

  entry.close();
  if (SD.rmdir(path.c_str())) {
    return true;
  }

  sdOK = false;
  if (!ensureSDReady()) {
    return false;
  }

  return SD.rmdir(path.c_str());
}

} // namespace

String getSDCardTypeName() {
  if (!ensureSDReady()) {
    return "NONE";
  }
  return String(cardTypeToName(SD.cardType()));
}

uint64_t getSDTotalBytes() {
  if (!ensureSDReady()) {
    return 0;
  }
  return SD.totalBytes();
}

uint64_t getSDUsedBytes() {
  if (!ensureSDReady()) {
    return 0;
  }
  return SD.usedBytes();
}

void initSD() {
  if (!mountSD(true)) {
    LOGE("[SD] Carte SD non detectee");
    return;
  }

  if (!SD.exists("/history")) {
    SD.mkdir("/history");
  }

  bool logCreated = !sdExists("/log.txt") && ensureFileExists("/log.txt");
  if (!sdExists("/settings.json")) {
    Serial.println("[SD] settings.json absent (conservation des valeurs runtime tant qu'aucune sauvegarde explicite)");
    LOGI("[SD] settings.json absent (pas d'initialisation auto)");
  }
  if (logCreated) {
    Serial.println("[SD] Ajout du fichier log.txt");
    LOGI("[SD] Ajout du fichier log.txt");
  }

}

String getDailyLogFile() {
  time_t nowEpoch = time(nullptr);
  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);

  return getDailyHistoryFilePath(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

void writeHistoryToSD(const SensorData &data) {
  if (!ensureSDReady()) return;

  // Track the last logged date to detect day changes explicitly.
  static int lastLoggedYear = -1;
  static int lastLoggedMonth = -1;
  static int lastLoggedDay = -1;

  // Get current date and time
  time_t nowEpoch = time(nullptr);
  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);

  int currentYear = timeinfo.tm_year + 1900;
  int currentMonth = timeinfo.tm_mon + 1;
  int currentDay = timeinfo.tm_mday;

  // Detect day change and log the transition
  if (lastLoggedDay != -1 && 
      (currentYear != lastLoggedYear || currentMonth != lastLoggedMonth || currentDay != lastLoggedDay)) {
    Serial.printf("[SD] ***Day change detected: %04d-%02d-%02d -> %04d-%02d-%02d***\n",
                  lastLoggedYear, lastLoggedMonth, lastLoggedDay,
                  currentYear, currentMonth, currentDay);
    LOGI(String("[SD] Day change: ") + lastLoggedYear + "-" + lastLoggedMonth + "-" + lastLoggedDay + 
        " -> " + currentYear + "-" + currentMonth + "-" + currentDay);
  }

  String fileName = getDailyLogFile();
  String dateKey;
  String monthKey;
  if (parseDailyDateFromPath(fileName, dateKey, monthKey)) {
    int year = monthKey.substring(0, 4).toInt();
    int month = monthKey.substring(5, 7).toInt();
    if (!ensureDailyHistoryDirectory(year, month)) {
      Serial.println("[SD] ERROR: Failed to create/verify history directory for year/month " + String(year) + "/" + String(month));
      LOGE("[SD] ERROR: Failed to create/verify history directory for year/month " + String(year) + "/" + String(month));
      return;
    }
  }
  bool newFile = !sdExists(fileName);

  // FILE_APPEND preserves existing rows and creates the file when missing.
  File f = openSDFile(fileName, FILE_APPEND);
  if (!f) {
    Serial.println("[SD] ERROR: Failed to open file for writing");
    LOGE("[SD] ERROR: Failed to open file for writing: " + fileName);
    return;
  }

  // If it's a new file, write the header
  if (newFile) {
    f.println("datetime,percent,volume,distance");
    f.flush();
    Serial.println("[SD] Created new file " + fileName);
    LOGI("[SD] Created new file " + fileName);
  }

  // Always stamp using current local time at write time.
  // This avoids persisting stale timestamps captured before NTP/TZ sync.
  String nowStr = getCurrentDateTime();
  if (nowStr == "--/--/---- --:--:--") {
    nowStr = data.time;
  }

  const char* tzEnv = getenv("TZ");

  // Write measurement data with explicit flush
  f.print(nowStr);
  f.print(",");
  f.print(data.percent, 2);
  f.print(",");
  f.print(data.volume, 2);
  f.print(",");
  f.println(data.distance, 2);
  
  // Explicit flush to ensure data reaches SD card before close
  f.flush();

  f.close();

  File verify = openSDFile(fileName, FILE_READ);
  if (verify) {
    size_t writtenSize = verify.size();
    verify.close();
    Serial.printf("[SD] Verified file=%s size=%u bytes\n", fileName.c_str(), (unsigned)writtenSize);
  }
  
  Serial.printf("[SD] write csvTime=%s sensorTime=%s epoch=%lld TZ=%s\n",
                nowStr.c_str(),
                data.time.c_str(),
                (long long)nowEpoch,
                tzEnv ? tzEnv : "");
  Serial.println("[SD] Ecriture des donnees dans " + fileName);

  // Update last logged date for next day-change detection
  lastLoggedYear = currentYear;
  lastLoggedMonth = currentMonth;
  lastLoggedDay = currentDay;

  // Keep one monthly summary file with one averaged row per day.
  updateMonthlySummaryFromDailyFile(fileName);
}


#include <vector>
#include <algorithm>

void logSD(const String& msg) {
  if (!ensureSDReady()) return;

  if (logMutex == nullptr) {
    logMutex = xSemaphoreCreateMutex();
    if (logMutex == nullptr) {
      Serial.println("[SD] log mutex creation failed");
      return;
    }
  }

  xSemaphoreTake(logMutex, portMAX_DELAY);

  // Vérifie la taille du log et archive si > LOG_MAX_SIZE
  File logFile = openSDFile("/log.txt", FILE_READ);
  if (logFile && logFile.size() > LOG_MAX_SIZE) {
    logFile.close();
    // Archive: rename /log.txt to /log-YYYYMMDD.txt
    // Compute YYYYMMDD from system time to avoid relying on getCurrentDateTime() format
    String yyyymmdd = "00000000";
    time_t nowEpoch = time(nullptr);
    if (nowEpoch >= 1600000000) {
      struct tm timeinfo;
      localtime_r(&nowEpoch, &timeinfo);
      char dateBuf[9];
      snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      yyyymmdd = String(dateBuf);
    }

    String archiveName = "/log-" + yyyymmdd + ".txt";
    // Avoid overwriting existing archive for today
    int suffix = 1;
    String tryName = archiveName;
    while (sdExists(tryName)) {
      tryName = "/log-" + yyyymmdd + "-" + String(suffix++) + ".txt";
    }

    bool renamed = SD.rename("/log.txt", tryName);

    // Prune old archives, keep only LOG_ARCHIVE_MAX_COUNT most recent
    std::vector<String> archives;
    File root = openSDFile("/", FILE_READ);
    if (root) {
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        String name = entry.name();
        if (name.startsWith("/log-") && name.endsWith(".txt")) {
          archives.push_back(name);
        }
        entry.close();
      }
      root.close();
    }
    std::sort(archives.begin(), archives.end(), std::greater<String>()); // newest first
    while (archives.size() > LOG_ARCHIVE_MAX_COUNT) {
      removeSDFile(archives.back());
      archives.pop_back();
    }

    File newLog = openSDFile("/log.txt", FILE_WRITE);
    if (newLog) {
      if (renamed) {
        newLog.println("[SD] log.txt archived (auto, >1MB): " + tryName);
      } else {
        newLog.println("[SD] WARNING: log.txt archive attempt FAILED, kept existing log. attempted target=" + tryName);
      }
      newLog.close();
    } else {
      if (!renamed) {
        Serial.println("[SD] WARNING: log archive attempted but could not recreate /log.txt");
        LOGW("[SD] WARNING: log archive attempted but could not recreate /log.txt");
      } else {
        Serial.println("[SD] log.txt archived: " + tryName + " but failed to recreate /log.txt");
        LOGW("[SD] log.txt archived: " + tryName + " but failed to recreate /log.txt");
      }
    }
  } else if (logFile) {
    logFile.close();
  }

  File file = openSDFile("/log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Impossible d'ouvrir log.txt");
    xSemaphoreGive(logMutex);
    return;
  }

  file.println(msg);
  file.close();
  xSemaphoreGive(logMutex);
}

SensorData computeDailyAverage(const char *path) {
  if (!sdExists(path)) {
    return {0, 0, 0};
  }

  SensorData avg;
  int count = 0;
  if (!computeDailyAverageWithCount(path, avg, count)) {
    Serial.print("[SD] Impossible d'ouvrir " + String(path));
    LOGW("[SD] Impossible d'ouvrir " + String(path));
    return {0, 0, 0};
  }
  return avg;
}

void loadLast7Days(DailyAverageBuffer &buf, bool infoLog) {
  time_t now = time(nullptr);
  struct tm timeinfo;

  for (int i = 6; i >= 0; i--) {
    time_t dayTime = now - (i * 86400);
    localtime_r(&dayTime, &timeinfo);

    String path = resolveDailyHistoryFilePath(timeinfo.tm_year + 1900,
                          timeinfo.tm_mon + 1,
                          timeinfo.tm_mday);

    SensorData avg = computeDailyAverage(path.c_str());

    if (avg.percent >= 0) {
      buf.SensorDataBuffer[buf.index] = avg;
      buf.index = (buf.index + 1) % 7;

      if (buf.count < 7) {
        buf.count++;
      }
    }
  }

  buf.lastDay = localtime(&now)->tm_mday;
  if (infoLog) {
    Serial.println("[SD] Chargement des moyennes journalieres des 7 derniers jours");
    LOGI("[SD] Chargement des moyennes journalieres des 7 derniers jours");
  }
}

void loadLast120Measures() {
  extern SensorData history[HISTORY_SIZE];
  extern int historyIndex;
  extern bool historyFilled;

  historyIndex = 0;
  historyFilled = false;

  if (!ensureSDReady()) {
    return;
  }

  SensorData newestMeasures[HISTORY_SIZE];
  int newestCount = 0;

  int startYear = 0;
  int startMonth = 0;
  int startDay = 0;
  bool hasStartDate = findLatestDailyCsvDateOnSd(startYear, startMonth, startDay);

  if (!hasStartDate) {
    struct tm nowTm;
    if (getLocalTime(&nowTm)) {
      startYear = nowTm.tm_year + 1900;
      startMonth = nowTm.tm_mon + 1;
      startDay = nowTm.tm_mday;
      hasStartDate = true;
    }
  }

  if (hasStartDate) {
    struct tm startTm = {};
    startTm.tm_year = startYear - 1900;
    startTm.tm_mon = startMonth - 1;
    startTm.tm_mday = startDay;
    startTm.tm_hour = 12; // Avoid DST edge at midnight when subtracting days.

    time_t startEpoch = mktime(&startTm);
    const int maxLookbackDays = 730;

    for (int offset = 0; offset < maxLookbackDays && newestCount < HISTORY_SIZE; offset++) {
      time_t dayEpoch = startEpoch - (offset * 86400);
      struct tm dayTm;
      localtime_r(&dayEpoch, &dayTm);

      String path = resolveDailyHistoryFilePath(dayTm.tm_year + 1900,
                                                dayTm.tm_mon + 1,
                                                dayTm.tm_mday);

      if (sdExists(path)) {
        // Read only the tail of each file and stop as soon as 120 measurements are loaded.
        readMeasurementsFromFileReverse(path.c_str(), newestMeasures, newestCount);
      }

      // Keep SD scan cooperative during long historical lookups.
      yield();
    }
  }

  // Internal load is newest->oldest. Reverse once to keep history oldest->newest.
  for (int i = newestCount - 1; i >= 0; i--) {
    history[historyIndex] = newestMeasures[i];
    historyIndex++;
  }

  if (historyIndex >= HISTORY_SIZE) {
    historyIndex = 0;
    historyFilled = true;
  }
  Serial.println("[SD] Chargement des 120 dernieres mesures");
  LOGI("[SD] Chargement des 120 dernieres mesures");
}

String buildMonthAggregateJsonFromMonthlyFile(int year, int month) {
  if (month < 1 || month > 12) {
    return "{\"labels\":[],\"values\":[]}";
  }

  rebuildMonthlySummaryForMonth(year, month);
  String summaryPath = getMonthlySummaryFilePath(year, month);
  if (!sdExists(summaryPath)) {
    return "{\"labels\":[],\"values\":[]}";
  }

  File file = openSDFile(summaryPath, FILE_READ);
  if (!file) {
    LOGW("[SD] Build month aggregate JSON abort: open failed " + summaryPath);
    return "{\"labels\":[],\"values\":[]}";
  }

  if (file.available()) {
    file.readStringUntil('\n');
  }

  String json = "{\"labels\":[";
  bool first = true;
  String values = "\"values\":[";
  bool firstValue = true;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    String date;
    float percent = 0.0f;
    float volume = 0.0f;
    float distance = 0.0f;
    int count = 0;
    if (!parseMonthlySummaryLine(line, date, percent, volume, distance, count)) {
      continue;
    }

    if (!first) {
      json += ",";
    }
    json += "\"" + date + "\"";
    first = false;

    if (!firstValue) {
      values += ",";
    }
    values += String(percent, 2);
    firstValue = false;
  }

  file.close();

  LOGI("[SD] Build month aggregate JSON done year=" + String(year) + " month=" + String(month));

  json += "],";
  values += "]}";
  json += values;
  return json;
}

String buildYearAggregateJsonFromMonthlyFiles(int year) {
  float monthAvg[12] = {0.0f};
  bool monthPresent[12] = {false};

  for (int month = 1; month <= 12; month++) {
    rebuildMonthlySummaryForMonth(year, month);
    String summaryPath = getMonthlySummaryFilePath(year, month);

    if (!sdExists(summaryPath)) {
      continue;
    }

    File file = openSDFile(summaryPath, FILE_READ);
    if (!file) {
      continue;
    }

    if (file.available()) {
      file.readStringUntil('\n');
    }

    float sum = 0.0f;
    int days = 0;

    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        continue;
      }

      String date;
      float percent = 0.0f;
      float volume = 0.0f;
      float distance = 0.0f;
      int count = 0;
      if (!parseMonthlySummaryLine(line, date, percent, volume, distance, count)) {
        continue;
      }

      sum += percent;
      days++;
    }

    file.close();

    if (days > 0) {
      monthAvg[month - 1] = sum / days;
      monthPresent[month - 1] = true;
    }
  }

  String json = "{\"labels\":[";
  bool first = true;
  String values = "\"values\":[";
  bool firstValue = true;

  for (int month = 1; month <= 12; month++) {
    if (!monthPresent[month - 1]) {
      continue;
    }

    if (!first) {
      json += ",";
    }

    char label[8];
    snprintf(label, sizeof(label), "%04d-%02d", year, month);
    json += "\"";
    json += label;
    json += "\"";
    first = false;

    if (!firstValue) {
      values += ",";
    }
    values += String(monthAvg[month - 1], 2);
    firstValue = false;
  }

  json += "],";
  values += "]}";
  json += values;
  return json;
}

bool rebuildMonthlySummaryForDailyFile(const String &dailyFileName) {
  String dateKey;
  String monthKey;
  if (!parseDailyDateFromPath(dailyFileName, dateKey, monthKey)) {
    return false;
  }

  int month = monthKey.substring(5, 7).toInt();
  if (month < 1 || month > 12) {
    return false;
  }

  int year = monthKey.substring(0, 4).toInt();
  return rebuildMonthlySummaryForMonth(year, month);
}

void pruneDailyHistoryDirectoriesForFile(const String &dailyFileName) {
  String dateKey;
  String monthKey;
  if (!parseDailyDateFromPath(dailyFileName, dateKey, monthKey)) {
    return;
  }

  int year = monthKey.substring(0, 4).toInt();
  int month = monthKey.substring(5, 7).toInt();
  if (year <= 0 || month < 1 || month > 12) {
    return;
  }

  pruneHistoryDirectoriesIfEmpty(year, month);
}
