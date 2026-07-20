# Rastreador Solar Inteligente — Sistema de Seguimiento Solar Dual-Eje

**Proyecto Integrador — Sistemas de Tiempo Real 2026**  
**Autores:** Brambilla Agustín Eduardo, Funes Tobias  
**Controlador:** ESP32-WROOM-32 | **Framework:** ESP-IDF v6.0.1 + FreeRTOS  
**Ejes:** Azimut (0–180°) + Elevación (95–175°)

---

## 1. Introducción

Sistema autónomo de seguimiento solar en dos ejes que maximiza la captación de radiación fotovoltaica mediante control diferencial de 4 sensores LDR dispuestos en cuadrantes. El firmware corre sobre FreeRTOS con tareas concurrentes para adquisición ADC, control de servos, monitoreo de batería y comunicación WiFi/UDP.

La radiación solar no es constante: el sol cambia de posición en azimut (horizontal) y elevación (vertical). Los paneles fijos capturan 40–60% menos energía. Este sistema ajusta automáticamente la orientación del panel para mantener la perpendicularidad con la fuente de luz, maximizando la generación.

---

## 2. Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────────────┐
│                     RASTREADOR SOLAR DUAL-EJE                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  SENSORES (ADC1)        PROCESAMIENTO            ACTUADORES      │
│  ──────────────────────────────────────────────────────────      │
│  ┌── TL ──── GPIO39 ──┐                       ┌──────────────┐  │
│  │  TR ──── GPIO33 ──┤                       │ Servo Azimut  │  │
│  │  BL ──── GPIO32 ──┤──→ ESP32 + FreeRTOS ─→│ GPIO18 (PWM)  │  │
│  │  BR ──── GPIO36 ──┘                       └──────────────┘  │
│  └────────────────────┘                       ┌──────────────┐  │
│  ┌── Batería ─ GPIO34 ─┐                     │ Servo Elev.  │  │
│  │  Panel   ─ GPIO35 ──┤                     │ GPIO19 (PWM)  │  │
│  └──────────────────────┘                    └──────────────┘  │
│                                                                   │
│  COMUNICACIÓN                                                     │
│  ──────────────────────                                           │
│  WiFi STA → Servidor UDP :8080 (modo Pull)                       │
│  PC daemon → monitor.py (consulta cada 1s)                        │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### 2.1. Componentes de Hardware

| Componente | Especificación | Cant. |
|------------|---------------|-------|
| ESP32 DevKit v1 | ESP32-WROOM-32, CH340 | 1 |
| Servo SG90 | 0–180°, 50Hz PWM | 2 |
| LDR 5mm | 200–20kΩ + resistor 10kΩ | 4 |
| Panel solar | 5.5V / 100mA | 1–2 |
| Batería Li-ion | 3.7V 2200mAh | 1 |
| TP4056 | Cargador Li-ion | 1 |
| MT3608 | Step-Up 5.0V | 1 |
| Resistencias 10kΩ | Divisores de tensión (×3) | 10 |

### 2.2. Power Distribution

```
ESP32
├── power_monitor   → ADC batería/panel (divisor ×3, filtro 30 muestras)
├── ldr_sensor      → 4 LDR → error azimut/elevación (zona muerta ~200)
├── servo_motor     → 2×MG90S vía LEDC PWM (50Hz, 13-bit, 500-2500µs)
├── udp_logger      → WiFi STA + servidor UDP pull + NVS credentials
├── web_dashboard   → Servidor HTTP embebido (monitoreo en navegador)
└── telegram_bot    → Bot Telegram (consulta de voltajes desde cualquier lado)

PC daemon_pc/
├── monitor.py         → consulta ESP32 cada 1s (UDP), muestra voltajes
└── configure_wifi.py  → configura WiFi del ESP32 sin recompilar
```



## Integración LDR ↔ Servos

Panel Solar → TP4056 → Li-ion 3.7V → MT3608 (5.0V) → VIN ESP32 + Servos SG90
                                              └→ 3.3V (LDO interno ESP32 → LDRs)

