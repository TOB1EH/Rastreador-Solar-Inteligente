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

## Current firmware (sin LDR)
Solo monitorea voltajes (batería/panel) y sirve UDP. Sin tracker, sin LDR.
- `main/main.c`: `power_monitor_init()` + `servo_motor_init()` + `udp_logger_init()`
- `main/CMakeLists.txt`: `REQUIRES power_monitor servo_motor udp_logger`

## Hardware architecture (verified)
- **Power**: 1×Li-ion 3.7V 2200mAh → TP4056 → MT3608 (Step-Up 5.0V) → VIN ESP32 and servos.
- **Flashing**: solo USB, sin batería. **Operación**: solo batería, sin USB.
- **Voltage sensing**: Voltage dividers using **3×10kΩ resistors per divider** (R1=20kΩ, R2=10kΩ, factor=×3). No 1µF capacitor on ADC (software 30-sample moving average at 50ms compensates).
- **Servos**: SG90 ×2, 50Hz PWM via LEDC, 13-bit resolution, 500-2500µs pulse range.

## Component APIs
| Component | Init | Read/Get | Notes |
|-----------|------|----------|-------|
| `power_monitor` | `power_monitor_init()` | `get_battery_voltage()` → float V, `get_solar_voltage()` → float V | Background task samples at 50ms, 30-sample avg |
| `servo_motor` | `servo_motor_init()` | `set_angle(SERVO_AXIS_AZIMUT, deg)`, `set_angle(SERVO_AXIS_ELEVATION, deg)` | Blocks until duty cycle update, 0-180° |
| `udp_logger` | `udp_logger_init()` | `udp_logger_server_task` | Pull-based architecture: ESP32 runs a UDP server on port 8080. When it receives a request (e.g. "GET"), it replies with "BAT:x.xx|SOL:y.yy". |

## Deferred features (Etapa 2+)
- **LDR → Servo integration** (component `ldr_sensor` exists but not used in main.c)
  - 4 photoresistors on ADC1, GPIOs 32, 33, 36, 39. ~200 raw-value deadzone.
  - ADC sharing: `ldr_sensor_init(handle)` receives handle from `power_monitor_get_adc_handle()`
  - Move one servo at a time (anti-brownout), 100ms gap between them
  - Elevation limited 0–90°, step 1° per cycle
- LCD 1602 I2C display
- Cloud comm (Supabase + Telegram bot)
- Web Dashboard (HTTP server)
- Telegram Bot
