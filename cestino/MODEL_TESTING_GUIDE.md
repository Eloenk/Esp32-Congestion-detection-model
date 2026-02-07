# Small-Scale Model Testing Guide (5-8 Toy Cars)

## Configuration for Physical Models

### The Problem with Default Settings

**Original setting:** `MAX_VEHICLES_PER_LANE = 30.0`

With only 5-8 toy cars available, the priority system barely reacts:
```
2 cars: 2/30 = 0.067 normalized → 0.004 quadratic score (0.4%!)
5 cars: 5/30 = 0.167 normalized → 0.028 quadratic score (2.8%)
8 cars: 8/30 = 0.267 normalized → 0.071 quadratic score (7.1%)
```

**Problem:** System treats 5 cars as "nearly empty" - priorities barely differentiate!

---

## Optimized Setting for Model Testing

### New Configuration
```cpp
const float MAX_VEHICLES_PER_LANE = 10.0;  // Perfect for 1-8 vehicle testing
```

### Impact on Priority Calculation

| Cars | Normalized | Linear Score | Quadratic Score | Priority Feel |
|------|-----------|--------------|-----------------|---------------|
| 1    | 0.10      | 0.10         | **0.01**        | Very low 🟢   |
| 2    | 0.20      | 0.20         | **0.04**        | Low 🟢        |
| 3    | 0.30      | 0.30         | **0.09**        | Getting busy 🟡 |
| 4    | 0.40      | 0.40         | **0.16**        | Medium 🟡     |
| 5    | 0.50      | 0.50         | **0.25**        | Half full! 🟠  |
| 6    | 0.60      | 0.60         | **0.36**        | Congested 🟠  |
| 7    | 0.70      | 0.70         | **0.49**        | Very busy 🔴  |
| 8    | 0.80      | 0.80         | **0.64**        | Critical! 🔴  |

**Notice:** With `MAX=10`, each car adds significant pressure!

---

## Full Priority Scores (Quadratic Model)

### With 0 Wait Time (Pure Congestion)

| Cars | Congestion × 0.6 | Wait × 0.3 | Health × 0.1 | **Total** |
|------|------------------|------------|--------------|-----------|
| 1    | 0.006            | 0.00       | 0.10         | **0.106** |
| 2    | 0.024            | 0.00       | 0.10         | **0.124** |
| 3    | 0.054            | 0.00       | 0.10         | **0.154** |
| 4    | 0.096            | 0.00       | 0.10         | **0.196** |
| 5    | 0.150            | 0.00       | 0.10         | **0.250** |
| 6    | 0.216            | 0.00       | 0.10         | **0.316** |
| 7    | 0.294            | 0.00       | 0.10         | **0.394** |
| 8    | 0.384            | 0.00       | 0.10         | **0.484** |

### With 5-Second Wait Time

| Cars | Congestion × 0.6 | Wait × 0.3 | Health × 0.1 | **Total** |
|------|------------------|------------|--------------|-----------|
| 1    | 0.006            | 0.10       | 0.10         | **0.206** |
| 3    | 0.054            | 0.10       | 0.10         | **0.254** |
| 5    | 0.150            | 0.10       | 0.10         | **0.350** |
| 7    | 0.294            | 0.10       | 0.10         | **0.494** |

**Wait time calculation:** (5000ms / 15000ms max) × 0.3 = 0.10

---

## Realistic Test Scenarios

### Scenario 1: Balanced Traffic

**Setup:**
- Lane ONE: 3 cars
- Lane TWO: 3 cars  
- Lane THREE: 3 cars
- Lane FOUR: 3 cars

**Expected:** Perfect round-robin (all equal priority ≈ 0.154)

**System should:** Cycle ONE → TWO → THREE → FOUR → repeat

---

### Scenario 2: One Congested Lane

**Setup:**
- Lane ONE: 7 cars (critical!)
- Lane TWO: 2 cars
- Lane THREE: 1 car
- Lane FOUR: 2 cars

**Priority Scores:**
- Lane ONE: 0.394 (49% congestion score)
- Lane TWO: 0.124 (2.4% congestion)
- Lane THREE: 0.106 (0.6% congestion)
- Lane FOUR: 0.124 (2.4% congestion)

**Expected:** Lane ONE gets priority **3× faster** than others!

**System should:** 
1. Lane ONE (first service, highest priority)
2. After ONE exits some cars, TWO/FOUR compete
3. THREE waits (lowest)

---

### Scenario 3: Gradual Buildup

**Setup (evolving over time):**

**Minute 1:**
- Lane ONE: Add 2 cars → Priority = 0.124
- Lane TWO: Add 1 car → Priority = 0.106

**Minute 2:**
- Lane ONE: Add 2 more (total 4) → Priority = 0.196
- Lane TWO: Add 2 more (total 3) → Priority = 0.154
- Lane THREE: Add 3 cars → Priority = 0.154

**Minute 3:**
- Lane ONE: Add 2 more (total 6) → Priority = 0.316
- Lane FOUR: Add 5 cars → Priority = 0.250

