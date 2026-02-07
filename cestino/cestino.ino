/*
 * Smart Traffic Control System v2.0 - 4 Independent Lanes
 * ESP32 + A3144 Hall Sensors + TLC5947 LED Driver
 * 
 * Features:
 * - 4 independent lanes with dual sensors (entry/exit) per lane
 * - Real-time vehicle counting (entry increments, exit decrements)
 * - TLC5947 PWM LED driver (one chip, 24 channels)
 * - Smart priority & fairness with sensor validation
 * - Web dashboard with real-time monitoring
 * - CSV data logging for traffic analysis
 * - Sensor health monitoring and anomaly detection
 * - Wi-Fi TCP logging + HTTP server
 * 
 * Hardware:
 * - 8x A3144 Hall Effect Sensors (2 per lane: entry + exit)
 * - 1x TLC5947 24-channel PWM LED driver
 * - 12x Traffic LEDs (3 per lane: R, Y, G)
 * - ESP32 DevKit
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <Adafruit_TLC5947.h>
#include <FS.h>
#include <SPIFFS.h>

// ==================== Wi-Fi CONFIGURATION ====================
const char* WIFI_SSID = "Theotherguy";
const char* WIFI_PASSWORD = "0812baron.";

// ==================== TLC5947 CONFIGURATION ====================
#define TLC_NUM_BOARDS 1
#define TLC_DATA_PIN 13    // DIN (MOSI)
#define TLC_CLOCK_PIN 14   // CLK (SCK)
#define TLC_LATCH_PIN 15   // LAT (Latch)
#define TLC_BLANK_PIN -1   // Optional, use -1 if not connected

Adafruit_TLC5947 tlc = Adafruit_TLC5947(TLC_NUM_BOARDS, TLC_CLOCK_PIN, TLC_DATA_PIN, TLC_LATCH_PIN);

// LED brightness (0-4095 for TLC5947, 12-bit PWM)
#define LED_BRIGHTNESS_FULL 4095
#define LED_BRIGHTNESS_DIM 1024
#define LED_BRIGHTNESS_OFF 0

// ==================== TLC5947 CHANNEL MAPPING ====================
// Lane ONE (Channels 0-2)
const int ONE_RED_CH = 0;
const int ONE_YELLOW_CH = 1;
const int ONE_GREEN_CH = 2;

// Lane TWO (Channels 3-5)
const int TWO_RED_CH = 3;
const int TWO_YELLOW_CH = 4;
const int TWO_GREEN_CH = 5;

// Lane THREE (Channels 6-8)
const int THREE_RED_CH = 6;
const int THREE_YELLOW_CH = 7;
const int THREE_GREEN_CH = 8;

// Lane FOUR (Channels 9-11)
const int FOUR_RED_CH = 9;
const int FOUR_YELLOW_CH = 10;
const int FOUR_GREEN_CH = 11;

// Channels 12-23 are spare/unused

// ==================== A3144 HALL SENSOR PIN DEFINITIONS ====================
// Lane ONE (Entry & Exit sensors)
const int ONE_ENTRY_SENSOR = 32;
const int ONE_EXIT_SENSOR = 33;

// Lane TWO (Entry & Exit sensors)
const int TWO_ENTRY_SENSOR = 25;
const int TWO_EXIT_SENSOR = 26;

// Lane THREE (Entry & Exit sensors)
const int THREE_ENTRY_SENSOR = 27;
const int THREE_EXIT_SENSOR = 14;

// Lane FOUR (Entry & Exit sensors)
const int FOUR_ENTRY_SENSOR = 12;
const int FOUR_EXIT_SENSOR = 13;

// ==================== LANE ENABLE/DISABLE FLAGS ====================
const bool LANE_ONE_ENABLED = true;
const bool LANE_TWO_ENABLED = true;
const bool LANE_THREE_ENABLED = true;
const bool LANE_FOUR_ENABLED = true;

// ==================== SENSOR VALIDATION SETTINGS ====================
const unsigned long SENSOR_DEBOUNCE_TIME = 200;      // 200ms debounce
const unsigned long SENSOR_TIMEOUT = 30000;          // 30s max time vehicle on sensor
const unsigned long VALIDATION_WINDOW = 10000;       // 10s window for entry-exit validation
const int MAX_NEGATIVE_COUNT_THRESHOLD = -3;         // Alert if count goes below -3

// ==================== TIMING PARAMETERS ====================
const unsigned long MIN_GREEN_TIME = 5000;
const unsigned long BASE_GREEN_TIME = 8000;
const unsigned long MAX_GREEN_TIME = 25000;
const unsigned long YELLOW_TIME = 2000;
const unsigned long ALL_RED_TIME = 1000;
const unsigned long MAX_WAIT_TIME = 15000;
const unsigned long EXTENSION_PER_VEHICLE = 400;

// ==================== FAIRNESS PARAMETERS ====================
const float CONGESTION_WEIGHT = 0.6;
const float WAIT_TIME_WEIGHT = 0.3;
const float SENSOR_HEALTH_WEIGHT = 0.1;
const bool ENABLE_EMPTY_LANE_SKIP = true;

// ==================== CONGESTION MODEL ====================
// Choose congestion scoring method:
// "LINEAR"     - Simple proportion: score = vehicles/max
// "QUADRATIC"  - Exponential pressure: score = (vehicles/max)²
// "CUBIC"      - Extreme congestion emphasis: score = (vehicles/max)³
// "SQRT"       - Gentle curve: score = sqrt(vehicles/max)
#define CONGESTION_MODEL "QUADRATIC"  // 🔴 CHANGE THIS to test different models

// For small-scale testing (5-8 toy cars), use lower max to increase pressure sensitivity
// This makes each car have more impact on priority calculations
const float MAX_VEHICLES_PER_LANE = 10.0;  // Optimized for 1-8 vehicle testing
                                            // (was 30.0 for full-scale traffic)
// With max=10: 5 vehicles = 0.50 normalized (50% capacity feels urgent!)
// With max=30: 5 vehicles = 0.17 normalized (17% capacity feels empty)

// ==================== LOGGING & DATA EXPORT ====================
#define LOG_SERVER_PORT 23
WiFiServer logServer(LOG_SERVER_PORT);
WiFiClient logClient;
bool logClientConnected = false;

// Web server for dashboard
WebServer httpServer(80);

// CSV logging
const char* CSV_LOG_FILE = "/traffic_log.csv";
unsigned long lastCSVLogTime = 0;
const unsigned long CSV_LOG_INTERVAL = 60000; // Log every 60 seconds

// ==================== ENUMERATIONS ====================
enum Lane { ONE = 0, TWO = 1, THREE = 2, FOUR = 3 };
enum TrafficState { IDLE, DECISION, TRANSITION_YELLOW, ACTIVE_GREEN, ALL_RED_CLEAR };
enum SensorType { ENTRY, EXIT };

// ==================== SENSOR DATA STRUCTURE ====================
struct SensorData {
  int pin;
  SensorType type;
  volatile unsigned long lastTriggerTime;
  volatile bool isBlocked;         // Vehicle currently on sensor
  volatile unsigned long blockStartTime;
  volatile int triggerCount;       // Total triggers since startup
  bool healthy;                    // Sensor health status
  unsigned long lastHealthCheck;
};

// ==================== LANE DATA STRUCTURE ====================
struct LaneData {
  SensorData entrySensor;
  SensorData exitSensor;
  
  int redChannel;
  int yellowChannel;
  int greenChannel;
  
  volatile int vehicleCount;       // Real-time count
  int peakCount;                   // Peak count during this cycle
  int totalEntered;                // Lifetime entry count
  int totalExited;                 // Lifetime exit count
  
  unsigned long waitStartTime;
  unsigned long lastServiceTime;
  unsigned long greenStartTime;
  
  float priorityScore;
  bool hasVehicles;
  bool enabled;
  
  // Validation & anomaly detection
  int negativeCountEvents;         // Count of times count went negative
  unsigned long lastAnomalyTime;
  bool sensorAnomaly;              // Flag for sensor issues
};

// ==================== GLOBAL VARIABLES ====================
LaneData lanes[4];

TrafficState currentState = IDLE;
Lane activeLane = ONE;
Lane lastServedLane = FOUR;

unsigned long stateStartTime = 0;
bool systemInitialized = false;

// System statistics
unsigned long systemStartTime = 0;
unsigned long totalVehiclesProcessed = 0;
unsigned long totalCycles = 0;

// ==================== LOGGING WRAPPERS ====================
void logPrint(const String& msg) {
  Serial.print(msg);
  if (logClientConnected && logClient.connected()) {
    logClient.print(msg);
  }
}

void logPrintln(const String& msg = "") {
  Serial.println(msg);
  if (logClientConnected && logClient.connected()) {
    logClient.println(msg);
  }
}

// ==================== CSV LOGGING ====================
void initCSVLog() {
  if (!SPIFFS.begin(true)) {
    logPrintln("⚠️  SPIFFS initialization failed!");
    return;
  }
  
  // Create CSV with headers if it doesn't exist
  if (!SPIFFS.exists(CSV_LOG_FILE)) {
    File file = SPIFFS.open(CSV_LOG_FILE, FILE_WRITE);
    if (file) {
      file.println("Timestamp,Lane,VehicleCount,TotalEntered,TotalExited,WaitTime,GreenDuration,PriorityScore,SensorHealth");
      file.close();
      logPrintln("✅ CSV log file created");
    }
  } else {
    logPrintln("✅ CSV log file exists");
  }
}

void appendCSVLog(Lane lane, unsigned long greenDuration) {
  File file = SPIFFS.open(CSV_LOG_FILE, FILE_APPEND);
  if (!file) {
    logPrintln("⚠️  Failed to open CSV log");
    return;
  }
  
  LaneData &l = lanes[lane];
  unsigned long waitTime = (l.waitStartTime > 0) ? (millis() - l.waitStartTime) : 0;
  bool sensorHealth = l.entrySensor.healthy && l.exitSensor.healthy;
  
  // Format: Timestamp,Lane,VehicleCount,TotalEntered,TotalExited,WaitTime,GreenDuration,PriorityScore,SensorHealth
  file.print(millis() / 1000);
  file.print(",");
  file.print(getLaneName(lane));
  file.print(",");
  file.print(l.vehicleCount);
  file.print(",");
  file.print(l.totalEntered);
  file.print(",");
  file.print(l.totalExited);
  file.print(",");
  file.print(waitTime);
  file.print(",");
  file.print(greenDuration);
  file.print(",");
  file.print(l.priorityScore);
  file.print(",");
  file.println(sensorHealth ? "OK" : "FAULT");
  
  file.close();
}

String getCSVData() {
  if (!SPIFFS.exists(CSV_LOG_FILE)) {
    return "No data available";
  }
  
  File file = SPIFFS.open(CSV_LOG_FILE, FILE_READ);
  if (!file) {
    return "Error reading file";
  }
  
  String data = file.readString();
  file.close();
  return data;
}

void clearCSVLog() {
  SPIFFS.remove(CSV_LOG_FILE);
  initCSVLog();
  logPrintln("🗑️  CSV log cleared");
}

// ==================== INTERRUPT HANDLERS ====================
// Entry sensor ISRs
void IRAM_ATTR entryOneSensorISR() {
  if (!lanes[ONE].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[ONE].entrySensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[ONE].vehicleCount++;
      lanes[ONE].totalEntered++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR entryTwoSensorISR() {
  if (!lanes[TWO].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[TWO].entrySensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[TWO].vehicleCount++;
      lanes[TWO].totalEntered++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR entryThreeSensorISR() {
  if (!lanes[THREE].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[THREE].entrySensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[THREE].vehicleCount++;
      lanes[THREE].totalEntered++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR entryFourSensorISR() {
  if (!lanes[FOUR].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[FOUR].entrySensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[FOUR].vehicleCount++;
      lanes[FOUR].totalEntered++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

// Exit sensor ISRs
void IRAM_ATTR exitOneSensorISR() {
  if (!lanes[ONE].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[ONE].exitSensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[ONE].vehicleCount--;
      lanes[ONE].totalExited++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR exitTwoSensorISR() {
  if (!lanes[TWO].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[TWO].exitSensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[TWO].vehicleCount--;
      lanes[TWO].totalExited++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR exitThreeSensorISR() {
  if (!lanes[THREE].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[THREE].exitSensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[THREE].vehicleCount--;
      lanes[THREE].totalExited++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

void IRAM_ATTR exitFourSensorISR() {
  if (!lanes[FOUR].enabled) return;
  unsigned long now = millis();
  SensorData &s = lanes[FOUR].exitSensor;
  
  if (now - s.lastTriggerTime >= SENSOR_DEBOUNCE_TIME) {
    if (!s.isBlocked) {
      lanes[FOUR].vehicleCount--;
      lanes[FOUR].totalExited++;
      s.isBlocked = true;
      s.blockStartTime = now;
      s.triggerCount++;
    }
    s.lastTriggerTime = now;
  }
}

// ==================== LED CONTROL (TLC5947) ====================
void setLaneLights(Lane lane, bool red, bool yellow, bool green) {
  LaneData &l = lanes[lane];
  
  if (!l.enabled) return;
  
  tlc.setPWM(l.redChannel, red ? LED_BRIGHTNESS_FULL : LED_BRIGHTNESS_OFF);
  tlc.setPWM(l.yellowChannel, yellow ? LED_BRIGHTNESS_FULL : LED_BRIGHTNESS_OFF);
  tlc.setPWM(l.greenChannel, green ? LED_BRIGHTNESS_FULL : LED_BRIGHTNESS_OFF);
  tlc.write();
}

void setAllLanesRed() {
  for (int i = 0; i < 4; i++) {
    if (lanes[i].enabled) {
      setLaneLights((Lane)i, true, false, false);
    }
  }
}

void setLaneColor(Lane lane, char color) {
  setAllLanesRed();
  
  if (color == 'R') {
    setLaneLights(lane, true, false, false);
  } else if (color == 'Y') {
    setLaneLights(lane, false, true, false);
  } else if (color == 'G') {
    setLaneLights(lane, false, false, true);
  }
}

// ==================== SENSOR VALIDATION ====================
void checkSensorHealth() {
  unsigned long now = millis();
  
  for (int i = 0; i < 4; i++) {
    if (!lanes[i].enabled) continue;
    
    LaneData &l = lanes[i];
    
    // Check entry sensor
    if (l.entrySensor.isBlocked) {
      unsigned long blockDuration = now - l.entrySensor.blockStartTime;
      if (blockDuration > SENSOR_TIMEOUT) {
        logPrint("⚠️  Lane ");
        logPrint(getLaneName((Lane)i));
        logPrintln(" ENTRY sensor timeout - vehicle stuck or sensor fault!");
        l.entrySensor.healthy = false;
        l.entrySensor.isBlocked = false; // Reset
        l.sensorAnomaly = true;
        l.lastAnomalyTime = now;
      }
    }
    
    // Check exit sensor
    if (l.exitSensor.isBlocked) {
      unsigned long blockDuration = now - l.exitSensor.blockStartTime;
      if (blockDuration > SENSOR_TIMEOUT) {
        logPrint("⚠️  Lane ");
        logPrint(getLaneName((Lane)i));
        logPrintln(" EXIT sensor timeout - vehicle stuck or sensor fault!");
        l.exitSensor.healthy = false;
        l.exitSensor.isBlocked = false; // Reset
        l.sensorAnomaly = true;
        l.lastAnomalyTime = now;
      }
    }
    
    // Check for negative count anomaly
    if (l.vehicleCount < MAX_NEGATIVE_COUNT_THRESHOLD) {
      logPrint("⚠️  Lane ");
      logPrint(getLaneName((Lane)i));
      logPrint(" ANOMALY: Negative count (");
      logPrint(String(l.vehicleCount));
      logPrintln(") - possible sensor miscalibration!");
      l.negativeCountEvents++;
      l.sensorAnomaly = true;
      l.lastAnomalyTime = now;
      l.vehicleCount = 0; // Reset to prevent further issues
    }
    
    // Periodic health check
    if (now - l.entrySensor.lastHealthCheck > 60000) {
      l.entrySensor.healthy = true;
      l.entrySensor.lastHealthCheck = now;
    }
    if (now - l.exitSensor.lastHealthCheck > 60000) {
      l.exitSensor.healthy = true;
      l.exitSensor.lastHealthCheck = now;
    }
  }
}

void updateSensorBlockStatus() {
  for (int i = 0; i < 4; i++) {
    if (!lanes[i].enabled) continue;
    
    // Read sensor states (A3144 outputs LOW when magnet detected)
    bool entryActive = (digitalRead(lanes[i].entrySensor.pin) == LOW);
    bool exitActive = (digitalRead(lanes[i].exitSensor.pin) == LOW);
    
    // Clear blocked flag when sensor clears
    if (!entryActive && lanes[i].entrySensor.isBlocked) {
      lanes[i].entrySensor.isBlocked = false;
    }
    if (!exitActive && lanes[i].exitSensor.isBlocked) {
      lanes[i].exitSensor.isBlocked = false;
    }
  }
}

// ==================== INITIALIZATION ====================
void initializeSensor(SensorData &sensor, int pin, SensorType type) {
  sensor.pin = pin;
  sensor.type = type;
  sensor.lastTriggerTime = 0;
  sensor.isBlocked = false;
  sensor.blockStartTime = 0;
  sensor.triggerCount = 0;
  sensor.healthy = true;
  sensor.lastHealthCheck = millis();
  
  pinMode(pin, INPUT_PULLUP);
}

void initializeLane(Lane lane, int entryPin, int exitPin, 
                   int redCh, int yellowCh, int greenCh, bool enabled) {
  LaneData &l = lanes[lane];
  
  initializeSensor(l.entrySensor, entryPin, ENTRY);
  initializeSensor(l.exitSensor, exitPin, EXIT);
  
  l.redChannel = redCh;
  l.yellowChannel = yellowCh;
  l.greenChannel = greenCh;
  l.enabled = enabled;
  
  l.vehicleCount = 0;
  l.peakCount = 0;
  l.totalEntered = 0;
  l.totalExited = 0;
  l.waitStartTime = 0;
  l.lastServiceTime = 0;
  l.greenStartTime = 0;
  l.priorityScore = 0.0;
  l.hasVehicles = false;
  l.negativeCountEvents = 0;
  l.lastAnomalyTime = 0;
  l.sensorAnomaly = false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  logPrintln("\n╔════════════════════════════════════════════════════════╗");
  logPrintln("║  Smart Traffic Control System v2.0                    ║");
  logPrintln("║  ESP32 + A3144 Sensors + TLC5947 LED Driver           ║");
  logPrintln("╚════════════════════════════════════════════════════════╝");
  logPrintln();

  // Initialize TLC5947
  logPrintln("🔧 Initializing TLC5947 LED driver...");
  tlc.begin();
  tlc.write(); // Clear all outputs
  logPrintln("✅ TLC5947 initialized");
  
  // Initialize SPIFFS for CSV logging
  logPrintln("🔧 Initializing SPIFFS...");
  initCSVLog();
  
  // Connect to Wi-Fi
  logPrint("📡 Connecting to WiFi: ");
  logPrintln(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    logPrint(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logPrintln("\n✅ WiFi connected!");
    logPrint("   IP Address: ");
    logPrintln(WiFi.localIP().toString());
  } else {
    logPrintln("\n⚠️  WiFi connection failed - continuing without network");
  }

  // Start TCP logging server
  if (WiFi.status() == WL_CONNECTED) {
    logServer.begin();
    logServer.setNoDelay(true);
    logPrint("🌐 TCP Log Server: telnet ");
    logPrint(WiFi.localIP().toString());
    logPrintln(" 23");
  }

  // Initialize lanes
  logPrintln("\n🚦 Initializing lanes...");
  initializeLane(ONE, ONE_ENTRY_SENSOR, ONE_EXIT_SENSOR, 
                ONE_RED_CH, ONE_YELLOW_CH, ONE_GREEN_CH, LANE_ONE_ENABLED);
  initializeLane(TWO, TWO_ENTRY_SENSOR, TWO_EXIT_SENSOR, 
                TWO_RED_CH, TWO_YELLOW_CH, TWO_GREEN_CH, LANE_TWO_ENABLED);
  initializeLane(THREE, THREE_ENTRY_SENSOR, THREE_EXIT_SENSOR, 
                THREE_RED_CH, THREE_YELLOW_CH, THREE_GREEN_CH, LANE_THREE_ENABLED);
  initializeLane(FOUR, FOUR_ENTRY_SENSOR, FOUR_EXIT_SENSOR, 
                FOUR_RED_CH, FOUR_YELLOW_CH, FOUR_GREEN_CH, LANE_FOUR_ENABLED);
  
  // Display lane configuration
  for (int i = 0; i < 4; i++) {
    LaneData &l = lanes[i];
    logPrint("  Lane ");
    logPrint(getLaneName((Lane)i));
    logPrint(": ");
    logPrint(l.enabled ? "ENABLED ✓" : "DISABLED ✗");
    if (l.enabled) {
      logPrint(" | Sensors: Entry=GPIO");
      logPrint(String(l.entrySensor.pin));
      logPrint(", Exit=GPIO");
      logPrint(String(l.exitSensor.pin));
      logPrint(" | LEDs: Ch");
      logPrint(String(l.redChannel));
      logPrint(",");
      logPrint(String(l.yellowChannel));
      logPrint(",");
      logPrint(String(l.greenChannel));
    }
    logPrintln();
  }
  logPrintln();
  
  // Attach interrupts for all sensors
  logPrintln("⚡ Attaching sensor interrupts...");
  if (lanes[ONE].enabled) {
    attachInterrupt(digitalPinToInterrupt(ONE_ENTRY_SENSOR), entryOneSensorISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(ONE_EXIT_SENSOR), exitOneSensorISR, FALLING);
  }
  if (lanes[TWO].enabled) {
    attachInterrupt(digitalPinToInterrupt(TWO_ENTRY_SENSOR), entryTwoSensorISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(TWO_EXIT_SENSOR), exitTwoSensorISR, FALLING);
  }
  if (lanes[THREE].enabled) {
    attachInterrupt(digitalPinToInterrupt(THREE_ENTRY_SENSOR), entryThreeSensorISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(THREE_EXIT_SENSOR), exitThreeSensorISR, FALLING);
  }
  if (lanes[FOUR].enabled) {
    attachInterrupt(digitalPinToInterrupt(FOUR_ENTRY_SENSOR), entryFourSensorISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(FOUR_EXIT_SENSOR), exitFourSensorISR, FALLING);
  }
  logPrintln("✅ All sensor interrupts attached");
  
  // Start web server
  if (WiFi.status() == WL_CONNECTED) {
    setupWebServer();
    httpServer.begin();
    logPrint("🌐 Web Dashboard: http://");
    logPrintln(WiFi.localIP().toString());
  }
  
  // Start with all red
  setAllLanesRed();
  
  stateStartTime = millis();
  systemStartTime = millis();
  systemInitialized = true;
  
  logPrintln("\n✅ System initialized and ready!");
  logPrintln("   - Hall sensors: Active (FALLING edge triggers)");
  logPrintln("   - Vehicle counting: Entry +1, Exit -1");
  logPrintln("   - Sensor validation: Enabled");
  logPrintln("   - CSV logging: Active");
  if (WiFi.status() == WL_CONNECTED) {
    logPrintln("   - Web dashboard: Active");
  }
  logPrintln();
}

// ==================== CONGESTION & PRIORITY ====================
void updateCongestion() {
  unsigned long now = millis();
  
  for (int i = 0; i < 4; i++) {
    LaneData &l = lanes[i];
    
    if (!l.enabled) {
      l.hasVehicles = false;
      continue;
    }
    
    l.hasVehicles = (l.vehicleCount > 0);
    
    // Track peak count
    if (l.vehicleCount > l.peakCount) {
      l.peakCount = l.vehicleCount;
    }
    
    // Update wait time tracking
    if (l.hasVehicles && l.waitStartTime == 0) {
      l.waitStartTime = now;
    } else if (!l.hasVehicles && l.waitStartTime > 0) {
      l.waitStartTime = 0;
    }
  }
}

float calculateLanePriorityScore(Lane lane) {
  LaneData &l = lanes[lane];
  
  if (!l.enabled) return 0.0;
  
  unsigned long now = millis();
  
  // Congestion component with configurable model
  float normalizedCount = min(1.0f, l.vehicleCount / MAX_VEHICLES_PER_LANE);
  float congestionScore = 0.0;
  
  #if defined(CONGESTION_MODEL)
    #if CONGESTION_MODEL == "LINEAR"
      // Linear: Direct proportion (original method)
      congestionScore = normalizedCount;
      
    #elif CONGESTION_MODEL == "QUADRATIC"
      // Quadratic: Exponential pressure increase (RECOMMENDED)
      // Examples: 10 vehicles = 0.33² = 0.11
      //           20 vehicles = 0.67² = 0.45
      //           30 vehicles = 1.00² = 1.00
      congestionScore = normalizedCount * normalizedCount;
      
    #elif CONGESTION_MODEL == "CUBIC"
      // Cubic: Extreme congestion emphasis
      congestionScore = normalizedCount * normalizedCount * normalizedCount;
      
    #elif CONGESTION_MODEL == "SQRT"
      // Square root: Gentler curve for low congestion sensitivity
      congestionScore = sqrt(normalizedCount);
      
    #else
      // Default to linear if unknown model
      congestionScore = normalizedCount;
    #endif
  #else
    congestionScore = normalizedCount;
  #endif
  
  // Wait time component (keep linear - fairness requirement)
  float waitScore = 0.0;
  if (l.waitStartTime > 0) {
    unsigned long waitTime = now - l.waitStartTime;
    waitScore = min(1.0f, waitTime / (float)MAX_WAIT_TIME);
  }
  
  // Sensor health component (penalty for unhealthy sensors)
  float healthScore = (l.entrySensor.healthy && l.exitSensor.healthy) ? 1.0 : 0.5;
  
  // Combined weighted score
  float score = (congestionScore * CONGESTION_WEIGHT) + 
                (waitScore * WAIT_TIME_WEIGHT) + 
                (healthScore * SENSOR_HEALTH_WEIGHT);
  
  return score;
}

Lane selectNextLane() {
  updateCongestion();
  
  unsigned long now = millis();
  
  // Calculate priority scores
  for (int i = 0; i < 4; i++) {
    lanes[i].priorityScore = calculateLanePriorityScore((Lane)i);
  }
  
  // Find highest priority lane
  Lane bestLane = ONE;
  float highestScore = -1.0;
  
  for (int i = 0; i < 4; i++) {
    if (!lanes[i].enabled) continue;
    
    LaneData &l = lanes[i];
    
    // Starvation override
    if (l.hasVehicles && l.waitStartTime > 0) {
      unsigned long waitTime = now - l.waitStartTime;
      if (waitTime > MAX_WAIT_TIME) {
        logPrint("⚠️  STARVATION OVERRIDE: Lane ");
        logPrint(getLaneName((Lane)i));
        logPrint(" waited ");
        logPrint(String(waitTime / 1000.0));
        logPrintln("s");
        return (Lane)i;
      }
    }
    
    // Skip empty lanes
    if (ENABLE_EMPTY_LANE_SKIP && !l.hasVehicles) {
      continue;
    }
    
    if (l.priorityScore > highestScore) {
      highestScore = l.priorityScore;
      bestLane = (Lane)i;
    }
  }
  
  // Round-robin if all empty
  if (highestScore <= 0.0) {
    logPrintln("ℹ️  All lanes empty - round-robin");
    Lane nextLane = (Lane)((lastServedLane + 1) % 4);
    int attempts = 0;
    while (!lanes[nextLane].enabled && attempts < 4) {
      nextLane = (Lane)((nextLane + 1) % 4);
      attempts++;
    }
    return nextLane;
  }
  
  return bestLane;
}

// ==================== GREEN TIME CALCULATION ====================
unsigned long calculateGreenTime(Lane lane) {
  LaneData &l = lanes[lane];
  
  unsigned long greenTime = BASE_GREEN_TIME + (l.vehicleCount * EXTENSION_PER_VEHICLE);
  greenTime = max(MIN_GREEN_TIME, greenTime);
  greenTime = min(MAX_GREEN_TIME, greenTime);
  
  return greenTime;
}

// ==================== STATE MACHINE ====================
void handleDecisionState() {
  activeLane = selectNextLane();
  LaneData &l = lanes[activeLane];
  
  logPrintln("\n╔═══ DECISION PHASE ═══╗");
  logPrint("║ Selected: Lane ");
  logPrintln(getLaneName(activeLane));
  logPrint("║ Vehicles: ");
  logPrintln(String(l.vehicleCount));
  logPrint("║ Priority: ");
  logPrintln(String(l.priorityScore));
  logPrintln("╚═════════════════════╝");
  
  printSystemStatus();
  
  setAllLanesRed();
  currentState = ALL_RED_CLEAR;
  stateStartTime = millis();
}

void handleTransitionYellowState() {
  unsigned long elapsed = millis() - stateStartTime;
  
  if (elapsed >= YELLOW_TIME) {
    currentState = DECISION;
    stateStartTime = millis();
    totalCycles++;
  }
}

void handleAllRedClearState() {
  unsigned long elapsed = millis() - stateStartTime;
  
  if (elapsed >= ALL_RED_TIME) {
    LaneData &l = lanes[activeLane];
    
    l.lastServiceTime = millis();
    l.greenStartTime = millis();
    
    currentState = ACTIVE_GREEN;
    stateStartTime = millis();
    
    setLaneColor(activeLane, 'G');
    
    unsigned long greenDuration = calculateGreenTime(activeLane);
    logPrint("🟢 GREEN: Lane ");
    logPrint(getLaneName(activeLane));
    logPrint(" | Duration: ");
    logPrint(String(greenDuration / 1000.0));
    logPrint("s | Vehicles: ");
    logPrintln(String(l.vehicleCount));
  }
}

void handleActiveGreenState() {
  unsigned long elapsed = millis() - stateStartTime;
  unsigned long greenDuration = calculateGreenTime(activeLane);
  
  if (elapsed >= greenDuration) {
    LaneData &l = lanes[activeLane];
    
    // Log to CSV
    appendCSVLog(activeLane, greenDuration);
    
    // Update statistics
    totalVehiclesProcessed += l.totalExited;
    
    setLaneColor(activeLane, 'Y');
    
    logPrint("🟡 YELLOW: Lane ");
    logPrint(getLaneName(activeLane));
    logPrint(" | Remaining: ");
    logPrintln(String(l.vehicleCount));
    
    lastServedLane = activeLane;
    l.waitStartTime = 0;
    l.peakCount = 0;
    
    currentState = TRANSITION_YELLOW;
    stateStartTime = millis();
  }
}

// ==================== UTILITY FUNCTIONS ====================
const char* getLaneName(Lane lane) {
  switch(lane) {
    case ONE: return "ONE";
    case TWO: return "TWO";
    case THREE: return "THREE";
    case FOUR: return "FOUR";
    default: return "UNKNOWN";
  }
}

void printSystemStatus() {
  logPrintln("\n┌─── System Status ───┐");
  
  for (int i = 0; i < 4; i++) {
    LaneData &l = lanes[i];
    
    if (!l.enabled) {
      logPrint("│ Lane ");
      logPrint(getLaneName((Lane)i));
      logPrintln(": DISABLED");
      continue;
    }
    
    unsigned long wait = (l.waitStartTime > 0) ? (millis() - l.waitStartTime) : 0;
    bool healthy = l.entrySensor.healthy && l.exitSensor.healthy;
    
    logPrint("│ Lane ");
    logPrint(getLaneName((Lane)i));
    logPrint(": Count=");
    logPrint(String(l.vehicleCount));
    logPrint(" Wait=");
    logPrint(String(wait / 1000.0));
    logPrint("s");
    logPrint(" Score=");
    logPrint(String(l.priorityScore));
    logPrint(" Health=");
    logPrintln(healthy ? "OK" : "FAULT");
  }
  
  logPrintln("└────────────────────┘\n");
}

// ==================== WEB SERVER ====================
void setupWebServer() {
  // Main dashboard
  httpServer.on("/", HTTP_GET, handleRoot);
  
  // API endpoints
  httpServer.on("/api/status", HTTP_GET, handleAPIStatus);
  httpServer.on("/api/stats", HTTP_GET, handleAPIStats);
  httpServer.on("/api/csv", HTTP_GET, handleAPICSV);
  httpServer.on("/api/clear_csv", HTTP_POST, handleClearCSV);
  
  logPrintln("✅ Web server routes configured");
}

void handleRoot() {
  String html = generateDashboardHTML();
  httpServer.send(200, "text/html", html);
}

void handleAPIStatus() {
  String json = "{";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"state\":\"" + String(getStateName()) + "\",";
  json += "\"activeLane\":\"" + String(getLaneName(activeLane)) + "\",";
  json += "\"totalCycles\":" + String(totalCycles) + ",";
  json += "\"lanes\":[";
  
  for (int i = 0; i < 4; i++) {
    LaneData &l = lanes[i];
    if (i > 0) json += ",";
    json += "{";
    json += "\"name\":\"" + String(getLaneName((Lane)i)) + "\",";
    json += "\"enabled\":" + String(l.enabled ? "true" : "false") + ",";
    json += "\"count\":" + String(l.vehicleCount) + ",";
    json += "\"entered\":" + String(l.totalEntered) + ",";
    json += "\"exited\":" + String(l.totalExited) + ",";
    json += "\"priority\":" + String(l.priorityScore) + ",";
    json += "\"sensorHealth\":" + String((l.entrySensor.healthy && l.exitSensor.healthy) ? "true" : "false");
    json += "}";
  }
  
  json += "]}";
  httpServer.send(200, "application/json", json);
}

void handleAPIStats() {
  String json = "{";
  json += "\"systemUptime\":" + String((millis() - systemStartTime) / 1000) + ",";
  json += "\"totalCycles\":" + String(totalCycles) + ",";
  json += "\"totalProcessed\":" + String(totalVehiclesProcessed) + ",";
  
  int totalCurrent = 0;
  for (int i = 0; i < 4; i++) {
    if (lanes[i].enabled) totalCurrent += lanes[i].vehicleCount;
  }
  json += "\"currentVehicles\":" + String(totalCurrent);
  json += "}";
  
  httpServer.send(200, "application/json", json);
}

void handleAPICSV() {
  String csv = getCSVData();
  httpServer.send(200, "text/csv", csv);
}

void handleClearCSV() {
  clearCSVLog();
  httpServer.send(200, "text/plain", "CSV log cleared");
}

const char* getStateName() {
  switch(currentState) {
    case IDLE: return "IDLE";
    case DECISION: return "DECISION";
    case TRANSITION_YELLOW: return "YELLOW";
    case ACTIVE_GREEN: return "GREEN";
    case ALL_RED_CLEAR: return "ALL_RED";
    default: return "UNKNOWN";
  }
}

String generateDashboardHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Traffic Control Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #333;
      padding: 20px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .header {
      background: white;
      padding: 30px;
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      margin-bottom: 20px;
      text-align: center;
    }
    h1 {
      color: #667eea;
      margin-bottom: 10px;
      font-size: 2.5em;
    }
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .stat-card {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
    }
    .stat-label {
      color: #666;
      font-size: 0.9em;
      margin-bottom: 5px;
    }
    .stat-value {
      font-size: 2em;
      font-weight: bold;
      color: #667eea;
    }
    .lanes-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .lane-card {
      background: white;
      padding: 25px;
      border-radius: 10px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
      border-left: 5px solid #667eea;
    }
    .lane-card.active {
      border-left-color: #10b981;
      background: #ecfdf5;
    }
    .lane-name {
      font-size: 1.5em;
      font-weight: bold;
      margin-bottom: 15px;
      color: #333;
    }
    .lane-metric {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #eee;
    }
    .lane-metric:last-child {
      border-bottom: none;
    }
    .metric-label {
      color: #666;
    }
    .metric-value {
      font-weight: bold;
      color: #333;
    }
    .health-ok {
      color: #10b981;
    }
    .health-fault {
      color: #ef4444;
    }
    .controls {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
      margin-bottom: 20px;
      text-align: center;
    }
    button {
      background: #667eea;
      color: white;
      border: none;
      padding: 12px 30px;
      border-radius: 5px;
      font-size: 1em;
      cursor: pointer;
      margin: 5px;
      transition: all 0.3s;
    }
    button:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .status-badge {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 0.9em;
      font-weight: bold;
      margin-top: 10px;
    }
    .status-green { background: #10b981; color: white; }
    .status-yellow { background: #f59e0b; color: white; }
    .status-red { background: #ef4444; color: white; }
    .status-idle { background: #6b7280; color: white; }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    .pulse {
      animation: pulse 2s infinite;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🚦 Smart Traffic Control System</h1>
      <p>Real-time monitoring and analytics</p>
      <div id="systemState" class="status-badge status-idle">Loading...</div>
    </div>

    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-label">System Uptime</div>
        <div class="stat-value" id="uptime">--</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Total Cycles</div>
        <div class="stat-value" id="totalCycles">--</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Vehicles Processed</div>
        <div class="stat-value" id="totalProcessed">--</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Current Queue</div>
        <div class="stat-value" id="currentVehicles">--</div>
      </div>
    </div>

    <div class="lanes-grid" id="lanesGrid">
      <!-- Lanes will be populated by JavaScript -->
    </div>

    <div class="controls">
      <h3>Actions</h3>
      <button onclick="downloadCSV()">📊 Download CSV Log</button>
      <button onclick="clearCSV()">🗑️ Clear CSV Log</button>
      <button onclick="location.reload()">🔄 Refresh</button>
    </div>
  </div>

  <script>
    function updateDashboard() {
      fetch('/api/status')
        .then(response => response.json())
        .then(data => {
          // Update system state
          const stateElement = document.getElementById('systemState');
          stateElement.textContent = data.state;
          stateElement.className = 'status-badge';
          
          if (data.state === 'GREEN') {
            stateElement.classList.add('status-green', 'pulse');
          } else if (data.state === 'YELLOW') {
            stateElement.classList.add('status-yellow');
          } else if (data.state === 'ALL_RED' || data.state === 'DECISION') {
            stateElement.classList.add('status-red');
          } else {
            stateElement.classList.add('status-idle');
          }
          
          // Update uptime
          const hours = Math.floor(data.uptime / 3600);
          const minutes = Math.floor((data.uptime % 3600) / 60);
          const seconds = data.uptime % 60;
          document.getElementById('uptime').textContent = 
            `${hours}h ${minutes}m ${seconds}s`;
          
          document.getElementById('totalCycles').textContent = data.totalCycles;
          
          // Update lanes
          const lanesGrid = document.getElementById('lanesGrid');
          lanesGrid.innerHTML = '';
          
          data.lanes.forEach(lane => {
            if (!lane.enabled) return;
            
            const isActive = lane.name === data.activeLane && data.state === 'GREEN';
            const laneCard = document.createElement('div');
            laneCard.className = 'lane-card' + (isActive ? ' active' : '');
            
            laneCard.innerHTML = `
              <div class="lane-name">Lane ${lane.name} ${isActive ? '🟢' : ''}</div>
              <div class="lane-metric">
                <span class="metric-label">Current Count:</span>
                <span class="metric-value">${lane.count}</span>
              </div>
              <div class="lane-metric">
                <span class="metric-label">Total Entered:</span>
                <span class="metric-value">${lane.entered}</span>
              </div>
              <div class="lane-metric">
                <span class="metric-label">Total Exited:</span>
                <span class="metric-value">${lane.exited}</span>
              </div>
              <div class="lane-metric">
                <span class="metric-label">Priority Score:</span>
                <span class="metric-value">${lane.priority.toFixed(2)}</span>
              </div>
              <div class="lane-metric">
                <span class="metric-label">Sensor Health:</span>
                <span class="metric-value ${lane.sensorHealth ? 'health-ok' : 'health-fault'}">
                  ${lane.sensorHealth ? '✓ OK' : '✗ FAULT'}
                </span>
              </div>
            `;
            
            lanesGrid.appendChild(laneCard);
          });
        })
        .catch(error => console.error('Error fetching status:', error));
      
      // Update stats
      fetch('/api/stats')
        .then(response => response.json())
        .then(data => {
          document.getElementById('totalProcessed').textContent = data.totalProcessed;
          document.getElementById('currentVehicles').textContent = data.currentVehicles;
        })
        .catch(error => console.error('Error fetching stats:', error));
    }
    
    function downloadCSV() {
      window.location.href = '/api/csv';
    }
    
    function clearCSV() {
      if (confirm('Are you sure you want to clear the CSV log?')) {
        fetch('/api/clear_csv', { method: 'POST' })
          .then(() => alert('CSV log cleared'))
          .catch(error => alert('Error clearing CSV: ' + error));
      }
    }
    
    // Update every 2 seconds
    updateDashboard();
    setInterval(updateDashboard, 2000);
  </script>
</body>
</html>
)rawliteral";
  
  return html;
}

// ==================== MAIN LOOP ====================
void loop() {
  if (!systemInitialized) return;

  // Handle web server
  if (WiFi.status() == WL_CONNECTED) {
    httpServer.handleClient();
  }

  // Handle TCP logging client
  if (!logClientConnected || !logClient.connected()) {
    if (logServer.hasClient()) {
      if (logClient.connected()) logClient.stop();
      logClient = logServer.available();
      logClientConnected = true;
      logPrintln("\n=== 🌐 TCP Client Connected ===");
    }
  }
  if (logClientConnected && !logClient.connected()) {
    logClient.stop();
    logClientConnected = false;
  }

  // Update sensor statuses
  updateSensorBlockStatus();
  
  // Periodic sensor health check (every 5 seconds)
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 5000) {
    checkSensorHealth();
    lastHealthCheck = millis();
  }

  // State machine
  switch (currentState) {
    case IDLE:
      currentState = DECISION;
      stateStartTime = millis();
      break;
    
    case DECISION:
      handleDecisionState();
      break;
    
    case TRANSITION_YELLOW:
      handleTransitionYellowState();
      break;
    
    case ALL_RED_CLEAR:
      handleAllRedClearState();
      break;
    
    case ACTIVE_GREEN:
      handleActiveGreenState();
      break;
  }
  
  delay(50);
}
