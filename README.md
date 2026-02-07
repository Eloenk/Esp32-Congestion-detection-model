Got it — if you're happy with the current version, that's great!  
Here's an updated **README.md** that reflects the final state of the project (v3.0 with manual override, polished fixes, rich logging, dashboard, etc.).

```markdown
# Smart Traffic Control System v3.0

ESP32-based smart traffic light controller for a 4-way intersection using Hall effect sensors and a TLC5947 LED driver.

## Features

- 4 independent lanes with dual Hall sensors (entry + exit) per lane
- Real-time vehicle counting (entry +1, exit -1)
- Adaptive green time based on vehicle count
- Configurable congestion scoring model (Quadratic by default)
- Priority-based lane selection with starvation protection
- Sensor health monitoring & anomaly detection
- Detailed CSV logging (wait time, priority score, sensor health, etc.)
- Beautiful real-time web dashboard
- **Manual override / emergency control**:
  - Force any lane green
  - Force all-red
  - Resume automatic mode
- TCP logging (telnet port 23)
- SPIFFS-based persistent CSV logs

## Hardware

- ESP32 DevKit / Dev Module
- 1× Adafruit TLC5947 (24-channel PWM LED driver)
- 12× LEDs (3 per lane: Red, Yellow, Green)
- 8× A3144 Hall effect sensors (2 per lane: entry & exit)
- Magnets on vehicles/toy cars to trigger sensors

## Pin Mapping

| Component              | ESP32 Pin | Notes                     |
|------------------------|-----------|---------------------------|
| TLC5947 DIN (MOSI)     | GPIO 13   | Data                      |
| TLC5947 CLK (SCK)      | GPIO 14   | Clock                     |
| TLC5947 LAT            | GPIO 15   | Latch                     |
| Lane 1 Entry           | GPIO 32   |                           |
| Lane 1 Exit            | GPIO 33   |                           |
| Lane 2 Entry           | GPIO 25   |                           |
| Lane 2 Exit            | GPIO 26   |                           |
| Lane 3 Entry           | GPIO 27   |                           |
| Lane 3 Exit            | GPIO 14   | Shared with TLC CLK (ok)  |
| Lane 4 Entry           | GPIO 12   |                           |
| Lane 4 Exit            | GPIO 13   | Shared with TLC DIN (ok)  |

## Web Dashboard

Access via browser at the ESP32's IP address (shown in serial monitor).

Features:
- Live system status (uptime, state, active lane)
- Per-lane stats (count, entered/exited, priority, sensor health)
- Manual override controls:
  - Force Lane 1–4 green
  - All Red (emergency stop)
  - Resume Auto mode
- Download / clear CSV logs

## Logging

- **Serial + TCP (telnet <IP> 23)**: detailed real-time logs
- **CSV file** (`/traffic_log.csv`): saved to SPIFFS
  - Columns: Timestamp, Lane, VehicleCount, TotalEntered, TotalExited, WaitTime, GreenDuration, PriorityScore, SensorHealth

## How to Use

1. Update `WIFI_SSID` and `WIFI_PASSWORD`
2. Upload to ESP32
3. Open Serial Monitor (115200 baud) to see IP address
4. Visit `http://<IP>` in browser
5. Test automatic mode or use manual override

## Congestion Models

Change via `#define CONGESTION_MODEL`:

```cpp
#define CONGESTION_MODEL "QUADRATIC"   // default - recommended
// "LINEAR"
// "CUBIC"
// "SQRT"
```

`MAX_VEHICLES_PER_LANE = 10.0` is tuned for small-scale testing (toy cars).

## Future Improvements (ideas)

- WiFi auto-reconnect
- Manual mode timeout (auto-resume after X minutes)
- Basic authentication on manual endpoints
- Directional counting validation (entry → exit sequence)
- Pedestrian demand input

## License

MIT – feel free to modify and use!

Happy building! 🚦
```

Feel free to copy-paste this directly into your `README.md`.

If you want to add screenshots, badges, license link, or expand any section, just let me know.  
Anything else you'd like to tweak or add next? 😄