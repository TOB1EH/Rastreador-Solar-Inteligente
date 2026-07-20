# power_monitor

Monitoreo de voltaje de batería y panel solar mediante ADC1 del ESP32.

## API

```c
void power_monitor_init(void);
```
Inicializa ADC1 con 6 canales (2 power + 4 LDR) y crea una tarea FreeRTOS que muestrea cada 50ms con media móvil de 30 muestras.

```c
float power_monitor_get_battery_voltage(void);
```
Retorna voltaje de batería en V (ej: 7.42). Incluye el factor ×3 del divisor resistivo (R1=20kΩ, R2=10kΩ).

```c
float power_monitor_get_solar_voltage(void);
```
Retorna voltaje del panel solar en V.

```c
adc_oneshot_unit_handle_t power_monitor_get_adc_handle(void);
```
Retorna el handle compartido de ADC1 para que `ldr_sensor` lo reutilice.

## Dependencias

- `esp_adc`, `freertos`

## Canales ADC

| Señal     | GPIO | ADC Channel |
|-----------|------|-------------|
| Batería   | GPIO34 | ADC_CHANNEL_6 |
| Panel     | GPIO35 | ADC_CHANNEL_7 |
| LDR TL    | GPIO36 | ADC_CHANNEL_0 |
| LDR TR    | GPIO33 | ADC_CHANNEL_5 |
| LDR BL    | GPIO32 | ADC_CHANNEL_4 |
| LDR BR    | GPIO39 | ADC_CHANNEL_3 |

## Divisor resistivo

3 resistencias de 10kΩ en serie: R1=20kΩ (entre Vmedido y ADC), R2=10kΩ (entre ADC y GND). Factor de división = ×3.

## Notas

- Sin capacitor externo en ADC (el filtro por software de 30 muestras compensa).
- Todos los canales se configuran una sola vez en `power_monitor_init()` para evitar timeouts en lecturas concurrentes.
- Voltajes < 1.0V se consideran "USB conectado sin batería" (fallback para tracking).
