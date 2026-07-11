# Rastreador Solar Inteligente — Agent Guide

## Project
ESP32 dual-axis solar tracker (azimuth + elevation). ESP-IDF v5.x + FreeRTOS.

## Project structure
```
Rastreador_Solar_Inteligente/
├── firmware/                  # ESP-IDF project root
│   ├── CMakeLists.txt         # project() includes IDF_PATH
│   ├── main/main.c            # entrypoint (app_main)
│   ├── components/
│   │   ├── power_monitor/     # ADC → battery/solar voltage (via 1/3 divider)
│   │   ├── ldr_sensor/        # 4×LDR → azimuth/elevation error
│   │   ├── servo_motor/       # 2×SG90 via LEDC PWM
│   │   └── udp_logger/        # WiFi STA + UDP Server (Pull mode)
│   ├── sdkconfig              # set-target esp32, auto-generated
│   └── build/                 # build artifacts (gitignored)
├── daemon_pc/                 # PC monitoring scripts
│   └── monitor.py             # UDP Client (requests data from ESP32)
├── docs/                      # markdown docs
└── AGENTS.md                  # this file
```

## Build (terminal)
```bash
cd firmware
. /home/tobias/.espressif/v6.0.1/esp-idf/export.sh
idf.py build
```
- **NEVER** use the generic CMake build / "Kit" selector in VSCode — that tries to compile ESP32 code for the host.

## Target
- `idf.py set-target esp32` (ESP32 original, not S2/S3/C3/C6).

## ⚠️ Status: ESP32 board burned (Jun 2026)
- **Causa**: se conectó batería (MT3608 → VIN) mientras el ESP32 estaba conectado por USB.
- **Síntoma**: ESP32 se calienta al conectar USB, no responde via CH340.
- **Solución**: comprar nuevo ESP32 DevKit (ESP32-WROOM-32, CH340).
- **Regla**: nunca conectar VIN y USB al mismo tiempo.

## Hardware architecture (verified)
- **Power**: 1×Li-ion 3.7V 2200mAh → TP4056 → MT3608 (Step-Up 5.0V) → VIN ESP32 and servos.
- **Flashing**: solo USB, sin batería. **Operación**: solo batería, sin USB.
- **Voltage sensing**: Voltage dividers using **3×10kΩ resistors per divider** (R1=20kΩ, R2=10kΩ, factor=×3). No 1µF capacitor on ADC (software 30-sample moving average at 50ms compensates).
- **Servos**: SG90 ×2, 50Hz PWM via LEDC, 13-bit resolution, 500-2500µs pulse range.
- **LDR wiring (ADC1)**:
  - TL = GPIO39 → ADC_CHANNEL_3 (TOP_LEFT)
  - TR = GPIO33 → ADC_CHANNEL_5 (TOP_RIGHT)
  - BL = GPIO32 → ADC_CHANNEL_4 (BOT_LEFT)
  - BR = GPIO36 → ADC_CHANNEL_0 (BOT_RIGHT)
- **Servo wiring**:
  - GPIO18 = SERVO_AXIS_AZIMUT (lower servo, rotation left/right)
  - GPIO19 = SERVO_AXIS_ELEVATION (upper servo, tilt up/down)

## Component APIs
| Component | Init | Read/Get/Set | Notes |
|-----------|------|--------------|-------|
| `power_monitor` | `power_monitor_init()` | `get_battery_voltage()` → float V, `get_solar_voltage()` → float V | Background task samples at 50ms, 30-sample avg. Exposes `adc_oneshot_unit_handle_t` via `power_monitor_get_adc_handle()` |
| `ldr_sensor` | `ldr_sensor_init(adc_handle)` | `get_errors(&az, &el)`, `get_individual(&tl,&tr,&bl,&br)` | Background task samples at 50ms, 50/50 exponential filter. `get_errors` → err_x = avg_left - avg_right, err_y = avg_top - avg_bot |
| `servo_motor` | `servo_motor_init()` | `set_angle(axis, deg)`, `set_angle_blocking(axis, deg)`, `wake_and_move(axis, deg)`, `sleep(axis)` | `set_angle` = smooth fade (40ms, NO_WAIT). `wake_and_move` = set duty + 100ms delay. `sleep` = duty=0. Range 0-180° |
| `udp_logger` | `udp_logger_init()` | `udp_logger_server_task` | Pull-based UDP server on port 8080. Responds "BAT:x.xx\|SOL:y.yy" to any request |

## Tracking logic (main.c)
- **Errors**: `ldr_sensor_get_errors()` → err_x (horizontal) for azimuth, err_y (vertical) for elevation
- **One servo at a time**: alternates via `move_azimuth` flag (anti-brownout)
- **Proportional step**: step = 1 + abs(err)/150, capped at 5 (az) / 3 (el), with ERR_THRESHOLD=30
- **Battery hysteresis**: tracking ON ≥3.5V, OFF ≤3.2V. USB fallback when v_bat < 1.0V
- **Sleep/wake**: servo gets PWM for 100ms per move, then duty=0 to save battery
- **Interval**: 500ms between cycles (each servo moves at most every 1000ms)
- **Ranges**: azimuth 0-180°, elevation 95-175°
- **Console log**: Bat, Panel, Az, El, err_x, err_y, TL, TR, BL, BR (every 2s)

## Critical Context
- ADC channels configured once at boot in `power_monitor_init()` (all 6: 2 power + 4 LDR) to prevent timeout during concurrent reads
- LDR channel mapping is per the verified hardware wiring above — do NOT change
- **IMPORTANT**: named `SERVO_AXIS_AZIMUT` (GPIO18) controls rotation (horizontal), `SERVO_AXIS_ELEVATION` (GPIO19) controls tilt (vertical). These names match the physical function.
- `err_x` (horizontal imbalance) drives azimuth servo; `err_y` (vertical imbalance) drives elevation servo
- Capacitor recommendation: 470-1000µF 25V electrolytic + 100nF ceramic close to ESP32 VIN; wiring diagram in `docs/capacitor_wiring.html`
