# Hardware Wiring Guide - Smart Traffic Control v2.0

## Components Required

### Main Components
- 1x ESP32 DevKit (38-pin)
- 1x TLC5947 24-channel PWM LED driver board
- 8x A3144 Hall Effect Sensors (2 per lane)
- 12x Traffic Light LEDs (3 per lane: Red, Yellow, Green)
- 12x 220Ω Resistors (for LEDs)
- 8x Neodymium magnets (for A3144 sensors)
- Breadboard or custom PCB
- Power supply (5V/2A recommended)

## TLC5947 Connections

### ESP32 to TLC5947
```
ESP32 GPIO 13 (MOSI) → TLC5947 DIN (Data In)
ESP32 GPIO 14 (SCK)  → TLC5947 CLK (Clock)
ESP32 GPIO 15        → TLC5947 LAT (Latch)
ESP32 GND            → TLC5947 GND
ESP32 5V             → TLC5947 VCC
```

### TLC5947 LED Mapping
The TLC5947 has 24 PWM channels (0-23). Our mapping:

**Lane ONE** (Channels 0-2):
- Channel 0: Red LED
- Channel 1: Yellow LED
- Channel 2: Green LED

**Lane TWO** (Channels 3-5):
- Channel 3: Red LED
- Channel 4: Yellow LED
- Channel 5: Green LED

**Lane THREE** (Channels 6-8):
- Channel 6: Red LED
- Channel 7: Yellow LED
- Channel 8: Green LED

**Lane FOUR** (Channels 9-11):
- Channel 9: Red LED
- Channel 10: Yellow LED
- Channel 11: Green LED

**Spare Channels**: 12-23 (available for future expansion)

### LED Connections
Each LED connects from TLC5947 channel → 220Ω resistor → LED anode (+)
LED cathode (-) → GND

```
TLC5947 OUT[0] ──[220Ω]──(+LED-)── GND  (Lane ONE Red)
TLC5947 OUT[1] ──[220Ω]──(+LED-)── GND  (Lane ONE Yellow)
TLC5947 OUT[2] ──[220Ω]──(+LED-)── GND  (Lane ONE Green)
... (repeat for all 12 LEDs)
```

## A3144 Hall Effect Sensor Connections

### Sensor Pin Configuration
A3144 Hall Sensors have 3 pins (flat side facing you, pins down):
```
Pin 1 (Left):  VCC (+5V)
Pin 2 (Middle): GND
Pin 3 (Right):  OUTPUT (digital signal)
```

### ESP32 to A3144 Sensors

**Lane ONE Sensors:**
```
Entry Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 32

Exit Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 33
```

**Lane TWO Sensors:**
```
Entry Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 25

Exit Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 26
```

**Lane THREE Sensors:**
```
Entry Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 27

Exit Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 14
```

**Lane FOUR Sensors:**
```
Entry Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 12

Exit Sensor:
  VCC → ESP32 5V
  GND → ESP32 GND
  OUT → ESP32 GPIO 13
```

### Important Notes on A3144 Sensors

1. **Pull-up Resistors**: ESP32's internal pull-ups are used (`INPUT_PULLUP`)
2. **Trigger Logic**: A3144 outputs **LOW when magnet is detected** (South pole)
3. **Interrupt Mode**: `FALLING` edge triggers the interrupt
4. **Magnet Placement**: 
   - Position magnet on vehicle/toy car
   - Sensor detects when magnet passes within ~1cm
   - Use South pole of magnet facing sensor

## Power Distribution

### Recommended Power Setup
```
5V Power Supply (2A minimum)
├── ESP32 5V pin
├── TLC5947 VCC
└── All 8x A3144 sensors VCC

Common GND for all components
```

### Current Requirements
- ESP32: ~250mA (with WiFi)
- TLC5947: ~10mA (IC only)
- 12 LEDs: ~240mA (assuming 20mA per LED)
- 8 A3144 sensors: ~40mA (5mA each)
- **Total**: ~550mA (use 2A supply for safety margin)

## Physical Sensor Placement

### Entry Sensor Placement
```
[Before Lane]
     ↓
  [ENTRY]  ← Sensor here
     ↓
[In Lane - Traffic Light Zone]
```

### Exit Sensor Placement
```
[In Lane - Traffic Light Zone]
     ↓
   [EXIT]  ← Sensor here
     ↓
[After Lane]
```

### Sensor Spacing
- Entry sensor: 10-15cm before traffic light
- Exit sensor: 10-15cm after traffic light
- This creates a "detection zone" where vehicles are counted

## Testing Checklist

### 1. TLC5947 Test
- Upload test sketch that cycles through all 12 LED channels
- Verify each LED lights up correctly
- Check PWM brightness control

### 2. A3144 Sensor Test
- Pass magnet near each sensor
- Monitor Serial output for trigger events
- Verify both entry and exit sensors work per lane

### 3. System Integration Test
- Run full traffic control code
- Simulate vehicle passage (magnet on toy car)
- Verify count increments on entry, decrements on exit
- Check web dashboard updates

### 4. Network Test
- Verify WiFi connection
- Access web dashboard at ESP32 IP address
- Test CSV data download
- Check telnet logging (telnet [IP] 23)

## Troubleshooting

### Issue: LEDs not lighting
- Check TLC5947 connections (DIN, CLK, LAT)
- Verify 5V power to TLC5947
- Check resistor values (220Ω)
- Test with example TLC5947 library code

### Issue: Sensors not triggering
- Check A3144 wiring (VCC, GND, OUT)
- Verify magnet polarity (South pole to sensor)
- Test magnet distance (should be <1cm)
- Check Serial monitor for interrupt triggers

### Issue: WiFi not connecting
- Verify SSID and password in code
- Check WiFi signal strength
- Try connecting to 2.4GHz network (ESP32 doesn't support 5GHz)

### Issue: Negative vehicle counts
- Sensors may be too sensitive (adjust debounce time)
- Check for magnetic interference
- Verify entry sensor triggers before exit sensor
- Review sensor health in web dashboard

## Schematic Overview

```
                    ESP32
                      │
      ┌───────────────┼───────────────┐
      │               │               │
   GPIO13/14/15   GPIO32-33      5V + GND
      │            (sensors)          │
      │               │               │
   TLC5947      A3144 Sensors    Power Bus
      │          (×8 total)          │
      │               │               │
   12 LEDs         Magnets      Distribution
  (R,Y,G×4)      (on vehicles)
```

## Advanced: Custom PCB Design

For a permanent installation, consider designing a PCB with:
- ESP32 module socket
- TLC5947 breakout area
- Screw terminals for sensors
- LED current-limiting resistors on-board
- Voltage regulation (if using 12V input)
- Status indicator LEDs

## Safety Notes

⚠️ **Important:**
- Never exceed 5V on A3144 sensors
- TLC5947 can sink up to 120mA per channel, but 20mA per LED is recommended
- Use proper wire gauge for power distribution
- Add fuses if using high-current LEDs
- Keep magnets away from sensitive electronics
