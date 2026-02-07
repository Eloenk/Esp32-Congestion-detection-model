/*
 * A3144 Hall Effect Sensors - Test Code
 * 
 * Tests all 8 sensors (2 per lane: entry + exit)
 * Uses hardware interrupts for fast detection
 * Displays real-time trigger events
 * 
 * Hardware:
 * - ESP32 DevKit
 * - 8× A3144 Hall Effect Sensors
 * - 8× Neodymium magnets (for testing)
 * 
 * Wiring (all sensors):
 * A3144 Pin 1 (VCC) → ESP32 5V
 * A3144 Pin 2 (GND) → ESP32 GND
 * A3144 Pin 3 (OUT) → ESP32 GPIO (see below)
 * 
 * GPIO Mapping:
 * Lane ONE:   Entry=GPIO32, Exit=GPIO33
 * Lane TWO:   Entry=GPIO25, Exit=GPIO26
 * Lane THREE: Entry=GPIO27, Exit=GPIO14
 * Lane FOUR:  Entry=GPIO12, Exit=GPIO13
 */

// ==================== SENSOR PIN DEFINITIONS ====================
// Lane ONE
const int ONE_ENTRY_SENSOR = 32;
const int ONE_EXIT_SENSOR = 33;

// Lane TWO
const int TWO_ENTRY_SENSOR = 25;
const int TWO_EXIT_SENSOR = 26;

// Lane THREE
const int THREE_ENTRY_SENSOR = 27;
const int THREE_EXIT_SENSOR = 14;

// Lane FOUR
const int FOUR_ENTRY_SENSOR = 12;
const int FOUR_EXIT_SENSOR = 13;

// ==================== DEBOUNCE SETTINGS ====================
const unsigned long DEBOUNCE_TIME = 200;  // 200ms between triggers

// ==================== SENSOR DATA STRUCTURE ====================
struct SensorInfo {
  int pin;
  const char* name;
  volatile unsigned long lastTriggerTime;
  volatile unsigned long triggerCount;
  volatile bool isActive;  // Currently detecting magnet
};

// Array of all 8 sensors
SensorInfo sensors[8];

// ==================== INTERRUPT HANDLERS ====================
void IRAM_ATTR sensor0ISR() {
  unsigned long now = millis();
  if (now - sensors[0].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[0].triggerCount++;
    sensors[0].lastTriggerTime = now;
    sensors[0].isActive = true;
  }
}

void IRAM_ATTR sensor1ISR() {
  unsigned long now = millis();
  if (now - sensors[1].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[1].triggerCount++;
    sensors[1].lastTriggerTime = now;
    sensors[1].isActive = true;
  }
}

void IRAM_ATTR sensor2ISR() {
  unsigned long now = millis();
  if (now - sensors[2].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[2].triggerCount++;
    sensors[2].lastTriggerTime = now;
    sensors[2].isActive = true;
  }
}

void IRAM_ATTR sensor3ISR() {
  unsigned long now = millis();
  if (now - sensors[3].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[3].triggerCount++;
    sensors[3].lastTriggerTime = now;
    sensors[3].isActive = true;
  }
}

void IRAM_ATTR sensor4ISR() {
  unsigned long now = millis();
  if (now - sensors[4].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[4].triggerCount++;
    sensors[4].lastTriggerTime = now;
    sensors[4].isActive = true;
  }
}

void IRAM_ATTR sensor5ISR() {
  unsigned long now = millis();
  if (now - sensors[5].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[5].triggerCount++;
    sensors[5].lastTriggerTime = now;
    sensors[5].isActive = true;
  }
}

void IRAM_ATTR sensor6ISR() {
  unsigned long now = millis();
  if (now - sensors[6].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[6].triggerCount++;
    sensors[6].lastTriggerTime = now;
    sensors[6].isActive = true;
  }
}

void IRAM_ATTR sensor7ISR() {
  unsigned long now = millis();
  if (now - sensors[7].lastTriggerTime >= DEBOUNCE_TIME) {
    sensors[7].triggerCount++;
    sensors[7].lastTriggerTime = now;
    sensors[7].isActive = true;
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  A3144 Hall Sensor Test - 8 Sensors       ║");
  Serial.println("║  Pass magnets near sensors to test        ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  // Initialize sensor data
  sensors[0] = {ONE_ENTRY_SENSOR, "Lane1_ENTRY ", 0, 0, false};
  sensors[1] = {ONE_EXIT_SENSOR,  "Lane1_EXIT  ", 0, 0, false};
  sensors[2] = {TWO_ENTRY_SENSOR, "Lane2_ENTRY ", 0, 0, false};
  sensors[3] = {TWO_EXIT_SENSOR,  "Lane2_EXIT  ", 0, 0, false};
  sensors[4] = {THREE_ENTRY_SENSOR, "Lane3_ENTRY ", 0, 0, false};
  sensors[5] = {THREE_EXIT_SENSOR,  "Lane3_EXIT  ", 0, 0, false};
  sensors[6] = {FOUR_ENTRY_SENSOR, "Lane4_ENTRY ", 0, 0, false};
  sensors[7] = {FOUR_EXIT_SENSOR,  "Lane4_EXIT  ", 0, 0, false};
  
  Serial.println("🔧 Initializing sensors...\n");
  
  // Setup pins and attach interrupts
  for (int i = 0; i < 8; i++) {
    pinMode(sensors[i].pin, INPUT_PULLUP);
    Serial.print("   ");
    Serial.print(sensors[i].name);
    Serial.print(" → GPIO");
    Serial.print(sensors[i].pin);
    Serial.println(" ✓");
  }
  
  // Attach interrupts (A3144 triggers on FALLING edge when magnet detected)
  attachInterrupt(digitalPinToInterrupt(ONE_ENTRY_SENSOR), sensor0ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(ONE_EXIT_SENSOR), sensor1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(TWO_ENTRY_SENSOR), sensor2ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(TWO_EXIT_SENSOR), sensor3ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(THREE_ENTRY_SENSOR), sensor4ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(THREE_EXIT_SENSOR), sensor5ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FOUR_ENTRY_SENSOR), sensor6ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FOUR_EXIT_SENSOR), sensor7ISR, FALLING);
  
  Serial.println("\n✅ All sensors initialized!");
  Serial.println("⚡ Hardware interrupts attached (FALLING edge)");
  Serial.println("\n📝 Sensor Info:");
  Serial.println("   - A3144 outputs LOW when magnet detected (South pole)");
  Serial.println("   - Optimal detection distance: <1cm");
  Serial.println("   - Debounce time: 200ms\n");
  
  Serial.println("┌────────────────────────────────────────────────────────┐");
  Serial.println("│  Pass a magnet near each sensor to test               │");
  Serial.println("│  Trigger count will increment for each detection      │");
  Serial.println("└────────────────────────────────────────────────────────┘\n");
  
  delay(1000);
  
  // Initial status display
  displayStatus();
}