**Observed Behavior:**
- Initially: ONE and TWO alternate
- Mid-test: ONE pulls ahead (4 cars)
- Late-test: ONE dominates (6 cars) with 0.316 score

**This demonstrates:** Quadratic pressure increasing with vehicle count!

---

### Scenario 4: Starvation Prevention

**Setup:**
- Lane ONE: 8 cars (max capacity)
- Lane TWO: 1 car
- Lane THREE: 0 cars (empty)
- Lane FOUR: 0 cars (empty)

**Without starvation override:**
- Lane ONE: 0.484 priority
- Lane TWO: 0.106 priority

**Expected:** ONE gets serviced first, but after 15 seconds, Lane TWO **must** get override!

**Test procedure:**
1. Add 8 cars to Lane ONE
2. Add 1 car to Lane TWO
3. Watch Lane ONE get green first (correct)
4. Monitor Serial for starvation warning after 15s
5. Verify Lane TWO gets emergency priority

**Serial output should show:**
```
⚠️ STARVATION OVERRIDE: Lane TWO waited 15.2s
🟢 GREEN: Lane TWO | Duration: 8.4s | Vehicles: 1
```

---

### Scenario 5: Wait Time Balancing

**Setup (all added simultaneously):**
- Lane ONE: 4 cars
- Lane TWO: 5 cars (1 more than ONE)
- Lane THREE: 0 cars
- Lane FOUR: 0 cars

**Cycle 1:**
- Lane TWO wins (0.250 vs 0.196)
- TWO gets green first

**After TWO service (cars exit):**
- Lane ONE: Still 4 cars, **now waited 12 seconds**
  - Congestion: 0.16 × 0.6 = 0.096
  - Wait: (12000/15000) × 0.3 = 0.24
  - **Total = 0.336 + 0.10 = 0.436**
  
- Lane TWO: Now 2 cars (exited 3), no wait
  - **Total = 0.124**

**Cycle 2:**
- Lane ONE wins decisively! (0.436 vs 0.124)

**Demonstrates:** Wait time factor prevents indefinite dominance!

---

## Green Light Duration with Small Scale

### Formula
```cpp
greenTime = BASE_GREEN_TIME + (vehicleCount × EXTENSION_PER_VEHICLE);
greenTime = max(MIN_GREEN_TIME, min(MAX_GREEN_TIME, greenTime));
```

### Calculated Durations

| Cars | Calculation | Duration | Enough Time? |
|------|-------------|----------|--------------|
| 1    | 8000 + (1×400) = 8400ms | 8.4s | ✓ Plenty |
| 3    | 8000 + (3×400) = 9200ms | 9.2s | ✓ Good |
| 5    | 8000 + (5×400) = 10000ms | 10.0s | ✓ Adequate |
| 8    | 8000 + (8×400) = 11200ms | 11.2s | ✓ OK |

**Note:** At 1 car/second exit rate:
- 5 cars need 5 seconds minimum
- System gives 10 seconds → **2× buffer** (safe!)

---

## Physical Model Setup Recommendations

### Track Layout

```
ENTRY SENSOR          EXIT SENSOR
     ↓                     ↓
  [====]    [LIGHT]    [====]
     │         │          │
  Vehicle   Stop/Go    Vehicle
  Enters    Signal     Exits
```

**Spacing:**
- Entry sensor: 15cm before traffic light
- Exit sensor: 15cm after traffic light
- Total detection zone: ~30-40cm

### Vehicle Preparation

**For 5-8 toy cars:**
1. Attach small neodymium magnets underneath
2. Label cars 1-8 for tracking
3. Different colors per lane (optional)

**Testing flow:**
1. Pass car through **entry sensor** → count +1
2. Park car at traffic light (wait for green)
3. When green, move car through **exit sensor** → count -1
4. Remove from track

---

## Expected Observations

### With 5-8 Cars Total

**Behavior you should see:**

✅ **1-2 cars per lane:**
- System feels "calm"
- Round-robin mostly
- Short green times (8-9s)

✅ **3-4 cars in one lane:**
- That lane starts getting priority
- Green time extends to 9-10s
- Other lanes still compete

✅ **5-6 cars in one lane:**
- **Clear dominance!**
- Priority score 2-3× higher
- Green time 10-11s
- Other lanes wait longer

✅ **7-8 cars in one lane:**
- **System urgency mode**
- Priority score 4-5× higher
- Maximum responsiveness
- Other lanes only get service via starvation override

### Dashboard Visualization

**Low congestion (1-3 cars):**
```
Lane ONE: Count=2  Priority=0.12  Health=OK
Lane TWO: Count=1  Priority=0.11  Health=OK
```

**High congestion (7-8 cars):**
```
Lane ONE: Count=7  Priority=0.39  Health=OK  🔴🔴🔴
Lane TWO: Count=1  Priority=0.11  Health=OK
```

**Notice:** Priority difference of 3.5× is visually obvious!

---

## Comparison: MAX=30 vs MAX=10

### Same Scenario: Lane with 5 Cars