Divisores de tensión: R1=20kΩ, R2=10kΩ → factor ×3
Flashing:     solo USB, sin batería
Operación:    solo batería, sin USB  ← NUNCA conectar ambos simultáneamente
```

### 2.3. Mapeo de Pines (verificado)

| Señal | GPIO | Canal ADC | Componente |
|-------|------|-----------|------------|
| TOP_LEFT (TL) | GPIO39 | ADC1_CH3 | LDR cuadrante superior izquierdo |
| TOP_RIGHT (TR) | GPIO33 | ADC1_CH5 | LDR cuadrante superior derecho |
| BOT_LEFT (BL) | GPIO32 | ADC1_CH4 | LDR cuadrante inferior izquierdo |
| BOT_RIGHT (BR) | GPIO36 | ADC1_CH0 | LDR cuadrante inferior derecho |
| BATTERY | GPIO34 | ADC1_CH6 | Voltaje batería (divisor ×3) |
| SOLAR | GPIO35 | ADC1_CH7 | Voltaje panel solar (divisor ×3) |
| SERVO_AZIMUT | GPIO18 | — | PWM 50Hz, azimut (rotación horizontal) |
| SERVO_ELEVATION | GPIO19 | — | PWM 50Hz, elevación (inclinación vertical) |

---

## 3. Componentes de Software (Módulos ESP-IDF)

### 3.1. `power_monitor` — Monitoreo de Voltajes

**Archivos:** `components/power_monitor/power_monitor.c`, `include/power_monitor.h`

**Responsabilidad:** Lee los voltajes de batería y panel solar mediante ADC1 con 30 muestras de promedio móvil a 50ms. Expone el handle del ADC para que otros componentes (LDR) lo compartan.

**API:**
| Función | Descripción |
|---------|-------------|
| `power_monitor_init()` | Inicializa ADC1, configura 6 canales (2 power + 4 LDR), arranca tarea FreeRTOS en Core 1 |
| `power_monitor_get_battery_voltage()` | Retorna float en Voltios (ej. 4.12) |
| `power_monitor_get_solar_voltage()` | Retorna float en Voltios (ej. 5.50) |
| `power_monitor_get_adc_handle()` | Retorna `adc_oneshot_unit_handle_t` compartido |

**Tarea interna (`power_monitor_task`):**
- Se ejecuta en Core 1, prioridad 5, cada 50ms
- Buffer circular de 30 muestras por canal
- Promedio aritmético al llegar a 30 muestras
- Conversión a mV mediante calibración ADC (curve-fitting) o fallback teórico `(raw * 3300) / 4095`
- Multiplica por `VOLTAGE_DIVIDER_FACTOR = 3.0f` para compensar divisor físico

**ADC channels configurados (en `power_monitor.c`):**
```c
ADC_CHAN_BATTERY   = ADC_CHANNEL_6  // GPIO34
ADC_CHAN_SOLAR     = ADC_CHANNEL_7  // GPIO35
ADC_CHAN_LDR_TL    = ADC_CHANNEL_0  // GPIO36  (BR en ldr_sensor)
ADC_CHAN_LDR_TR    = ADC_CHANNEL_5  // GPIO33  (TR)
ADC_CHAN_LDR_BL    = ADC_CHANNEL_4  // GPIO32  (BL)
ADC_CHAN_LDR_BR    = ADC_CHANNEL_3  // GPIO39  (TL)
```
> **Nota:** Todos los canales se configuran una sola vez en `power_monitor_init()` para evitar timeouts por reconexión concurrente.

---

### 3.2. `ldr_sensor` — Sensor Diferencial de Luz

**Archivos:** `components/ldr_sensor/ldr_sensor.c`, `include/ldr_sensor.h`

**Responsabilidad:** Lee 4 LDR dispuestos en cuadrantes y calcula errores diferenciales para azimut (error horizontal) y elevación (error vertical).

**API:**
| Función | Descripción |
|---------|-------------|
| `ldr_sensor_init(adc_handle)` | Almacena el handle ADC y arranca tarea FreeRTOS en Core 1 |
| `ldr_sensor_get_errors(&az, &el)` | `err_x = avg_left - avg_right`; `err_y = avg_top - avg_bot` |
| `ldr_sensor_get_raw_values(&t, &b, &l, &r)` | Promedios combinados por cuadrante |
| `ldr_sensor_get_individual(&tl, &tr, &bl, &br)` | Valores individuales de cada LDR |

**Tarea interna (`ldr_sensor_task`):**
- Se ejecuta en Core 1, prioridad 5, cada 50ms
- Lee los 4 canales ADC con reintentos (3 intentos, timeout 10ms)
- Filtro exponencial 50/50: `avg = (avg + new) / 2`
- Calcula: `err_x = (avg_tl+avg_bl)/2 - (avg_tr+avg_br)/2` (izquierda vs derecha)
- Calcula: `err_y = (avg_tl+avg_tr)/2 - (avg_bl+avg_br)/2` (arriba vs abajo)
- Zona muerta configurable (`DEADZONE_THRESHOLD`): si `abs(err) < threshold`, err = 0

**Lógica de error:**
- `err_x > 0` → luz a la izquierda → mover azimut a la izquierda (CW o CCW según montaje)
- `err_x < 0` → luz a la derecha → mover azimut a la derecha
- `err_y > 0` → luz arriba → aumentar elevación
- `err_y < 0` → luz abajo → disminuir elevación
- `err = 0` → centrado → no mover

---

### 3.3. `servo_motor` — Control de Servos SG90

**Archivos:** `components/servo_motor/servo_motor.c`, `include/servo_motor.h`

**Responsabilidad:** Controla 2 servos SG90 mediante LEDC PWM con resolución de 13 bits a 50Hz.

**API:**
| Función | Descripción |
|---------|-------------|
| `servo_motor_init()` | Configura timer LEDC, 2 canales PWM (CH0=azimut GPIO18, CH1=elevación GPIO19), instala fade |
| `set_angle(axis, deg)` | Fade suave a ángulo (100ms, NO_WAIT) |
| `set_angle_blocking(axis, deg)` | Fade bloqueante (100ms, WAIT_DONE) |
| `wake_and_move(axis, deg)` | Duty instantáneo + 100ms delay |
| `sleep(axis)` | Duty = 0 (sin PWM, servo libre) |

**Calibración:**
- Frecuencia: 50Hz (período 20ms)
- Resolución: 13 bits (0–8191)
- Pulso mínimo: 500µs (0°)
- Pulso máximo: 2500µs (180°)
- Conversión: `duty = pulsewidth_us * 8192 / 20000`

**Estrategia de ahorro de energía:**
- El servo recibe PWM solo durante 100ms por movimiento
- Luego `sleep()` pone duty = 0
- Esto evita consumo en reposo (~50mA vs ~5µA)

---

### 3.4. `udp_logger` — Servidor UDP Pull

**Archivos:** `components/udp_logger/udp_logger.c`, `include/udp_logger.h`

**Responsabilidad:** Conexión WiFi STA + servidor UDP en modo Pull. Escucha en puerto 8080 y responde con voltajes.

**API:**
| Función | Descripción |
|---------|-------------|
| `udp_logger_init()` | Inicializa NVS, crea event group, configura WiFi STA |
| `udp_logger_server_task(pv)` | Tarea FreeRTOS que crea socket UDP, bindea, responde consultas |

**Funcionamiento:**
1. Inicializa NVS (flash)
2. Crea interfaz netif WiFi STA
3. Se conecta al SSID configurado (vía `menuconfig`)
4. Espera conexión (`WIFI_CONNECTED_BIT`)
5. Crea socket UDP en puerto 8080
6. Por cada datagrama recibido, responde: `"BAT:x.xx|SOL:y.yy"`
7. Monitoreo PC: `python3 daemon_pc/monitor.py`

---

### 3.5. `main.c` — Punto de Entrada y Lógica de Tracking

**Archivo:** `main/main.c`

**Inicialización en `app_main()`:**
1. Desactiva brownout detector (`WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)`)
2. Inicializa power_monitor (ADC + tarea)
3. Inicializa servo_motor (LEDC PWM)
4. Inicializa ldr_sensor con handle ADC compartido
5. Inicializa udp_logger (WiFi + NVS)
6. Mueve servos a posición inicial (90°, 135°) en modo blocking
7. Crea `udp_server` task (stack 4096, prioridad 5, sin pin)
8. Crea `tracker` task (stack 2048, prioridad 5, Core 1)
9. Loop principal: cada 2s logea Bat, Panel, Az, El, err_x, err_y, TL, TR, BL, BR

**Lógica de tracking (`tracker_task`):**

```c
constantes:
  BATTERY_MOVE_ON  = 3.5V   // Tracking se activa
  BATTERY_MOVE_OFF = 3.2V   // Tracking se desactiva (histéresis)
  ERR_THRESHOLD    = 20     // Zona muerta de error
  TRACK_INTERVAL   = 100ms  // Período del ciclo
  AZIMUTH_RANGE    = 0–180°
  ELEVATION_RANGE  = 95–175°

