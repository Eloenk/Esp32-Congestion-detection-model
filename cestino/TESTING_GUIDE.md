# Testing Guide & Example Data

## Pre-Flight Checklist

Before running the full system, verify each component works:

### 1. TLC5947 LED Driver Test

Upload this minimal test sketch:

```cpp
#include <Adafruit_TLC5947.h>

#define NUM_TLC 1
#define DATA_PIN 13
#define CLOCK_PIN 14
#define LATCH_PIN 15

Adafruit_TLC5947 tlc = Adafruit_TLC5947(NUM_TLC, CLOCK_PIN, DATA_PIN, LATCH_PIN);

void setup() {
  Serial.begin(115200);
  tlc.begin();
  Serial.println("TLC5947 Test - Cycling through all 12 LEDs");
}

void loop() {
  for (int i = 0; i < 12; i++) {
    tlc.setPWM(i, 4095);  // Full brightness
    tlc.write();
    Serial.print("LED Channel ");
    Serial.print(i);
    Serial.println(" ON");
    delay(500);
    
    tlc.setPWM(i, 0);  // Off
    tlc.write();
    delay(200);
  }
}
```

**Expected Result**: Each of your 12 traffic LEDs should light up in sequence.

**Troubleshooting:**
- If no LEDs light: Check 5V power to TLC5947, verify wiring (DIN, CLK, LAT)
- If LEDs stay dim: Check resistor values (should be ~220Ω)
- If random LEDs: Double-check channel-to-LED mapping

---

### 2. A3144 Hall Sensor Test

Upload this sensor test sketch:

```cpp
const int SENSOR_PINS[] = {32, 33, 25, 26, 27, 14, 12, 13};
const char* SENSOR_NAMES[] = {
  "Lane1_Entry", "Lane1_Exit", 
  "Lane2_Entry", "Lane2_Exit",
  "Lane3_Entry", "Lane3_Exit",
  "Lane4_Entry", "Lane4_Exit"
};

void setup() {
  Serial.begin(115200);
  Serial.println("A3144 Sensor Test - Pass magnet near each sensor");
  
  for (int i = 0; i < 8; i++) {
    pinMode(SENSOR_PINS[i], INPUT_PULLUP);
  }
}

void loop() {
  for (int i = 0; i < 8; i++) {
    int state = digitalRead(SENSOR_PINS[i]);
    if (state == LOW) {  // A3144 outputs LOW when magnet detected
      Serial.print("✓ ");
      Serial.print(SENSOR_NAMES[i]);
      Serial.println(" TRIGGERED!");
      delay(500);  // Debounce
    }
  }
  delay(100);
}
```

**Expected Result**: When you bring a magnet near a sensor, you should see:
```
✓ Lane1_Entry TRIGGERED!
✓ Lane2_Exit TRIGGERED!
```

**Troubleshooting:**
- If no triggers: Check magnet polarity (flip it), verify VCC/GND connections
- If always triggered: Magnetic interference nearby, move away from metals
- If intermittent: Check wire connections, try different GPIO pin

---

### 3. WiFi Connection Test

Before running the full code, test WiFi:

```cpp
#include <WiFi.h>

const char* ssid = "YourNetworkName";
const char* password = "YourPassword";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {}
```

**Expected Result:**
```
Connecting to WiFi.....
Connected!
IP Address: 192.168.1.100
```

**Troubleshooting:**
- If times out: Check SSID/password, ensure 2.4GHz network
- If connects but no IP: Router DHCP issue, try rebooting router
- If unstable: Move ESP32 closer to router, check antenna

---

## Full System Testing Protocol

### Phase 1: Static Validation (No Vehicles)

1. **Upload main code** and open Serial Monitor (115200 baud)
2. **Verify initialization**:
   ```
   ✅ TLC5947 initialized
   ✅ SPIFFS initialized  
   ✅ WiFi connected!
      IP Address: 192.168.1.xxx
   ✅ All sensor interrupts attached
   ✅ System initialized and ready!
   ```

