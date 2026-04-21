# ThankRain Water Monitoring System - Complete Setup Guide
---
## Disclaimer
This project is provided "as is", without any warranty of any kind, express or implied. I make no guarantees regarding its reliability, accuracy, or suitability for any particular purpose. Use it at your own risk. I am not responsible for any damage, data loss, hardware issues, or other problems that may occur as a result of using this project. This is a personal project, shared for educational and experimental purposes, and it may contain bugs, incomplete features, or unexpected behavior.


---
### Step 1: Hardware Preparation

-   **Ultrasonic Sensor SEN0208** - connect as the pinout [include/constants.h](include/constants.h) : - RXD2 (pin 16) - TXD2 (pin 21)\
-   **MicroSD cart** - Insert the microSD cart formated in FAT32 - Storgage size recommended : 4-32 GB
-   **ESP32-S3 LILYGO T-Display S3** - turn it on\

---
### Step 2: Compilation
---
### Step 3: First Booting (Access Point)
- If no WiFi credentials are already stored, the ESP remains on hold until the AP mode is opened from the LCD menu
- Active the AP portal via Wifi -> Active AP
- Connect to AP with the credential showned on the screen
- Open http://192.168.4.1 in one browser
> **Note:**
> The AP mode is temporary, stops after 10 minutes of inactivity or no client connected

---
### Step 4: Configuration

In the AP portal there are two form:

#### WiFi Configuration
- **Networks detected**: Scanner list of available WiFi networks
- **SSID**: Name of the main network to be used
- **Password**: WiFi key (minimum 8 characters)
- **"Initialize WiFi" button**: Save and test the connection
- If the SSID cannot be found or if the password is wrong, the portal displays an explicit error instead of looping in retries

#### Tank Configuration
- **Capacity (Liters)**: Total volume of the tank
      *Example: `5000` L*
- **Height (mm)**: Minimum water vertical distance → maximum
      *Example: `1190` mm*
- **Sensor mounting distance (mm)**: Distance from the sensor to the bottom of the tank
     *Example: `390` mm*
- **Button "Save config tank"**: Save the settings

> **Note:**
> - WiFi (SSID / password): stored in NVS, then reused at startup
> - Tank and basic parameters: stored on the SD card in `settings.json`
> - Advanced sensor parameters (number of samples, anti-spike threshold, average/median method, interval) are adjusted in the Web dashboard
> - The `settings.json` file is rewritten when you validate the settings

---
### Step 5: Complete Configuration via Web Dashboard

#### Time Zone (Time)
- Select from the presets of Europe:
- `Europe/Zurich (CET/CEST)` - Switzerland, France, Germany, Belgium
- `Europe/London (GMT/BST)` - United Kingdom
- `Europe/Lisbon (WET/WEST)` - Portugal, Ireland
- `Europe/Athens (EET/EEST)` - Greece, Bulgaria
- `UTC (GMT)` - GMT by default
- `Europe/Paris (winter)` - CET without DST
- **Or customized**: Enter a POSIX string for other areas

#### Measures
- **Reading period**: Interval between measurements (10 sec → 1 hour)
- Presets provided: 10 sec, 30 sec, 1 min, 5 min, 15 min, 30 min, 1 h

#### Sensor
- **Sensor offset**: Mounting distance (0 → 1000 mm)
- **Number of samples**: Number of readings per cycle (1 → 10)
- **Sample management**: `Medium` or `Median`
- **Spike detection**: Disaprove to big measure change and try another. (max. 4 times)
- **Spike detection threlshot**: Max delta between two mesure
- **Sampling interval**: Delay between each sample (40 → 2000 ms)

#### Display
- **Brightness**: 10% → 100%
- **Sting mode**: Inactivity before brightness drop
- **Page interval**: Time before automatic screen change

#### Buttons
- **Debounce time**: Anti-rebound for clicks (10 → 1000 ms)
- **Long press**: Long press detection threshold (1 → 4 seconds)

---
### Step 6: Daily Use

#### LCD display
The LCD change shown each page in circle. When a button is pressed, the screen turn bright. Now, when a button is pressed, the screen switch forward or backward the page:
- **Page Gauge**: Analog gauge of the current level
- **Graph Page**: History of the last 7 days
- **Data Page**: Numeric values (liter, %)
- **Info Page**: WiFi status, SD, IP address

To acces the setting page of the local screen, long-press the right button.

#### Web Dashboard
- **Status**: Real-time state (connection, SD, sensor)
- **Graph**: Complete history with CSV export
- **Logs**: Log `log.txt` with entries `INFO` / `WARN` / `ERROR`
- **File Manager**: SD management and file visualization

#### File Manager
- Access to all SD card files
- Uploads/download files
- **"See" button**: Direct display of supported files:
- `.csv` - Measurement data
- `.json` - Configuration, JSON logs
- `.txt` - Notes, text logs
- `.log` - System log files
- **"Download" button**: Recover files
- **"Delete" button**: Delete files and folders
- **"Upload" button**: Upload file in the current directory