por ciclo:
  1. Leer v_bat = power_monitor_get_battery_voltage()
  2. Evaluar histéresis de batería:
     - Si v_bat < 1.0V → modo USB (bat_ok = true)
     - Si v_bat >= 3.5V → tracking ON
     - Si v_bat < 3.2V  → tracking OFF
  3. Si bat_ok:
     a. Leer err_x, err_y = ldr_sensor_get_errors()
     b. Calcular paso proporcional:
        step = 1 + abs(err) / divisor
        capping: azimut ≤ 8, elevación ≤ 5
        divisor: azimut = 80, elevación = 60
     c. Si step > 0:
        - err > 0 → decrementar ángulo
        - err < 0 → incrementar ángulo
        - Aplicar límites (0–180°, 95–175°)
        - Llamar servo_motor_set_angle()
  4. vTaskDelay(100ms)
```

**Histéresis de batería:**
```
v_bat
  ↑ 3.5V ──────→ Tracking ON
  │ 3.2V ──────→ Tracking OFF
  │ 1.0V ──────→ Fallback USB (bat_ok = true, asume USB power)
  └─────────────────→
```

**Proportional step function:**
```c
static int proportional_step(int err, int max_step, int divisor) {
    int abs_err = abs(err);
    if (abs_err < ERR_THRESHOLD) return 0;   // Zona muerta
    int step = 1 + abs_err / divisor;         // Paso base + proporcional
    if (step > max_step) step = max_step;     // Saturación
    return step;
}
```

**Ejemplo de comportamiento:**
| err_x | step (div=80, max=8) | movimiento |
|-------|---------------------|------------|
| 0–19  | 0                   | nada (zona muerta) |
| 80    | 2                   | 2°        |
| 200   | 3                   | 3°        |
| 600   | 8                   | 8° (cap)  |
| 1000  | 8                   | 8° (cap)  |

---

### 3.6. `web_dashboard` — Dashboard Web

**Archivos:** `components/web_dashboard/web_dashboard.c`, `include/web_dashboard.h`

**Responsabilidad:** Servidor HTTP embebido que expone una pagina web con indicadores de voltaje en tiempo real.

**API:**
| Funcion | Descripcion |
|---------|-------------|
| `web_dashboard_start()` | Espera WiFi, inicia servidor HTTP, registra endpoints |

**Endpoints:**

| Ruta | Metodo | Descripcion |
|------|--------|-------------|
| `/` | GET | Pagina HTML+JS con auto-refresh cada 3s |
| `/api/status` | GET | JSON `{"battery":x.xx,"solar":y.yy}` |
| `/favicon.ico` | GET | 204 No Content (evita error 404) |

**Pagina web:**
- Tema oscuro, dos tarjetas (bateria verde, panel amarillo)
- Fetch periodico a `/api/status` cada 3 segundos
- Indicador de conexion (verde/rojo)
- Sin dependencias externas (no requiere CDN ni frameworks)

**Uso:** Abrir `http://<ip-esp32>/` desde cualquier navegador en la misma red.