// ==================== MAIN LOOP ====================
void loop() {
  static unsigned long lastDisplayTime = 0;
  static unsigned long lastTriggerCount[8] = {0};
  
  bool anyTriggered = false;
  
  // Check for new triggers
  for (int i = 0; i < 8; i++) {
    if (sensors[i].triggerCount > lastTriggerCount[i]) {
      // New trigger detected!
      Serial.print("🔴 TRIGGER: ");
      Serial.print(sensors[i].name);
      Serial.print(" (GPIO");
      Serial.print(sensors[i].pin);
      Serial.print(") | Count: ");
      Serial.print(sensors[i].triggerCount);
      Serial.print(" | Time: ");
      Serial.print(sensors[i].lastTriggerTime);
      Serial.println("ms");
      
      lastTriggerCount[i] = sensors[i].triggerCount;
      anyTriggered = true;
    }
    
    // Update active status (check if magnet still present)
    if (digitalRead(sensors[i].pin) == HIGH && sensors[i].isActive) {
      sensors[i].isActive = false;
      Serial.print("   ↳ ");
      Serial.print(sensors[i].name);
      Serial.println(" cleared (magnet removed)");
    }
  }
  
  // Display status every 10 seconds
  if (millis() - lastDisplayTime >= 10000) {
    if (!anyTriggered) {
      Serial.println("ℹ️  No activity in last 10 seconds...");
    }
    displayStatus();
    lastDisplayTime = millis();
  }
  
  delay(50);  // Small delay for stability
}

// ==================== STATUS DISPLAY ====================
void displayStatus() {
  Serial.println("\n╔═══════════════════ SENSOR STATUS ═══════════════════╗");
  Serial.println("║ Sensor          | GPIO | Active | Trigger Count     ║");
  Serial.println("╠═════════════════╪══════╪════════╪═══════════════════╣");
  
  for (int i = 0; i < 8; i++) {
    Serial.print("║ ");
    Serial.print(sensors[i].name);
    Serial.print(" | ");
    
    if (sensors[i].pin < 10) Serial.print(" ");
    Serial.print(sensors[i].pin);
    Serial.print("   | ");
    
    if (sensors[i].isActive) {
      Serial.print("🟢 YES ");
    } else {
      Serial.print("⚪ NO  ");
    }
    Serial.print(" | ");
    
    if (sensors[i].triggerCount < 10) Serial.print("  ");
    else if (sensors[i].triggerCount < 100) Serial.print(" ");
    Serial.print(sensors[i].triggerCount);
    Serial.print(" triggers       ║");
    Serial.println();
  }
  
  Serial.println("╚═════════════════════════════════════════════════════╝\n");
  
  // Calculate statistics
  unsigned long totalTriggers = 0;
  int activeSensors = 0;
  
  for (int i = 0; i < 8; i++) {
    totalTriggers += sensors[i].triggerCount;
    if (sensors[i].isActive) activeSensors++;
  }
  
  Serial.print("📊 Total Triggers: ");
  Serial.print(totalTriggers);
  Serial.print(" | Currently Active: ");
  Serial.print(activeSensors);
  Serial.println(" sensors\n");
}

// ==================== DIAGNOSTIC FUNCTIONS ====================
// Test individual sensor (call from Serial commands if needed)
void testSensor(int sensorIndex) {
  if (sensorIndex < 0 || sensorIndex >= 8) {
    Serial.println("❌ Invalid sensor index (0-7)");
    return;
  }
  
  Serial.print("🔍 Testing ");
  Serial.print(sensors[sensorIndex].name);
  Serial.print(" (GPIO");
  Serial.print(sensors[sensorIndex].pin);
  Serial.println(")");
  
  Serial.println("   Place magnet near sensor...");
  
  unsigned long startCount = sensors[sensorIndex].triggerCount;
  unsigned long startTime = millis();
  
  while (millis() - startTime < 5000) {
    if (sensors[sensorIndex].triggerCount > startCount) {
      Serial.println("   ✅ Sensor working! Trigger detected.");
      return;
    }
    delay(100);
  }
  
  Serial.println("   ⚠️  No trigger in 5 seconds. Check:");
  Serial.println("      - Wiring (VCC, GND, OUT)");
  Serial.println("      - Magnet polarity (South pole to sensor)");
  Serial.println("      - Distance (<1cm from sensor)");
}