**MAX_VEHICLES = 30 (Full Scale):**
```
Normalized: 5/30 = 0.167
Quadratic: 0.167² = 0.028
Priority: 0.028 × 0.6 + 0.1 = 0.117
```

**MAX_VEHICLES = 10 (Model Scale):**
```
Normalized: 5/10 = 0.50
Quadratic: 0.50² = 0.25
Priority: 0.25 × 0.6 + 0.1 = 0.250
```

**Difference:** 2.14× higher priority with MAX=10!

### Visual Impact

**MAX=30 (Too gentle for models):**
```
1 car  ▓
2 cars ▓
3 cars ▓░
4 cars ▓░
5 cars ▓░░
```
Barely visible difference!

**MAX=10 (Perfect for models):**
```
1 car  ▓
2 cars ▓░
3 cars ▓░░
4 cars ▓░░░
5 cars ▓░░░░
6 cars ▓░░░░░
7 cars ▓░░░░░░
8 cars ▓░░░░░░░
```
Clear escalation!

---

## Testing Protocol

### Day 1: Baseline (3 cars per lane)
1. Add 3 cars to each lane
2. Record green times (should be ~9s each)
3. Verify round-robin behavior
4. Download CSV after 15 minutes

### Day 2: Congestion Test (8 cars one lane)
1. Add 8 cars to Lane ONE only
2. Add 1 car to other lanes
3. **Observe:** Lane ONE gets priority
4. **Measure:** How much faster does ONE get service?
5. Download CSV, compare with Day 1

### Day 3: Dynamic Test (Gradual buildup)
1. Start with 2 cars per lane
2. Every 2 minutes, add 1 car to Lane ONE
3. **Observe:** When does ONE start dominating? (around 5-6 cars)
4. Chart priority scores over time

### Day 4: Starvation Test
1. Add 8 cars to Lane ONE
2. Add 1 car to Lane TWO
3. **Do NOT let Lane TWO exit cars**
4. Wait 15+ seconds
5. Verify starvation override activates

---

## Data Analysis

### CSV Output Example (Model Scale)

```csv
Timestamp,Lane,VehicleCount,TotalEntered,TotalExited,WaitTime,GreenDuration,PriorityScore,SensorHealth
120,ONE,3,8,5,0,9200,0.154,OK
145,TWO,5,12,7,5200,10000,0.250,OK
170,THREE,1,3,2,8100,8400,0.106,OK
195,FOUR,2,6,4,0,8800,0.124,OK
220,ONE,7,15,8,12000,11200,0.394,OK
```

**Analysis:**
- Lane ONE with 7 cars got **0.394 priority** (highest)
- Lane THREE with 1 car got **0.106 priority** (lowest)
- Ratio: 3.7× difference (quadratic effect visible!)

### Expected Metrics (1-hour test)

| Metric | Target (MAX=10) |
|--------|-----------------|
| Total cycles | 150-250 |
| Avg cars per cycle | 3-5 |
| Avg green time | 9-10 seconds |
| Priority spread | 2-4× between min/max |
| Starvation events | 0-2 (if testing) |

---

## Troubleshooting Small-Scale Issues

### Issue: All priorities seem equal
**Solution:** Increase car count variation (e.g., 1 car vs 7 cars)

### Issue: System too sensitive
**Solution:** Increase MAX_VEHICLES to 12 or 15

### Issue: System not sensitive enough
**Solution:** Decrease MAX_VEHICLES to 8 or even 6

### Issue: Green times too long
**Solution:** Reduce `BASE_GREEN_TIME` from 8000 to 6000ms

### Issue: Cars exit too fast to observe
**Solution:** Increase `EXTENSION_PER_VEHICLE` from 400 to 800ms

---

## Recommended Configuration Summary

```cpp
// Perfect for 5-8 toy car testing
const float MAX_VEHICLES_PER_LANE = 10.0;
const unsigned long BASE_GREEN_TIME = 8000;
const unsigned long EXTENSION_PER_VEHICLE = 400;
#define CONGESTION_MODEL "QUADRATIC"
```

**This gives you:**
- ✅ Visible priority differences with just 1-8 cars
- ✅ Quadratic pressure feels realistic at small scale
- ✅ Green times appropriate for manual car movement
- ✅ Easy to demonstrate system intelligence

---

## Video Demonstration Script

**Script for showing off your model:**

1. **Start:** All lanes empty → lights cycle evenly
2. **Add 3 cars to Lane ONE** → ONE gets priority
3. **Add 5 cars to Lane TWO** → TWO dominates!
4. **Show dashboard:** Priority scores clearly different
5. **Remove cars from TWO** → system rebalances
6. **Add 1 car to each lane** → perfect round-robin returns

**Key talking points:**
- "Notice how 5 cars creates 4× the pressure of 2 cars - that's quadratic!"
- "The system prevents one lane from starving by the 15-second override"
- "Web dashboard shows real-time priorities and sensor health"
- "All data is logged to CSV for traffic analysis"

---

**🚗 Perfect for Demos, Science Fairs, and Educational Displays! 🚗**