Ver `docs/GUIA_WEB_DASHBOARD.md` para mas detalles.

---

### 3.7. `telegram_bot` — Bot Telegram

**Archivos:** `components/telegram_bot/telegram_bot.c`, `cJSON.c`, `include/telegram_bot.h`

**Responsabilidad:** Bot de Telegram que responde comandos con el estado del rastreador desde cualquier lugar con Internet.

**API:**
| Funcion | Descripcion |
|---------|-------------|
| `telegram_bot_start()` | Crea tarea de polling a la API de Telegram |

**Comandos:**
| Comando | Respuesta |
|---------|-----------|
| `/start` | Mensaje de bienvenida con lista de comandos |
| `/status` | `Bateria: 4.12V\nPanel Solar: 5.50V` |
| `/help` | Lista de comandos disponibles |

**Arquitectura:**
- Polling cada 5s a `api.telegram.org` via HTTPS
- Certificate bundle de ESP-IDF (`esp_crt_bundle_attach`)
- Solo responde a chats privados (ignora grupos)
- Token configurable via menuconfig
- `chat_id` de 64 bits manejado con `double` (evita truncamiento en cJSON)

**Uso:** Enviar `/status` al bot desde Telegram.

Ver `docs/GUIA_TELEGRAM_BOT.md` para mas detalles.

---

## 4. Tareas FreeRTOS

