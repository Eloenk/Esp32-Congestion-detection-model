/*
 * Smart Traffic Control System v3.0 (Quadratic Model)
 * Hardware: ESP32 + TLC5947 + 8x A3114 Sensors (IN/OUT)
 * * THE IMPROVED MODEL:
 * 1. Quadratic Scoring: Punishes long queues exponentially (Count^2).
 * 2. Dynamic Green Duration: Green time scales with Traffic Density.
 * 3. Anti-Starvation: Wait time acts as a linear "aging" factor.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include "Adafruit_TLC5947.h"

// ==================== WIFI ====================
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
WiFiServer logServer(23);
WiFiClient logClient;

// ==================== HARDWARE (TLC5947) ====================
#define TLC_NUM 1
#define TLC_DATA 23
#define TLC_CLOCK 18
#define TLC_LATCH 5
Adafruit_TLC5947 tlc = Adafruit_TLC5947(TLC_NUM, TLC_CLOCK, TLC_DATA, TLC_LATCH);

// ==================== SENSOR PINS ====================
// Lane 1
const int P1_IN = 13; const int P1_OUT = 12;
// Lane 2
const int P2_IN = 14; const int P2_OUT = 27;
// Lane 3
const int P3_IN = 26; const int P3_OUT = 25;
// Lane 4
const int P4_IN = 33; const int P4_OUT = 32;

// ==================== THE MODEL PARAMETERS ====================
// New Weights for Quadratic Model
const float WEIGHT_CONGESTION = 1.5; // Multiplier for (Count^2)
const float WEIGHT_WAIT_TIME  = 0.8; // Multiplier for Seconds Waited

// Timing
const unsigned long MIN_GREEN = 5000;
const unsigned long ABSOLUTE_MAX_GREEN = 45000; // Hard cap
const unsigned long YELLOW_TIME = 3000;
const unsigned long ALL_RED_TIME = 1000;

// ==================== STRUCTS ====================
struct LaneControl {
  int pinIn, pinOut;
  int chRed, chYellow, chGreen;
  
  volatile int vehicleCount; // Real-time Counter
  
  unsigned long waitStartTime;
  float currentScore; // For logging
};

LaneControl lanes[4];

enum TrafficState { DECISION, ALL_RED, ACTIVE_GREEN, TRANSITION_YELLOW };
TrafficState currentState = DECISION;
int activeLane = 0;
unsigned long stateStartTime = 0;

// ==================== INTERRUPTS (DATA FEED) ====================
// The "Eyes" of the model. Pure counting.

void IRAM_ATTR handleIn(int id) {
  lanes[id].vehicleCount++;
}

void IRAM_ATTR handleOut(int id) {
  if (lanes[id].vehicleCount > 0) lanes[id].vehicleCount--;
}

void IRAM_ATTR isr_1_in() { handleIn(0); }
void IRAM_ATTR isr_1_out() { handleOut(0); }
void IRAM_ATTR isr_2_in() { handleIn(1); }
void IRAM_ATTR isr_2_out() { handleOut(1); }
void IRAM_ATTR isr_3_in() { handleIn(2); }
void IRAM_ATTR isr_3_out() { handleOut(2); }
void IRAM_ATTR isr_4_in() { handleIn(3); }
void IRAM_ATTR isr_4_out() { handleOut(3); }

// ==================== INITIALIZATION ====================
void setupLane(int id, int pIn, int pOut, void (*isrIn)(), void (*isrOut)(), int r, int y, int g) {
  lanes[id].pinIn = pIn; lanes[id].pinOut = pOut;
  lanes[id].chRed = r; lanes[id].chYellow = y; lanes[id].chGreen = g;
  lanes[id].vehicleCount = 0;
  lanes[id].waitStartTime = 0;
  
  pinMode(pIn, INPUT_PULLUP);
  pinMode(pOut, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pIn), isrIn, FALLING);
  attachInterrupt(digitalPinToInterrupt(pOut), isrOut, FALLING);
}

void setup() {
  Serial.begin(115200);
  tlc.begin(); tlc.write();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while(WiFi.status() != WL_CONNECTED) delay(500);
  logServer.begin();

  setupLane(0, P1_IN, P1_OUT, isr_1_in, isr_1_out, 2, 1, 0);
  setupLane(1, P2_IN, P2_OUT, isr_2_in, isr_2_out, 5, 4, 3);
  setupLane(2, P3_IN, P3_OUT, isr_3_in, isr_3_out, 8, 7, 6);
  setupLane(3, P4_IN, P4_OUT, isr_4_in, isr_4_out, 11, 10, 9);
  
  // Start All Red
  for(int i=0; i<4; i++) tlc.setPWM(lanes[i].chRed, 4095);
  tlc.write();
  Serial.println("System Active: Quadratic Model Loaded");
}

// ==================== THE MATHEMATICAL MODEL ====================

// 1. Calculate Score based on Physics of Traffic
float calculateQuadraticScore(int laneIdx) {
  LaneControl &l = lanes[laneIdx];
  
  // If empty, score is 0 (unless we want to implement "patrolling")
  if (l.vehicleCount <= 0) return 0.0;
  
  if (l.waitStartTime == 0) l.waitStartTime = millis();
  
  // A. CONGESTION COMPONENT (Quadratic)
  // Input: Vehicle Count. 
  // Effect: 5 cars = 25pts. 10 cars = 100pts. 
  float congestionScore = pow(l.vehicleCount, 2) * WEIGHT_CONGESTION;
  
  // B. WAIT TIME COMPONENT (Linear)
  // Input: Seconds waited.
  // Effect: Adds steady pressure so single cars aren't ignored forever.
  float waitSeconds = (millis() - l.waitStartTime) / 1000.0;
  float waitScore = waitSeconds * WEIGHT_WAIT_TIME;
  
  return congestionScore + waitScore;
}

// 2. Dynamic Green Time Calculation
unsigned long calculateDynamicGreenTime(int laneIdx) {
  LaneControl &l = lanes[laneIdx];
  
  // Base calculation: 2 seconds per car is usually safe for flow
  unsigned long requiredTime = l.vehicleCount * 2000;
  
  // Add a "Startup Penalty" (Time lost due to driver reaction)
  requiredTime += 3000; 
  
  // Clamp to safe limits
  return constrain(requiredTime, MIN_GREEN, ABSOLUTE_MAX_GREEN);
}

// ==================== LOGIC LOOP ====================

void updateLogs() {
  if (logServer.hasClient()) {
    if (logClient) logClient.stop();
    logClient = logServer.available();
  }
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 1000 && logClient && logClient.connected()) {
    String s = "Scores: ";
    for(int i=0; i<4; i++) s += "L" + String(i) + ":" + String((int)lanes[i].currentScore) + " ";
    logClient.println(s);
    lastLog = millis();
  }
}

void loop() {
  updateLogs();
  unsigned long now = millis();
  unsigned long elapsed = now - stateStartTime;

  switch (currentState) {
    
    // --- PHASE 1: SMART DECISION ---
    case DECISION:
    {
      float maxScore = -1;
      int bestLane = -1;

      // Evaluate the Model for all lanes
      for(int i=0; i<4; i++) {
        // Skip current lane from being re-selected immediately if others are waiting
        // (Round robin effect) - UNLESS it has massive traffic
        if (i == activeLane && lanes[i].vehicleCount < 5) continue; 

        lanes[i].currentScore = calculateQuadraticScore(i);
        
        if (lanes[i].currentScore > maxScore) {
          maxScore = lanes[i].currentScore;
          bestLane = i;
        }
      }

      // If everyone is empty (Score 0)
      if (maxScore <= 0) {
        // If current lane still has cars, keep it. Else Red.
        if (lanes[activeLane].vehicleCount > 0) {
           currentState = ACTIVE_GREEN;
        } 
        // Else Stay in Decision (Effectively Red/Idle)
      } 
      else {
        // We found a winner
        if (bestLane != activeLane) {
          activeLane = bestLane;
          // Switch to Red
          for(int i=0; i<4; i++) {
            tlc.setPWM(lanes[i].chRed, 4095);
            tlc.setPWM(lanes[i].chGreen, 0); 
            tlc.setPWM(lanes[i].chYellow, 0);
          }
          tlc.write();
          currentState = ALL_RED;
          stateStartTime = now;
        } else {
          // Winner is the current lane (extended green)
          currentState = ACTIVE_GREEN;
        }
      }
      break;
    }

    // --- PHASE 2: CLEARANCE ---
    case ALL_RED:
      if (elapsed > ALL_RED_TIME) {
        currentState = ACTIVE_GREEN;
        stateStartTime = now;
        tlc.setPWM(lanes[activeLane].chRed, 0);
        tlc.setPWM(lanes[activeLane].chGreen, 4095);
        tlc.write();
      }
      break;

    // --- PHASE 3: EXECUTION ---
    case ACTIVE_GREEN:
    {
      // Ask the model: How much time does this density need?
      unsigned long dynamicDuration = calculateDynamicGreenTime(activeLane);
      
      // 1. Min Green Check
      if (elapsed < MIN_GREEN) return;

      // 2. Logic: Switch if Time Up OR Count is Zero
      if (elapsed > dynamicDuration || lanes[activeLane].vehicleCount == 0) {
        currentState = TRANSITION_YELLOW;
        stateStartTime = now;
        
        tlc.setPWM(lanes[activeLane].chGreen, 0);
        tlc.setPWM(lanes[activeLane].chYellow, 4095);
        tlc.write();
        
        // Reset Fairness Timer for this lane
        lanes[activeLane].waitStartTime = 0; 
        
        // Safety Clean
        if (lanes[activeLane].vehicleCount < 0) lanes[activeLane].vehicleCount = 0;
      }
      break;
    }

    // --- PHASE 4: TRANSITION ---
    case TRANSITION_YELLOW:
      if (elapsed > YELLOW_TIME) {
        currentState = DECISION;
        tlc.setPWM(lanes[activeLane].chYellow, 0);
        tlc.setPWM(lanes[activeLane].chRed, 4095);
        tlc.write();
      }
      break;
  }
  
  delay(10);
}