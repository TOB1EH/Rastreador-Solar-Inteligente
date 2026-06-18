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

## LDR → Servo integration guide (for teammate)

### Init order in `app_main`
```c
power_monitor_init();
ldr_sensor_init(power_monitor_get_adc_handle());   // share ADC_UNIT_1
servo_motor_init();
udp_logger_init();
xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
```

### Tracking task pattern
```c
void tracker_task(void *pv) {
    int az = 90, el = 45;  // tracked positions
    servo_motor_set_angle(SERVO_AXIS_AZIMUT, az);
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, el);
    vTaskDelay(pdMS_TO_TICKS(1000));
    while (1) {
        int az_err, el_err;
        ldr_sensor_get_errors(&az_err, &el_err);
        // Move ONE servo at a time (brownout prevention):
        if (az_err != 0) {
            az += (az_err > 0) ? -1 : 1;
            if (az < 0) az = 0; if (az > 180) az = 180;
            servo_motor_set_angle(SERVO_AXIS_AZIMUT, az);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (el_err != 0) {
            el += (el_err > 0) ? 1 : -1;
            if (el < 0) el = 0; if (el > 90) el = 90;  // panel limit
            servo_motor_set_angle(SERVO_AXIS_ELEVATION, el);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
xTaskCreatePinnedToCore(tracker_task, "tracker", 4096, NULL, 5, NULL, 1);
```

### Key constraints
- **ADC handle**: `ldr_sensor_init(handle)` receives it from `power_monitor_get_adc_handle()`
- **Brownout**: move one servo at a time, 100ms gap between them
- **Elevation limit**: panel mechanically limited 0–90° (clamp `el`)
- **Deadzone**: `DEADZONE_THRESHOLD = 200` raw (~5%), errors below it are ignored
- **Step**: `LDR_STEP_DEG = 1` degree per cycle, adjust for aggressiveness
- **main/CMakeLists.txt** already includes `ldr_sensor servo_motor power_monitor udp_logger` in REQUIRES

## Deferred features (Etapa 2+)
- LCD 1602 I2C display (component not created yet)
- Cloud comm (Supabase + Telegram bot, `cloud_comm` component pending)
- Web Dashboard (HTTP server, `web_dashboard` component pending)
- Telegram Bot (`telegram_bot` component pending)