| Tarea | Función | Stack | Prioridad | Core | Período |
|-------|---------|-------|-----------|------|---------|
| `tracker` | `tracker_task` | 2048 | 5 | 1 | 100ms |
| `power_monitor` | `power_monitor_task` | 4096 | 5 | 1 | 50ms |
| `ldr_sensor` | `ldr_sensor_task` | 4096 | 5 | 1 | 50ms |
| `udp_server` | `udp_logger_server_task` | 4096 | 5 | any | — |
| `telegram_bot` | `telegram_bot_task` | 8192 | 5 | any | 5s |
| `httpd` | (servidor web) | 4096+ | any | any | — |
| `main` | `app_main` loop | — | 1 | 0 | 2000ms |

**Diagrama de comunicación:**
```
Variable global compartida (adc1_handle)
power_monitor ──→ adc1_handle ──→ ldr_sensor
      │                                │
      │ get_battery_voltage()          │ get_errors()
      │ get_solar_voltage()            │ get_individual()
      ↓          ↓          ↓          ↓
   udp_logger  web_dash   telegram   tracker_task
   (UDP pull)  (HTTP)     (polling)  (controla servos)
                                         ↓
                                    servo_motor
                                    (LEDC PWM)

PC user ←→ web_dashboard (navegador, HTTP)
PC user ←→ telegram_bot  (Telegram API, cloud)
PC user ←→ udp_logger    (monitor.py, UDP)
```

> Nota: No se usan Queues FreeRTOS para la comunicación sensor→tracker. Los valores se comparten via variables globales estáticas con getters. Esto es seguro porque las escrituras son atómicas (int, float en ESP32 sin contención de cache entre cores).

---

## 5. Scripts PC (daemon_pc/)

### 5.1. `monitor.py` — Consulta UDP

Script Python que consulta al ESP32 via UDP cada 1 segundo y muestra voltajes:

```bash
python3 daemon_pc/monitor.py
```

**Salida:**
```
[ESP32 en 192.168.18.149] -> Bateria: 4.12 V  |  Panel Solar: 5.50 V
```

### 5.2. `configure_wifi.py` — Configuracion WiFi

Configura las credenciales WiFi del ESP32 sin recompilar. Escanea redes desde la PC y envia las credenciales por USB serial:

```bash
# Modo interactivo (recomendado)
python3 daemon_pc/configure_wifi.py

# O directamente
python3 daemon_pc/configure_wifi.py --ssid "MiRed" --password "clave"

# Solo escribir en sdkconfig (sin serial)
python3 daemon_pc/configure_wifi.py --no-serial
```

Ver `docs/GUIA_CONFIGURACION_WIFI.md` para mas detalles.
---

## 6. WiFi Configuración

El ESP32 guarda las credenciales WiFi en NVS (persisten entre flashes).
Si no hay credenciales guardadas, entra en **modo config serial** por 5 segundos
y puede recibirlas por UART.

```bash
# Desde la PC, escanea redes disponibles y configura
python daemon_pc/configure_wifi.py

# O directamente
python daemon_pc/configure_wifi.py --ssid "MiRed" --password "clave"
```

Ver `docs/GUIA_CONFIGURACION_WIFI.md` para más detalles.

---

## 7. Compilación y Descarga