3. **Check web dashboard**:
   - Navigate to `http://[IP_ADDRESS]`
   - Verify all 4 lanes show 0 vehicles
   - Confirm "System Uptime" is counting
   - Check "Sensor Health" shows OK (green) for all

4. **Verify LED sequence**:
   - System should cycle through lanes even with 0 vehicles
   - Observe: DECISION → ALL_RED → GREEN → YELLOW → (repeat)
   - Each lane should get green light in round-robin

**Expected Behavior with 0 Vehicles:**
- Green times will be minimum (5 seconds)
- Lanes cycle in order: ONE → TWO → THREE → FOUR → ONE...
- All lanes show 0 count in dashboard

---

### Phase 2: Single Lane Testing

**Objective**: Test one lane's entry/exit sensors independently

**Test Lane ONE:**

1. **Add vehicles (entry sensor):**
   - Pass magnet through entry sensor 3 times
   - Serial output should show:
     ```
     Lane ONE: Entry triggered (Count: 1)
     Lane ONE: Entry triggered (Count: 2)
     Lane ONE: Entry triggered (Count: 3)
     ```

2. **Check dashboard:**
   - Lane ONE card should show "Current Count: 3"
   - "Total Entered: 3"
   - "Total Exited: 0"

3. **Wait for green light:**
   - Lane ONE should get priority (highest count)
   - Green light duration: ~8s base + (3 vehicles × 0.4s) = ~9.2s
   - Serial shows:
     ```
     🟢 GREEN: Lane ONE | Duration: 9.2s | Vehicles: 3
     ```

4. **Remove vehicles (exit sensor):**
   - During green light, pass magnet through exit sensor 3 times
   - Serial output should show:
     ```
     Lane ONE: Exit triggered (Count: 2)
     Lane ONE: Exit triggered (Count: 1)
     Lane ONE: Exit triggered (Count: 0)
     ```

5. **Verify final state:**
   - Dashboard shows "Current Count: 0"
   - "Total Entered: 3"
   - "Total Exited: 3"
   - Perfect balance!

**Repeat for lanes TWO, THREE, FOUR** to ensure all sensors work.

---

### Phase 3: Multi-Lane Testing

**Objective**: Test priority system and lane competition

**Setup:**
- Lane ONE: Add 5 vehicles (entry sensor × 5)
- Lane TWO: Add 2 vehicles (entry sensor × 2)
- Lane THREE: Add 8 vehicles (entry sensor × 8)
- Lane FOUR: Add 0 vehicles

**Expected Priority:**
1. **Lane THREE should go first** (highest count: 8)
   - Priority score ≈ 0.53 (8/30 × 0.6 = 0.16, plus wait time)
   
2. **Lane ONE should go second** (count: 5)
   - Priority score ≈ 0.40
   
3. **Lane TWO should go third** (count: 2)
   - Priority score ≈ 0.24
   
4. **Lane FOUR should be skipped** (empty lane skip enabled)

**Observe in Serial Monitor:**
```
╔═══ DECISION PHASE ═══╗
║ Selected: Lane THREE
║ Vehicles: 8
║ Priority: 0.53
╚═════════════════════╝

🟢 GREEN: Lane THREE | Duration: 11.2s | Vehicles: 8
```

**Exit some vehicles:**
- During Lane THREE green: Pass magnet through exit sensor 5 times
- Lane THREE count should drop: 8 → 7 → 6 → 5 → 4 → 3
- Remaining vehicles carry over to next cycle

---

### Phase 4: Starvation Prevention Test

**Objective**: Verify max wait time override