#### System Menu

Access via the LCD screen (long-press on the right button):
- **Information**: WiFi/SD/sensor diagnostics (press any button to exit)
- **Display**: Screen regulations (brightness, rest)
- **Measures**: Lift period
- **WiFi**: Activation/deactivation, Manual AP Mode
- **Factory reset**: Complete reset with confirmation
- **Resart ESP**: Restart the ESP with confirmation

---
### Step 7: Maintenance and Troubleshooting

#### Logs System
Go to `http://citerne.local/logs` or use the file manager for:
- WiFi events (connect/disconnect/error) with `INFO` / `WARN` / `ERROR` levels
- Sensor status (OK/timeout/CRC error)
- SD management (mount/remount/file operations)
- Diagnostics mDNS, OTA and AP provisioning

#### Manual AP Mode
1. Go to **Menu → WiFi → AP Manual**
2. ESP starts in temporary AP mode for 10 minutes of max inactivity
3. Reconnect to the AP SSID displayed on the TFT screen to reconfigure

#### Factory Reset
1. **Menu → Factory reset** (with confirmation)
2. Delete `settings.json` and NVS WiFi credentials
3. Restarts

> **Note:**
> Historical data on SD is preserved

#### No WiFi
- Check SSID and password in the AP portal

#### Automatic WiFi Reconnection
- For transient errors, automatic retry with exponential backoff
- Max delay: 60 seconds between attempts
- The Data page displays the current WiFi status during these transitions
- AP/STA diagnoses are also visible in `logs.txt` with the level of severity

## File Structure

```
project-root/
├── data/                          # Fichiers web (CSS, HTML, JS)
│   ├── portal-index.html         # Portail setup WiFi + cuve (3.2.1)
│   ├── file-manager-index.html   # Gestionnaire SD avec viewer (3.2.1)
│   ├── dashboard-index.html      # Tableau de bord principal
│   ├── settings-index.html       # Configuration web complète
│   ├── *.css, *.js               # Stylesheets et scripts
│   └── favicon.png               # Icône
│
├── include/                       # Headers C++
│   ├── constants.h               # Configuration matérielle (pins, timeouts)
│   ├── settings.h                # Structures de configuration
│   ├── settings_schema.h         # Schémas et presets (3.2.1)
│   ├── wifi_config.h             # WiFi state machine
│   ├── display.h                 # Contrôle LCD
│   ├── sensor.h                  # Capteur ultrasonic
│   └── ...                        # Autres modules
│
├── src/                          # Code source C++
│   ├── main.cpp                  # Boucle principale
│   ├── wifi_config.cpp           # État WiFi et provisioning
│   ├── webserver_config.cpp      # API HTTP et web pages
│   ├── display_*.cpp             # Rendus LCD
│   ├── settings_schema.cpp       # Gestion presets (3.2.1)
│   └── ...                        # Autres modules
│
├── test/                         # Tests unitaires
│
├── platformio.ini                # Configuration PlatformIO
├── README.md                     # Documentation générale
├── REFACTORING.md                # Notes refactoring interne
└── SETUP_GUIDE.md              # CE FICHIER - Guide setup v3.2.1
```

### API Endpoints Clés
- `GET /get-settings` - Récupérer tous les paramètres + presets
- `POST /save-settings` - Sauvegarder les paramètres
- `GET /measure-status` - Diagnostic mesure (capteur, timing, dernière ligne)
- `GET /list-files` - Lister les fichiers CSV journaliers
- `POST /fm/upload` - Uploader un fichier sur la SD, avec `overwrite=1` pour remplacer un fichier existant
- `GET /csv?file=YYYY-MM-DD.csv` - Lire un fichier CSV
- `GET /wifi/scan` - Scanner WiFi disponibles
- `POST /wifi/provision` - Provisionning WiFi
- `POST /reboot` - Redémarrer l'ESP
- `GET /fm/list` - Lister fichiers SD

### Fichier d'Entrée matérielle
Configurable dans [include/constants.h](include/constants.h) :
```cpp
#define RXD2 16         // Capteur RX
#define TXD2 21         // Capteur TX
#define B1_PIN 0        // Bouton 1 (TFT)
#define B2_PIN 14       // Bouton 2 (TFT)
#define SD_CS 10        // CS ligne SD
#define SENSOR_OFFSET_MM 390.0  // Distance montage (valeur par défaut)
```
---

**Documentation created for ThankRain - April 2026**

---
Sujets à maintenir à jour :
- WiFi retry backoff strategy (exponential backoff parameters)
- OTA package handling and firmware size limits
- Offline mode persistence (pending.csv migration strategy)

------------------------------------------------------------------------

**ThankRain - April 2026**
