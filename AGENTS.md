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

## Build (VSCode only)
- **ALWAYS** use `ESP-IDF: Build your project` from Command Palette (`F1`).
- **NEVER** use the generic CMake build / "Kit" selector — that tries to compile ESP32 code for the host.
- If IntelliSense shows missing includes (`#include` errors 1696), run `ESP-IDF: Build your project` once to generate `compile_commands.json`, then `ESP-IDF: Add vscode configuration folder`.

## Target
- `idf.py set-target esp32` (ESP32 original, not S2/S3/C3/C6).

## Hardware architecture (verified)
- **Power**: 1×Li-ion 3.7V 2200mAh → TP4056 → MT3608 (Step-Up 5.0V) → VIN ESP32 and servos. **No external regulators** (AMS1117 removed). Risk of brownout mitigated by software: move one servo at a time, monitor voltage.
- **Voltage sensing**: Voltage dividers using **3×10kΩ resistors per divider** (R1=20kΩ, R2=10kΩ, factor=×3). No 1µF capacitor on ADC (software 30-sample moving average at 50ms compensates).
- **Servos**: SG90 ×2, 50Hz PWM via LEDC, 13-bit resolution, 500-2500µs pulse range.
- **LDR**: 4 photoresistors on ADC1, GPIOs 32, 33, 36, 39. ~200 raw-value deadzone (≈5% threshold).
- **ADC sharing caveat**: Both `power_monitor` and `ldr_sensor` use `ADC_UNIT_1` — their `init()` both try `adc_oneshot_new_unit()`. Integration must share one handle or use `adc_oneshot_new_unit()` only once.

## Component APIs
| Component | Init | Read/Get | Notes |
|-----------|------|----------|-------|
| `power_monitor` | `power_monitor_init()` | `get_battery_voltage()` → float V, `get_solar_voltage()` → float V | Background task samples at 50ms, 30-sample avg |
| `ldr_sensor` | `ldr_sensor_init()` | `get_errors(&az, &el)` → int error, `get_raw_values(&t,&b,&l,&r)` | Background task, complementary filter, deadzone applied |
| `servo_motor` | `servo_motor_init()` | `set_angle(SERVO_AXIS_AZIMUT, deg)`, `set_angle(SERVO_AXIS_ELEVATION, deg)` | Blocks until duty cycle update, 0-180° |
| `udp_logger` | `udp_logger_init()` | `udp_logger_server_task` | Pull-based architecture: ESP32 runs a UDP server on port 8080. When it receives a request (e.g. "GET"), it replies with "BAT:x.xx|SOL:y.yy". No `rp_filter` issues. |

## Component dependencies (`CMakeLists.txt` REQUIRES)
- `power_monitor`: `REQUIRES esp_adc freertos`
- `ldr_sensor`: `REQUIRES esp_adc freertos`
- `servo_motor`: `REQUIRES driver`

`main/CMakeLists.txt` must `REQUIRES` all components used (currently missing this — needs `power_monitor ldr_sensor servo_motor`).

## Deferred features (Etapa 2+)
- LCD 1602 I2C display (component not created yet)
- Cloud comm (Supabase + Telegram bot, `cloud_comm` component pending)
- PC daemon (Python/Node, in `daemon_pc/`)