**Setup:**
1. Lane ONE: Add 10 vehicles
2. Lane TWO: Add 1 vehicle
3. Wait 15+ seconds (don't let Lane TWO get green)

**Expected Behavior:**
- After 15 seconds, Lane TWO should get **starvation override**
- Serial shows:
  ```
  ⚠️ STARVATION OVERRIDE: Lane TWO waited 15.2s
  🟢 GREEN: Lane TWO | Duration: 8.4s | Vehicles: 1
  ```

**Why This Matters:**
- Prevents indefinite waiting
- Ensures fairness even with low congestion
- Mimics real-world traffic light behavior

---

### Phase 5: Sensor Health & Anomaly Testing

**Test 1: Negative Count Anomaly**

1. **Without adding vehicles first**, pass magnet through EXIT sensor
2. Count goes negative: 0 → -1 → -2 → -3 → -4
3. At -4, system should trigger anomaly:
   ```
   ⚠️ Lane ONE ANOMALY: Negative count (-4) - possible sensor miscalibration!
   ```
4. Count auto-resets to 0
5. Dashboard shows "Sensor Health: FAULT" (red)

**Test 2: Sensor Timeout**

1. Place magnet directly on sensor (don't remove)
2. Wait 30+ seconds
3. System detects timeout:
   ```
   ⚠️ Lane ONE ENTRY sensor timeout - vehicle stuck or sensor fault!
   ```
4. Sensor marked unhealthy
5. Dashboard shows "Sensor Health: FAULT"

**Test 3: Health Recovery**

1. After fault, wait 60 seconds
2. Sensor health auto-recovers (periodic check)
3. Dashboard returns to "Sensor Health: OK"

---

### Phase 6: CSV Data Logging

**Generate Traffic Data:**

1. Run system for 5-10 minutes with various vehicle counts
2. Let multiple lanes cycle through green lights
3. Access dashboard and click **"📊 Download CSV Log"**

**Example CSV Output:**

```csv
Timestamp,Lane,VehicleCount,TotalEntered,TotalExited,WaitTime,GreenDuration,PriorityScore,SensorHealth
120,ONE,3,15,12,5200,8000,0.42,OK
145,TWO,5,23,18,8100,10000,0.58,OK
170,THREE,0,8,8,0,5000,0.05,OK
195,FOUR,7,31,24,12000,10800,0.63,OK
220,ONE,2,18,16,3200,7200,0.35,FAULT
245,TWO,4,28,24,5600,8800,0.51,OK
```

**Data Analysis in Excel/Python:**

```python
import pandas as pd

df = pd.read_csv('traffic_log.csv')

# Calculate metrics
print("Average wait time per lane:")
print(df.groupby('Lane')['WaitTime'].mean() / 1000, "seconds")

print("\nTotal vehicles processed:")
print(df.groupby('Lane')['TotalExited'].max())

print("\nSensor health rate:")
print(df['SensorHealth'].value_counts(normalize=True) * 100, "%")
```

---

## Performance Benchmarks

### Expected Metrics (After 1 Hour of Operation)

| Metric | Target Value | Acceptable Range |
|--------|--------------|------------------|
| **Total Cycles** | ~200-300 cycles | 150-400 |
| **Vehicles Processed** | ~500-1000 | 300-1500 |
| **Average Green Time** | 8-12 seconds | 5-20 |
| **Sensor Health Rate** | >95% OK | >90% |
| **Negative Count Events** | 0-2 per lane | 0-5 |
| **System Uptime** | 100% | >99% |

### Red Flags (Investigate if you see):

- ❌ Sensor health <80% OK → Check wiring, magnet strength
- ❌ >10 negative count events → Recalibrate sensors, increase debounce
- ❌ Average wait time >20s → Adjust priority weights, check vehicle distribution
- ❌ System crashes/reboots → Check power supply, reduce LED brightness
- ❌ CSV file not saving → Re-format SPIFFS, check flash partition

---

## Advanced Testing Scenarios

### Scenario 1: Rush Hour Simulation

**Setup:**
- Add 20+ vehicles to Lane ONE over 2 minutes
- Add 5 vehicles to other lanes
- Observe priority balancing

**Expected:**
- Lane ONE gets extended green times (up to 25s max)
- Other lanes still get service (fairness algorithm)
- No starvation events

---

### Scenario 2: Emergency Override (Manual Implementation)

Add this code to simulate emergency vehicle:

```cpp
// In loop(), add:
if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
  activeLane = ONE;  // Force Lane ONE
  currentState = ALL_RED_CLEAR;
  stateStartTime = millis();
  logPrintln("🚨 EMERGENCY OVERRIDE: Lane ONE activated!");
}
```

---

### Scenario 3: Sensor Failure Recovery

**Test resilience:**
1. Disconnect one sensor's VCC wire (simulate failure)
2. System should mark sensor as FAULT
3. Lane still operates with reduced priority
4. Reconnect wire → health recovers after 60s

---

## Example Serial Output (Normal Operation)

```
╔════════════════════════════════════════════════════════╗
║  Smart Traffic Control System v2.0                    ║
║  ESP32 + A3144 Sensors + TLC5947 LED Driver           ║
╚════════════════════════════════════════════════════════╝

🔧 Initializing TLC5947 LED driver...
✅ TLC5947 initialized
🔧 Initializing SPIFFS...
✅ CSV log file exists
📡 Connecting to WiFi: Theotherguy
..
✅ WiFi connected!
   IP Address: 192.168.1.105
🌐 TCP Log Server: telnet 192.168.1.105 23
🚦 Initializing lanes...
  Lane ONE: ENABLED ✓ | Sensors: Entry=GPIO32, Exit=GPIO33 | LEDs: Ch0,1,2
  Lane TWO: ENABLED ✓ | Sensors: Entry=GPIO25, Exit=GPIO26 | LEDs: Ch3,4,5
  Lane THREE: ENABLED ✓ | Sensors: Entry=GPIO27, Exit=GPIO14 | LEDs: Ch6,7,8
  Lane FOUR: ENABLED ✓ | Sensors: Entry=GPIO12, Exit=GPIO13 | LEDs: Ch9,10,11
⚡ Attaching sensor interrupts...
✅ All sensor interrupts attached
🌐 Web Dashboard: http://192.168.1.105
✅ System initialized and ready!

╔═══ DECISION PHASE ═══╗
║ Selected: Lane ONE
║ Vehicles: 0
║ Priority: 0.00
╚═════════════════════╝

🟢 GREEN: Lane ONE | Duration: 5.0s | Vehicles: 0
🟡 YELLOW: Lane ONE | Remaining: 0

[Vehicle enters Lane TWO]
Lane TWO: Entry triggered (Count: 1)

╔═══ DECISION PHASE ═══╗
║ Selected: Lane TWO
║ Vehicles: 1
║ Priority: 0.15
╚═════════════════════╝

🟢 GREEN: Lane TWO | Duration: 8.4s | Vehicles: 1

[Vehicle exits Lane TWO]
Lane TWO: Exit triggered (Count: 0)

🟡 YELLOW: Lane TWO | Remaining: 0
```

---

## Telnet Logging Example

Connect via terminal:
```bash
telnet 192.168.1.105 23
```

You'll see the same Serial output streamed live. Perfect for remote monitoring!

---

## Debugging Tips

### Issue: Erratic Counts
- **Solution**: Increase `SENSOR_DEBOUNCE_TIME` from 200ms to 500ms
- **Check**: Magnet not too strong (causing multiple triggers)

### Issue: Vehicles Not Detected
- **Solution**: Reduce gap between sensor and magnet (<1cm)
- **Check**: Sensor orientation (flat side toward magnet)

### Issue: Web Dashboard Not Updating
- **Solution**: Hard refresh browser (Ctrl+F5)
- **Check**: ESP32 still connected to WiFi (Serial monitor)

### Issue: CSV Download Empty
- **Solution**: Let system run a few cycles first (minimum 1-2 green lights)
- **Check**: SPIFFS initialized (Serial shows "✅ CSV log file exists")

---

**🎉 If all tests pass, your system is fully operational! 🎉**

Ready for demonstrations, traffic research, or educational purposes.
