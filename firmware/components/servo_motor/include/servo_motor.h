#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H
#ifdef __cplusplus
extern "C" {
#endif
// Definición del eje (solamente azimut)
typedef enum {
    SERVO_AXIS_AZIMUT = 0
} servo_axis_t;
/**
 * @brief Inicializa el hardware PWM (LEDC) para los servos
 */
void servo_motor_init(void);
/**
 * @brief Mueve un servo a un ángulo específico
 * 
 * @param axis Eje a mover (SERVO_AXIS_AZIMUT)
 * @param angle_degrees Ángulo de 0 a 180 grados
 */
void servo_motor_set_angle(servo_axis_t axis, int angle_degrees);
#ifdef __cplusplus
}
#endif
#endif // SERVO_MOTOR_H