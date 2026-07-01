#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    SERVO_AXIS_AZIMUT = 0,
    SERVO_AXIS_ELEVATION = 1
} servo_axis_t;
/**
 * @brief Inicializa el hardware PWM (LEDC) para los servos
 */
void servo_motor_init(void);
/**
 * @brief Mueve un servo a un ángulo específico
 * 
 * @param axis Eje a mover (SERVO_AXIS_AZIMUT o SERVO_AXIS_ELEVATION)
 * @param angle_degrees Ángulo de 0 a 180 grados
 */
void servo_motor_set_angle(servo_axis_t axis, int angle_degrees);
#ifdef __cplusplus
}
#endif
#endif // SERVO_MOTOR_H