```bash
cd firmware
. /home/tobias/.espressif/v6.0.1/esp-idf/export.sh
idf.py set-target esp32
idf.py menuconfig    # Configurar WiFi SSID/PASSWORD, UDP_PORT
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Importante:** Nunca usar el selector "Kit" de CMake en VSCode — compila para host, no para ESP32.

---

## 8. Configuración via Kconfig (menuconfig)

El proyecto usa `menuconfig` para configurar los parametros de cada componente:

| Menu | Opciones | Default |
|------|----------|---------|
| `Telegram Bot Configuration` | `TELEGRAM_BOT_TOKEN`, `TELEGRAM_POLL_SECS` | "", 5s |
| `Web Dashboard Configuration` | `WEB_DASHBOARD_PORT` | 80 |
| Componentes base (via sdkconfig) | `WIFI_SSID`, `WIFI_PASSWORD`, `UDP_BROADCAST_PORT` | — |

Cada componente define sus opciones en su propio `Kconfig.projbuild`.

---

## 9. Estructura del Repositorio

```
Rastreador_Solar_Inteligente/
├── firmware/                              # ESP-IDF project root
│   ├── CMakeLists.txt                     # project() includes IDF_PATH
│   ├── main/
│   │   ├── CMakeLists.txt                 # Registra main.c
│   │   └── main.c                         # app_main + tracker_task
│   ├── components/
│   │   ├── power_monitor/                 # ADC → battery/solar voltage
│   │   │   ├── CMakeLists.txt
│   │   │   ├── power_monitor.c
│   │   │   └── include/power_monitor.h
│   │   ├── ldr_sensor/                    # 4×LDR → azimuth/elevation error
│   │   │   ├── CMakeLists.txt
│   │   │   ├── ldr_sensor.c
│   │   │   └── include/ldr_sensor.h
│   │   ├── servo_motor/                   # 2×MG90S via LEDC PWM
│   │   │   ├── CMakeLists.txt
│   │   │   ├── servo_motor.c
│   │   │   └── include/servo_motor.h
│   │   ├── udp_logger/                    # WiFi STA + UDP pull + NVS
│   │   │   ├── CMakeLists.txt
│   │   │   ├── udp_logger.c
│   │   │   └── include/udp_logger.h
│   │   ├── web_dashboard/                 # Servidor HTTP embebido
│   │   │   ├── CMakeLists.txt
│   │   │   ├── web_dashboard.c
│   │   │   └── include/web_dashboard.h
│   │   └── telegram_bot/                  # Bot Telegram
│   │       ├── CMakeLists.txt
│   │       ├── telegram_bot.c
│   │       ├── cJSON.c / cJSON.h
│   │       ├── Kconfig.projbuild
│   │       └── include/telegram_bot.h
│   ├── sdkconfig                           # Configuracion compilacion
│   └── build/                              # Build artifacts (gitignored)
├── daemon_pc/
│   ├── monitor.py                          # UDP Client (consulta datos)
│   └── configure_wifi.py                   # Configura WiFi via serial
├── docs/                                   # Documentacion tecnica
│   ├── 00_INDICE_GENERAL.md
│   ├── GUIA_CONFIGURACION_WIFI.md          # WiFi por serial + NVS
│   ├── GUIA_TELEGRAM_BOT.md                # Bot de Telegram
│   ├── GUIA_WEB_DASHBOARD.md               # Dashboard web
│   └── ... (otros documentos)
├── AGENTS.md                               # Guia para agentes AI
├── .gitignore
└── README.md                               # Este archivo
```

---

## 10. Decisiones Técnicas y Mitigaciones

| Decisión | Justificación |
|----------|---------------|
| Sin capacitor 1µF en ADC | Compensado por promedio software de 30 muestras a 50ms |
| Un servo a la vez | Anti-brownout: pico de 2 servos (~1A) supera capacidad MT3608 |
| PWM 13 bits vs 10 bits | Mayor resolución angular (~0.022° por paso vs ~0.176°) |
| Duty = 0 entre movimientos | Ahorro de energía: servo consume 50mA con PWM vs ~5µA sin PWM |
| ADC configurado una vez al boot | Previene timeout en `adc_oneshot_read()` por reconexión concurrente |
| Sin Queues FreeRTOS | Variables globales atomicas suficientes para int/float sin contención cross-core |
| Histéresis de batería 3.5/3.2V | Evita oscilación cerca del umbral de corte |
| Deadzone de error (threshold 20) | Evita micro-movimientos por ruido ADC |

---

## 11. Estado del Hardware

> **⚠️ ATENCIÓN:** El ESP32 original se quemó (Junio 2026).  
> **Causa:** Se conectó batería (MT3608 → VIN) mientras el ESP32 estaba conectado por USB.  
> **Síntoma:** ESP32 se calienta al conectar USB, no responde via CH340.  
> **Solución:** Comprar nuevo ESP32 DevKit (ESP32-WROOM-32, CH340).  
> **Regla:** Nunca conectar VIN y USB al mismo tiempo.

**Recomendación de capacitor:** 470–1000µF 25V electrolítico + 100nF cerámico cerca del pin VIN del ESP32 para filtrar transitorios del MT3608.

---

## 12. Referencias

- **ESP-IDF Documentation:** https://docs.espressif.com/projects/esp-idf/
- **FreeRTOS:** https://www.freertos.org/
- **SG90 Datasheet:** Micro Servo SG90 (500–2500µs, 50Hz, 4.5–6V)
- **MT3608 Datasheet:** Step-Up DC-DC Converter (2–24V in, up to 28V out)
- **TP4056 Datasheet:** 1A Linear Li-ion Charger
- **Supabase:** https://supabase.com/docs
- **Telegram Bot API:** https://core.telegram.org/bots/api
- **docs/GUIA_TELEGRAM_BOT.md** — Guia de configuracion del bot Telegram
- **docs/GUIA_WEB_DASHBOARD.md** — Guia del dashboard web
- **docs/GUIA_CONFIGURACION_WIFI.md** — Guia de configuracion WiFi
