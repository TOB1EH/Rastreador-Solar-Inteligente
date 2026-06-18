# Rastreador Solar Inteligente

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.0.1-blue)](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/)

Rastreador solar autónomo de dos ejes (azimut + elevación) basado en ESP32 con ESP-IDF v6.0.1 y FreeRTOS.

## Arquitectura

```
ESP32
├── power_monitor   → ADC batería/panel (divisor ×3, filtro 30 muestras)
├── ldr_sensor      → 4 LDR → error azimut/elevación (zona muerta ~200)
├── servo_motor     → 2×SG90 vía LEDC PWM (50Hz, 13-bit, 500-2500µs)
└── udp_logger      → WiFi STA + servidor UDP pull (daemon PC consulta)

PC daemon_pc/monitor.py → consulta ESP32 cada 1s, muestra voltajes
```

## Integración LDR ↔ Servos (para el compañero)

### 1. Cómo se conectan los componentes

```c
#include "power_monitor.h"
#include "ldr_sensor.h"
#include "servo_motor.h"
#include "udp_logger.h"

void app_main(void) {
    power_monitor_init();                              // Inicia ADC
    ldr_sensor_init(power_monitor_get_adc_handle());   // Mismo ADC_UNIT_1
    servo_motor_init();                                 // LEDC PWM
    udp_logger_init();                                  // WiFi + NVS
    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
}
```

### 2. API del LDR

| Función | Uso |
|---------|-----|
| `ldr_sensor_get_errors(&az_err, &el_err)` | `az_err > 0` = luz a la izquierda, `< 0` = derecha, `0` = centrado |
| `ldr_sensor_get_raw_values(&t, &b, &l, &r)` | Valores crudos de cada cuadrante |

### 3. API del Servo

| Función | Rango |
|---------|-------|
| `servo_motor_set_angle(SERVO_AXIS_AZIMUT, 0..180)` | Azimut (horizontal) |
| `servo_motor_set_angle(SERVO_AXIS_ELEVATION, 0..180)` | Elevación (vertical) |

### 4. Loop de tracking (tarea separada)

La lógica debe ejecutarse en una **tarea FreeRTOS dedicada** para no bloquear el loop principal:

```c
static int current_az = 90;   // posición actual azimut
static int current_el = 45;   // posición actual elevación

// Factor de conversión: cuántos grados mover por cada unidad de error LDR
// Ajustar según pruebas (empírico)
#define LDR_STEP_DEG 1

void tracker_task(void *pv) {
    // Posición inicial segura
    servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        int az_err, el_err;
        ldr_sensor_get_errors(&az_err, &el_err);
        ESP_LOGI("TRACKER", "Errores → Az: %d, El: %d", az_err, el_err);

        // --- IMPORTANTE: Mover UN servo a la vez ---
        // Esto evita que el consumo pico de los dos servos
        // tire abajo el riel de 5V del MT3608 (brownout).

        if (az_err != 0) {
            if (az_err > 0) current_az -= LDR_STEP_DEG; // izquierda
            else             current_az += LDR_STEP_DEG; // derecha
            if (current_az < 0) current_az = 0;
            if (current_az > 180) current_az = 180;
            servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
            vTaskDelay(pdMS_TO_TICKS(100)); // Pausa para que la fuente se recupere
        }

        if (el_err != 0) {
            if (el_err > 0) current_el += LDR_STEP_DEG; // arriba
            else             current_el -= LDR_STEP_DEG; // abajo
            if (current_el < 0) current_el = 0;
            if (current_el > 90) current_el = 90; // Límite mecánico del panel
            servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Ciclo completo cada ~500ms
    }
}
```

### 5. Instanciar la tarea en `app_main`

```c
xTaskCreatePinnedToCore(tracker_task, "tracker", 4096, NULL, 5, NULL, 1);
```

Y agregar `ldr_sensor` en `main/CMakeLists.txt` (ya está incluido).

### 6. Consideraciones importantes

| Aspecto | Detalle |
|---------|---------|
| **Zona muerta** | `DEADZONE_THRESHOLD = 200` (~5% de 4095). Si el error es menor, no se mueve. |
| **Brownout** | Mover un servo por vez + pausa de 100ms entre ellos. |
| **Límite elevación** | El panel montado NO debe bajar de 0° ni subir de 90° (ajustar `current_el`). |
| **Velocidad** | Ajustar `LDR_STEP_DEG` y los delays para controlar la agresividad del seguimiento. |
| **ADC compartido** | `ldr_sensor_init()` recibe el handle de `power_monitor_get_adc_handle()`. |

### 7. Monitoreo durante pruebas

Mientras el tracker funciona, el daemon en PC sigue recibiendo datos:

```bash
cd daemon_pc && python3 monitor.py
```
