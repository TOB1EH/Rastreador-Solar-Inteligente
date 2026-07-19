# Guía de Conceptos para la Exposición — Rastreador Solar Inteligente

**Autores:** Brambilla Agustín Eduardo, Funes Tobias  
**Materia:** Sistemas de Tiempo Real — STR 2026  
**Profesor:** Storaccio Luis  
**Propósito:** Explicar todos los conceptos técnicos del proyecto para la defensa oral

---

## Índice de Conceptos

1. [Sistemas de Tiempo Real — ¿Qué significa?](#1-sistemas-de-tiempo-real)
2. [FreeRTOS — Tareas, Prioridades, Scheduling](#2-freertos)
3. [ESP32 — Arquitectura del Microcontrolador](#3-esp32)
4. [ADC — Conversión Analógico-Digital](#4-adc)
5. [Divisor de Tensión](#5-divisor-de-tensión)
6. [PWM con LEDC — Control de Servos](#6-pwm-ledc)
7. [Servomotor SG90](#7-servomotor-sg90)
8. [LDR — Sensor de Luz](#8-ldr)
9. [Filtros por Software — Media Móvil y Exponencial](#9-filtros-software)
10. [Control Proporcional y Zona Muerta](#10-control-proporcional)
11. [Histéresis de Batería](#11-histéresis)
12. [Comunicación UDP — Modo Pull](#12-udp)
13. [WiFi en el ESP32](#13-wifi)
14. [Sistema de Alimentación — TP4056 + MT3608](#14-alimentación)
15. [Brownout — Qué es y Cómo lo Evitamos](#15-brownout)
16. [Preguntas Frecuentes de Profesores](#16-preguntas-frecuentes)

---

## 1. Sistemas de Tiempo Real

### ¿Qué es un Sistema de Tiempo Real?

Es un sistema computacional donde **la corrección del resultado no solo depende del valor lógico, sino también del instante en que se produce**. Si una tarea llega tarde, el resultado es incorrecto aunque el cálculo sea correcto.

### Tipos de Sistemas de Tiempo Real

| Tipo | Descripción | Ejemplo |
|------|-------------|---------|
| **Hard (rígido)** | Incumplir un deadline causa catástrofe | Airbag, frenos ABS |
| **Soft (flexible)** | Incumplir degrada performance pero no es catastrófico | **Nuestro tracker** |

### ¿Por qué nuestro proyecto es un STR Soft?

- Si el tracker tarda 600ms en vez de 500ms en responder, el panel sigue apuntando cerca del sol → pérdida marginal de eficiencia, no un desastre.
- El sol se mueve ~0.25°/minuto. Una latencia de 500ms implica un error de ~0.002° → imperceptible.

### Características de STR en nuestro sistema:

- **Multitarea:** 5 tareas concurrentes (tracker, power_monitor, ldr_sensor, udp_server, main loop)
- **Planificación (scheduling):** FreeRTOS usa Fixed-Priority Preemptive Scheduling
- **Sincronización:** Variables compartidas protegidas por diseño (escrituras atómicas)
- **Determinismo:** Las tareas se ejecutan con períodos fijos (50ms, 100ms, 2000ms)

---

## 2. FreeRTOS

### ¿Qué es FreeRTOS?

Es un sistema operativo de tiempo real (RTOS) liviano, gratuito y open-source, diseñado para microcontroladores. Es el RTOS estándar en el mundo embedded.

### Conceptos Clave

#### Tareas (Tasks)

Una tarea es un **hilo de ejecución independiente** con su propio stack y prioridad. En FreeRTOS, cada tarea es un loop infinito.

```c
void mi_tarea(void *pvParameters) {
    while (1) {
        // hacer algo
        vTaskDelay(pdMS_TO_TICKS(100)); // esperar 100ms
    }
}
```

#### Prioridades

- Número más alto = mayor prioridad.
- El scheduler siempre ejecuta la tarea de mayor prioridad que esté lista (Ready).
- Prioridades en nuestro sistema: **5** (tracker, power_monitor, ldr_sensor, udp_server), **1** (main loop).

#### Scheduling (Planificación)

FreeRTUS implementa **Fixed-Priority Preemptive Scheduling**:

1. **Fixed-Priority:** La prioridad de cada tarea se asigna al crear la tarea y NO cambia.
2. **Preemptive:** Si una tarea de mayor prioridad se desbloquea, el scheduler interrumpe (desaloja) la tarea de menor prioridad y ejecuta la de mayor prioridad.

```
Ejemplo:
t=0ms:  main loop ejecutándose (prioridad 1)
t=50ms: ldr_sensor se desbloquea (prioridad 5) → desaloja a main → ejecuta ldr_sensor
t=50ms+: ldr_sensor termina su ciclo → vTaskDelay → main loop se reanuda
```

#### Estados de una Tarea

```
Ready ─→ Running ─→ Blocked (vTaskDelay, esperando recurso)
  ↑                      │
  └──────────────────────┘ (cuando expira el delay)
```

- **Ready:** Lista para ejecutar, esperando que el scheduler la seleccione.
- **Running:** Ejecutándose activamente en la CPU.
- **Blocked:** Esperando un evento (delay, queue, semáforo). **NO consume CPU**.

#### vTaskDelay vs vTaskDelayUntil

- `vTaskDelay(x)`: Espera **al menos** x ticks desde ahora. No es preciso porque no considera el tiempo que estuvo ejecutándose.
- `vTaskDelayUntil(&prev, x)`: Espera **exactamente** x ticks desde la última activación. Útil para tareas periódicas precisas.

Nosotros usamos `vTaskDelay` simple porque la precisión de 50ms no es crítica.

#### xTaskCreatePinnedToCore

Específico de ESP-IDF. Permite anclar una tarea a un núcleo específico:

```c
xTaskCreatePinnedToCore(tracker_task, "tracker", 2048, NULL, 5, NULL, 1);
//                                                                     ↑
//                                                              Core 1 (PRO_CPU)
```

#### Stack de la Tarea

Memoria reservada para variables locales y llamadas a función. En nuestro proyecto:
- `tracker_task`: 2048 bytes
- `power_monitor_task`: 4096 bytes
- `ldr_sensor_task`: 4096 bytes
- `udp_logger_server_task`: 4096 bytes

Si el stack es muy chico → desbordamiento (stack overflow) → crash. Si es muy grande → desperdicio de RAM.

---

## 3. ESP32

### Arquitectura General

| Característica | Valor |
|---------------|-------|
| CPU | Xtensa LX6 dual-core a 240MHz |
| RAM | 520KB SRAM |
| Flash | 4MB |
| ADC | 2 unidades (ADC1, ADC2), 12-bit, 18 canales total |
| WiFi | 802.11 b/g/n integrado |
| Bluetooth | BLE 4.2 |
| PWM | LEDC (8 canales, hasta 20 bits de resolución) |

### Dos Núcleos (Dual-Core)

El ESP32 tiene dos núcleos: **Core 0 (PRO_CPU)** y **Core 1 (APP_CPU)**.

- **Core 0:** Tradicionalmente usado para tareas del sistema (WiFi stack, Bluetooth).
- **Core 1:** Tradicionalmente para tareas de usuario.

En nuestro proyecto:
- **Core 1:** tracker_task, power_monitor_task, ldr_sensor_task (tareas pesadas y de sensado)
- **Core 0:** main loop log, tareas IDF internas

Ventaja: Las tareas de sensado no compiten por CPU con el loop de log.

### ADC (más detalle en sección 4)

- ADC1: 8 canales, usable con WiFi activo (por eso lo usamos).
- ADC2: 8 canales, **no usable con WiFi activo** (el WiFi toma control del ADC2).

### LEDC PWM (más detalle en sección 6)

Periférico de hardware para generar PWM. Tiene 8 canales, 4 timers, hasta 20 bits de resolución.

---

## 4. ADC — Conversión Analógico-Digital

### ¿Qué hace un ADC?

Convierte un **voltaje analógico** (continuo) en un **valor digital** (discreto). El ADC del ESP32 es de **12 bits**, lo que significa:

- Resolución: 2^12 = 4096 valores posibles
- Rango de entrada: 0V a ~3.1V (con atenuación de 12dB)
- Cada paso digital vale: 3.1V / 4096 ≈ **0.76 mV**

### Atenuación (ADC_ATTEN)

El ESP32 puede medir distintos rangos según la atenuación:

| Atenuación | Rango Medible |
|------------|---------------|
| 0dB | 0–1.1V |
| 2.5dB | 0–1.5V |
| 6dB | 0–2.2V |
| **12dB** | **0–3.1V** ← Usamos esta |

Usamos 12dB porque necesitamos medir hasta ~3.1V (el divisor de tensión nos da máximo ~1.67V a la entrada del pin, pero la calibración se hace para 12dB).

### Canales ADC1 en nuestro proyecto

| Canal | GPIO | Señal |
|-------|------|-------|
| CH0 | GPIO36 | LDR BR (Bottom Right) |
| CH3 | GPIO39 | LDR TL (Top Left) |
| CH4 | GPIO32 | LDR BL (Bottom Left) |
| CH5 | GPIO33 | LDR TR (Top Right) |
| CH6 | GPIO34 | Batería (via divisor ×3) |
| CH7 | GPIO35 | Panel solar (via divisor ×3) |

### Calibración del ADC

El ADC del ESP32 no es lineal perfecto. Para mejor precisión usamos **curve-fitting** (ajuste de curva):

```c
adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
// Convierte raw ADC → milivoltios reales usando la curva de calibración del chip
```

Si la calibración no está disponible, usamos fallback teórico:
```c
mV = (raw * 3300) / 4095;  // Linealización simple
```

### ¿Por qué configuramos todos los canales juntos al inicio?

Si cada componente (`power_monitor` y `ldr_sensor`) configurara sus canales por separado, podrían pisarse. Por eso `power_monitor_init()` configura TODOS los canales (2 de power + 4 LDR) de una sola vez.

---

## 5. Divisor de Tensión

### ¿Para qué sirve?

El ESP32 solo puede medir hasta ~3.1V en sus pines ADC. Nuestra batería Li-ion mide 3.7–4.2V y el panel solar puede dar hasta 6V. Necesitamos **reducir** esos voltajes para que entren en el rango del ADC.

### Circuito

```
Vin (batería/panel)
     │
     R1 = 20kΩ
     │
     ├───→ GPIO34/35 (ADC)
     │
     R2 = 10kΩ
     │
    GND
```

### Fórmula

```
Vout = Vin × R2 / (R1 + R2)
Vout = Vin × 10k / (20k + 10k)
Vout = Vin × 10k / 30k
Vout = Vin / 3

⇒ Vin = Vout × 3
```

**Factor del divisor = 3.0**

### Ejemplo

- Batería a 4.2V → ADC ve 4.2 / 3 = 1.4V → dentro del rango de 3.1V ✓
- Panel a 6V → ADC ve 6 / 3 = 2.0V → dentro del rango ✓

### En el código

```c
#define VOLTAGE_DIVIDER_FACTOR 3.0f
// ...
battery_voltage_avg = (bat_mv / 1000.0f) * VOLTAGE_DIVIDER_FACTOR;
```

---

## 6. PWM con LEDC

### ¿Qué es PWM?

**Pulse Width Modulation** — Modulación por Ancho de Pulso. Es una técnica que varía el ancho de un pulso cuadrado a frecuencia constante para controlar la potencia entregada a un dispositivo.

```
50Hz = 20ms período

0°:   ┌─┐┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  (500µs ancho)
90°:  ┌───┐┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  (1500µs ancho)
180°: ┌─────┐┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  (2500µs ancho)
```

### Periférico LEDC del ESP32

- 8 canales independientes
- 4 timers (compartibles entre canales)
- Resolución configurable: hasta 20 bits
- Frecuencia configurable
- **Fade hardware:** Transición suave entre valores sin intervención de CPU

### Configuración de nuestro sistema

```c
// Timer: frecuencia 50Hz, resolución 13 bits (8192 pasos)
ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,  // 13 bits = 0 a 8191
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 50,
    .clk_cfg = LEDC_AUTO_CLK
};

// Canal 0 → GPIO18 (Azimut)
// Canal 1 → GPIO19 (Elevación)
```

### Duty Cycle

El **duty cycle** es el porcentaje del período que la señal está en HIGH.

Para servo SG90:
- 0° → pulso de 500µs → duty = (500µs / 20000µs) × 8192 = 205
- 90° → pulso de 1500µs → duty = (1500 / 20000) × 8192 = 614
- 180° → pulso de 2500µs → duty = (2500 / 20000) × 8192 = 1024

### Fade Hardware

```c
ledc_set_fade_with_time(SERVO_LEDC_MODE, ch, duty, 100);  // Transición en 100ms
ledc_fade_start(SERVO_LEDC_MODE, ch, LEDC_FADE_NO_WAIT);  // Sin bloquear
```

El fade lo hace el hardware LEDC sin intervención de la CPU. Podemos iniciar 100 fades que el hardware los ejecuta solo.

---

## 7. Servomotor SG90

### Especificaciones

| Parámetro | Valor |
|-----------|-------|
| Voltaje operación | 4.5–6.0V |
| Corriente reposo | ~10mA |
| Corriente movimiento | ~150–250mA |
| Corriente pico bloqueado | ~500–700mA |
| Torque | 1.8 kg·cm (a 4.8V) |
| Velocidad | 60° en ~120ms |
| Rango angular | 0–180° |
| Señal control | PWM 50Hz, 500–2500µs |

### Cómo se controla

El servo tiene 3 cables:
- **Marrón (Negro):** GND
- **Rojo:** VCC (5V)
- **Naranja (Amarillo):** Señal PWM

Internamente, el servo tiene:
1. Motor DC con engranajes
2. Potenciómetro (realimentación de posición)
3. Circuito de control que compara el pulso PWM recibido con la posición del potenciómetro

### Mapeo Pulso → Ángulo

El fabricante especifica:
- 500µs → 0°
- 1500µs → 90°
- 2500µs → 180°

Pero esto varía entre marcas. Nosotros usamos estos valores y ajustamos mecánicamente.

### ¿Por qué 13 bits de resolución?

Con 13 bits tenemos 8192 pasos. El pulso varía entre 500µs y 2500µs (2000µs de rango). Cada paso representa:

```
2000µs / 8192 = 0.244µs/paso
```

Ángulo por paso: `180° / 8192 = 0.022°`

Esto es mucho más fino que lo que el servo puede resolver mecánicamente (~1–2°), pero el fade hardware se ve más suave.

---

## 8. LDR — Sensor de Luz

### ¿Qué es un LDR?

**Light Dependent Resistor** — Resistor que cambia su resistencia según la luz incidente:
- **Mucha luz** → **baja resistencia** (~200Ω)
- **Poca luz** → **alta resistencia** (~20kΩ o más)

### Circuito Divisor con LDR

```
3.3V
  │
  R LDR
  │
  ├───→ GPIO ADC
  │
  R fijo = 10kΩ
  │
 GND
```

Cuando la luz es intensa, el LDR tiene baja resistencia → Vout tiende a 3.3V (valor ADC alto).
Cuando está oscuro, el LDR tiene alta resistencia → Vout tiende a 0V (valor ADC bajo).

### Configuración de 4 LDR (Cuadrantes)

Colocamos 4 LDR alrededor del panel solar:

```
        TOP_LEFT    TOP_RIGHT
        (GPIO39)    (GPIO33)
             \        /
              \      /
            [ PANEL ]
              /      \
             /        \
        BOT_LEFT    BOT_RIGHT
        (GPIO32)    (GPIO36)
```

### Cálculo de Errores Diferenciales

```c
// Error horizontal (Azimut): izquierda vs derecha
err_x = (TL + BL) / 2 - (TR + BR) / 2;
// Si err_x > 0 → más luz a la izquierda → girar a la izquierda
// Si err_x < 0 → más luz a la derecha → girar a la derecha
// Si err_x ≈ 0 → centrado

// Error vertical (Elevación): arriba vs abajo
err_y = (TL + TR) / 2 - (BL + BR) / 2;
// Si err_y > 0 → más luz arriba → inclinar hacia arriba
// Si err_y < 0 → más luz abajo → inclinar hacia abajo
// Si err_y ≈ 0 → centrado
```

### Ventaja del enfoque diferencial

- No necesitamos un valor absoluto de luz
- Solo importa la **diferencia** entre cuadrantes
- Inmune a cambios globales de iluminación (nubes pasajeras): si todo se oscurece parejo, err_x y err_y siguen siendo 0

---

## 9. Filtros por Software

### ¿Por qué necesitamos filtros?

El ADC tiene ruido:
- Ruido eléctrico del circuito
- Fluctuaciones en la fuente de alimentación
- Variaciones rápidas de luz (sombras, nubes parciales)

### Filtro de Media Móvil (Moving Average) — Power Monitor

Usamos 30 muestras con promedio aritmético:

```c
int bat_raw_buffer[30];
// Cada 50ms: agregar nueva muestra al buffer circular
// Cuando hay 30 muestras: promediar
for (int i = 0; i < 30; i++) {
    suma += bat_raw_buffer[i];
}
promedio = suma / 30;
```

**Efecto:** Responde a los ~1.5 segundos (30 × 50ms). Suaviza mucho el ruido.

### Filtro Exponencial (Exponential Moving Average) — LDR Sensor

```c
avg_tl = (avg_tl + tl) / 2;  // 50/50
```

Equivalente a: `avg = 0.5 × avg_anterior + 0.5 × muestra_nueva`

**Efecto:** Responde más rápido que la media móvil (~2–3 lecturas). El factor 0.5 hace que las últimas 2 lecturas tengan el 75% del peso.

### Comparación

| Filtro | Velocidad | Suavizado | Uso |
|--------|-----------|-----------|-----|
| Media móvil 30 muestras | Lento (~1.5s) | Muy suave | Power monitor (voltajes cambian lento) |
| Exponencial 50/50 | Rápido (~100ms) | Moderado | LDR (necesita responder a cambios de luz) |

---

## 10. Control Proporcional y Zona Muerta

### Control Proporcional

Nuestro algoritmo de tracking no es un simple ON/OFF. Es **proporcional**: a mayor error, mayor paso de corrección.

```c
static int proportional_step(int err, int max_step, int divisor) {
    int abs_err = abs(err);
    if (abs_err < ERR_THRESHOLD) return 0;     // Zona muerta
    int step = 1 + abs_err / divisor;           // Paso = base + proporcional
    if (step > max_step) step = max_step;       // Saturación
    return step;
}
```

**Comportamiento:**

| Error | Cálculo | Paso |
|-------|---------|------|
| 0–19  | —       | 0° (zona muerta) |
| 80    | 1 + 80/80 | 2° |
| 160   | 1 + 160/80 | 3° |
| 400   | 1 + 400/80 | 6° |
| ≥560  | —       | 8° (cap) |

**Divisores distintos:**
- Azimut: divisor = 80, max = 8
- Elevación: divisor = 60, max = 5

¿Por qué diferentes? La elevación tiene un rango menor (95–175° = 80° vs azimut 0–180° = 180°). Se mueve menos agresivamente.

### Zona Muerta (Dead Zone)

```c
#define ERR_THRESHOLD 20
```

Si `abs(err) < 20`, no nos movemos. Esto evita:
- Micro-movimientos constantes por ruido ADC
- Desgaste innecesario del servo
- Consumo de batería por movimientos irrelevantes

### ¿Por qué no usamos PID?

El sol se mueve muy lento (~0.25°/minuto). Un control proporcional simple es suficiente porque:
- No hay inercia significativa (el servo se mueve y queda ahí)
- No necesitamos anticipación (la derivada del error es casi 0)
- El error de estado estacionario es tolerable (el sol se mueve, el error siempre cambia)

---

## 11. Histéresis de Batería

### Problema

El voltaje de una batería Li-ion no es constante:
- Cargada: ~4.2V
- Vacía: ~3.0V
- El ESP32 puede funcionar hasta ~3.0V (regulador interno 3.3V)

Si ponemos un umbral simple (ej: "apagar tracking si < 3.3V"), cuando el tracking se apaga, el voltaje **sube** porque los servos dejan de consumir. Esto causa un ciclo:

```
Tracking ON → voltaje baja → < 3.3V → Tracking OFF → voltaje sube → > 3.3V → Tracking ON → ...
```

### Solución: Histéresis

Usamos dos umbrales diferentes:

```c
#define BATTERY_MOVE_ON  3.5f   // Para ACTIVAR tracking
#define BATTERY_MOVE_OFF 3.2f   // Para DESACTIVAR tracking
```

```
Voltaje
   ↑
4.2V │
     │    ┌──────────────── Tracking ON
3.5V │────┘
     │
3.2V │────┐
     │    └──────────────── Tracking OFF
3.0V │
     └────────────────────────────→ Tiempo
```

- Si el voltaje sube y **cruza 3.5V** → tracking ON
- Si el voltaje baja y **cruza 3.2V** → tracking OFF
- Entre 3.2V y 3.5V: se mantiene el estado anterior

La banda de 0.3V evita la oscilación.

### USB Fallback

```c
if (v_bat < 1.0f) {
    bat_ok = true;  // Asume que está alimentado por USB (sin batería)
}
```

Si el voltaje es < 1.0V, asumimos que no hay batería y el ESP32 está alimentado por USB. En ese caso, tracking siempre activo.

---

## 12. Comunicación UDP — Modo Pull

### ¿Qué es UDP?

**User Datagram Protocol** — Protocolo de comunicación no orientado a conexión. Más simple y rápido que TCP, pero sin garantía de entrega.

### Modo Pull vs Push

- **Push (empuje):** El ESP32 envía datos periódicamente a un servidor. Más común pero consume más energía (necesita mantener conexión).
- **Pull (tirón):** El ESP32 espera pasivamente consultas y responde. Nosotros usamos este.

Ventaja del modo Pull:
- El ESP32 no gasta recursos enviando datos si nadie los está pidiendo
- El PC decide cuándo consultar (cada 1s, 10s, etc.)
- El ESP32 no necesita almacenar datos ni reconectarse si el WiFi se cae

### Protocolo

```
PC                           ESP32
  │                            │
  ├── "GET" ──── UDP :8080 ──→│
  │                            │
  │←── "BAT:4.12|SOL:5.50" ──┤
  │                            │
  (1 segundo)                  │
  ├── "GET" ──── UDP :8080 ──→│
  │                            │
  │←── "BAT:4.11|SOL:5.48" ──┤
```

### Socket UDP Server (en ESP32)

```c
// 1. Crear socket
sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

// 2. Asociar a puerto 8080
bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

// 3. Esperar datagrama (bloqueante)
recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, ...);

// 4. Responder
sendto(sock, tx_buffer, strlen(tx_buffer), 0, ...);
```

### Daemon PC

```python
# Enviar consulta cada 1s
while True:
    sock.sendto(b"GET", (ESP32_IP, UDP_PORT))
    data, addr = sock.recvfrom(1024)
    # "BAT:4.12|SOL:5.50"
    print(f"Batería: {bat} V  |  Panel: {sol} V")
    time.sleep(1)
```

---

## 13. WiFi en el ESP32

### STA Mode (Station)

El ESP32 funciona como **cliente WiFi** (no como Access Point). Se conecta a un router existente.

### Flujo de conexión

```
udp_logger_init()
  ├→ nvs_flash_init()          — Inicializa almacenamiento no volátil
  ├→ esp_netif_init()           — Inicializa interfaz de red
  ├→ esp_event_loop_create()    — Crea loop de eventos
  ├→ esp_wifi_init()            — Inicializa stack WiFi
  ├→ esp_wifi_set_mode(STA)     — Modo cliente
  ├→ esp_wifi_set_config()      — SSID y password
  ├→ esp_wifi_start()           — Conecta
  └→ … espera WIFI_CONNECTED_BIT
```

### Manejo de Eventos

Usamos un **event group** (bitmask) para sincronizar:

```c
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Handler:
// WIFI_EVENT_STA_START → esp_wifi_connect()
// WIFI_EVENT_STA_DISCONNECTED → reconectar
// IP_EVENT_STA_GOT_IP → set WIFI_CONNECTED_BIT

// El servidor UDP espera:
xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, ...);
// No crea el socket hasta tener IP
```

### WiFi Power Saving

```c
esp_wifi_set_ps(WIFI_PS_NONE);  // Sin ahorro de energía
```

Desactivamos el power saving porque introduce latencia impredecible.

---

## 14. Sistema de Alimentación

### Cadena de Potencia

```
Panel Solar (5.5V 100mA)
         │
         ↓
      TP4056 ───────→ Batería Li-ion 3.7V 2200mAh
      (Cargador)          │
                          ↓
                      MT3608 ──────→ VIN ESP32
                    (Step-Up 5V)      ├→ GPIO 3.3V → LDRs
                                      ├→ Servo Azimut (5V)
                                      └→ Servo Elevación (5V)
```

### TP4056 — Cargador de Batería Li-ion

- Carga a corriente constante (1A programable por resistor)
- Detecta fin de carga cuando la batería llega a 4.2V
- Protección: térmica, polaridad inversa
- Indicadores LED: rojo (cargando), azul (cargada)

### MT3608 — Step-Up DC-DC

- Entrada: 2–24V
- Salida ajustable: hasta 28V
- Nosotros: ajustado a 5.0V
- Eficiencia: ~85–90%
- Corriente máxima: ~2A (pero limitado por la batería)

**Riesgo:** Si los dos servos (500mA pico cada uno) + ESP32 (250mA pico) consumen simultáneamente, el pico de ~1.25A puede hacer caer el voltaje de salida del MT3608. Lo mitigamos moviendo un servo a la vez.

---

## 15. Brownout

### ¿Qué es el Brownout?

Es una **caída de tensión** momentánea por debajo del mínimo de operación. En el ESP32, cuando la alimentación cae por debajo de ~2.8V (con el regulador interno), el chip se resetea.

### Causas en nuestro sistema

- Ambos servos moviéndose simultáneamente: ~1A pico
- WiFi transmitiendo + servo moviéndose: ~500mA
- El MT3608 puede no entregar suficiente corriente instantánea

### Mitigaciones Implementadas

1. **Desactivar brownout detector por software:**
   ```c
   WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
   ```
   Esto evita que el ESP32 se resetee ante caídas breves, pero ojo: si la caída es severa, igual se resetea.

2. **Un servo a la vez:** Alternamos movimientos de azimut y elevación (flag `move_azimuth`).

3. **Pausa de 100ms entre movimientos:** Permite que la fuente se recupere.

4. **Sleep del servo:** duty = 0 después de cada movimiento.

5. **Capacitor recomendado:** 470–1000µF + 100nF en VIN.

---

## 16. Preguntas Frecuentes de Profesores

### ¿Por qué no usaron un shield o módulo FZ0430?

El FZ0430 es un módulo listo para medir voltaje (divisor + optoacoplador). Elegimos no usarlo porque:
- Es caro y difícil de conseguir en Argentina
- Ocupa más espacio en la protoboard
- Nosotros ya implementamos el divisor con 3 resistencias de 10kΩ
- El optoacoplador del FZ0430 sirve para aislación galvánica, que no necesitamos porque batería y ESP32 comparten GND

### ¿Por qué no usan colas FreeRTOS (Queues) entre tareas?

Usamos variables globales compartidas con getters. Esto funciona porque:
- Las escrituras de `int` y `float` en ESP32 son **atómicas** (32 bits, alineadas)
- Las tareas no compiten por el mismo recurso: una escribe (ldr_sensor_task, power_monitor_task) y otra lee (tracker_task, main loop)
- El dato no es crítico: una lectura desactualizada por 50ms no tiene consecuencias

Si necesitáramos pasar datos entre tareas de forma segura, usaríamos Queues (como en el TP10 de FreeRTOS).

### ¿Por qué no usan PID?

El sol se mueve a ~0.25°/minuto. Nuestro ciclo de tracking es de 100ms. En ese tiempo el sol apenas se movió ~0.0004°. Un control proporcional simple es más que suficiente. Agregar acción integral (I) o derivativa (D) no mejoraría el tracking y agregaría complejidad innecesaria.

### ¿Por qué la elevación arranca en 135° y no en 90°?

Los límites de elevación son 95–175°. En Argentina (latitud ~33°S), el sol nunca está en el cenit. La posición de "reposo" a 135° apunta al norte geográfico con una inclinación de ~45°, que es el ángulo óptimo promedio.

### ¿Qué pasa si el wifi no está disponible?

El sistema funciona sin WiFi. El tracker sigue moviendo los servos y midiendo voltajes. Solo la funcionalidad de monitoreo remoto (UDP) se pierde. Si quisiéramos almacenar datos localmente, podríamos usar SPIFFS o NVS en el flash del ESP32.

### ¿Cuánto dura la batería?

- Consumo promedio estimado: ~200mA
- Batería: 2200mAh
- Autonomía teórica: 2200 / 200 = **~11 horas**
- En la práctica: ~6–8 horas por pérdidas y picos de consumo

### ¿Qué mejoras se podrían hacer?

1. **Deep Sleep:** Poner el ESP32 en deep sleep entre ciclos de tracking (cada 5s en vez de 100ms). Autonomía pasaría a >24h.
2. **Almacenamiento local:** SPIFFS o NVS como backup de datos si WiFi falla.
3. **Panel más grande:** Para poder cargar la batería mientras el sistema opera.
4. **PID:** Si se necesita mayor precisión (aplicaciones de concentración solar).
5. **Supabase/Telegram:** Integración cloud planificada pero no implementada por falta de tiempo/hardware.

### ¿El sistema cumple con los requisitos de tiempo real?

Sí:
- **Latencia sensor→motor:** ~150ms (50ms LDR + 100ms ciclo tracker) < 500ms ✓
- **Períodos fijos:** 50ms power_monitor, 50ms LDR, 100ms tracker, 2000ms log ✓
- **Determinismo:** FreeRTOS con prioridades fijas garantiza que las tareas críticas (tracker, sensores) siempre tengan CPU ✓
- **Sin inversión de prioridad:** No usamos mutexes ni semáforos, solo variables atómicas ✓

---

## Resumen Rápido para la Exposición (Cheatsheet)

| Concepto | Frase para decir |
|----------|-----------------|
| ¿Qué es? | Rastreador solar dual-eje con ESP32 + FreeRTOS |
| ¿Cómo sigue el sol? | 4 LDR en cuadrantes, error diferencial → servo |
| ¿Por qué FreeRTOS? | Tareas concurrentes: sensado, control, WiFi, log |
| ¿Por qué dos ejes? | +50% eficiencia vs panel fijo |
| ¿Problema principal? | Brownout al mover 2 servos → solución: uno a la vez |
| ¿Comunicación? | UDP Pull: PC consulta, ESP32 responde |
| ¿Filtros ADC? | Media móvil (power) y exponencial (LDR) |
| ¿Control? | Proporcional con zona muerta, no PID |
| ¿Batería? | Li-ion 3.7V → TP4056 → MT3608 5V → ESP32 |
| ¿Histéresis? | ON ≥ 3.5V, OFF ≤ 3.2V, USB fallback < 1.0V |

---

*Documento preparado para la exposición oral — STR 2026*
*Cualquier duda técnica, remitirse al README.md del proyecto o al código fuente.*
