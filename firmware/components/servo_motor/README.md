# servo_motor

Control de dos servos SG90 mediante LEDC PWM (50Hz, 13-bit resolution).

## API

```c
void servo_motor_init(void);
```
Configura LEDC timer y dos canales (GPIO18 azimut, GPIO19 elevación).

```c
void servo_motor_set_angle(servo_axis_t axis, int angle_degrees);
```
Mueve el servo a `angle_degrees` (0-180°) con fade suave de ~40ms (non-blocking).

```c
void servo_motor_set_angle_blocking(servo_axis_t axis, int angle_degrees);
```
Igual que `set_angle` pero bloqueante (espera a que termine el fade).

```c
void servo_motor_wake_and_move(servo_axis_t axis, int angle_degrees);
```
Activa PWM, mueve, y espera 100ms (modo ahorro de energía).

```c
void servo_motor_sleep(servo_axis_t axis);
```
Pone duty=0 para desactivar el servo y ahorrar batería.

## Tipos

```c
typedef enum {
    SERVO_AXIS_AZIMUT = 0,     // GPIO18, rotación horizontal (izquierda/derecha)
    SERVO_AXIS_ELEVATION = 1   // GPIO19, inclinación vertical (arriba/abajo)
} servo_axis_t;
```

## Dependencias

- `esp_driver_ledc`

## Parámetros PWM

| Parámetro | Valor |
|-----------|-------|
| Frecuencia | 50 Hz |
| Resolución | 13 bits |
| Pulse mínimo | 500 µs (0°) |
| Pulse máximo | 2500 µs (180°) |

## Rango operativo en tracking

| Eje      | Mínimo | Máximo |
|----------|--------|--------|
| Azimut   | 0°     | 180°   |
| Elevación | 90°   | 175°   |
