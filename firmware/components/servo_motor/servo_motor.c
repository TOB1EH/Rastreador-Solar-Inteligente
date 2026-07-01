#include "servo_motor.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "SERVO_MOTOR";

// --- Configuración de los Pines ---
#define SERVO_PIN_AZIMUT      18 // GPIO 18
#define SERVO_PIN_ELEVATION   19 // GPIO 19

// --- Configuración de LEDC ---
#define SERVO_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_TIMER     LEDC_TIMER_0
#define SERVO_LEDC_RESOLUTION LEDC_TIMER_13_BIT // 13 bits = 8192 pasos (0 a 8191)
#define SERVO_LEDC_FREQ_HZ   50 // Los servos SG90 funcionan a 50Hz (20ms periodo)

// --- Calibración del Ancho de Pulso para SG90 (en microsegundos) ---
#define SERVO_MIN_PULSEWIDTH_US 500  // Normalmente 0 grados
#define SERVO_MAX_PULSEWIDTH_US 2500 // Normalmente 180 grados

// Función matemática interna para calcular el Duty Cycle
static uint32_t angle_to_duty(int angle) {
    // 1. Limitar el ángulo entre 0 y 180 para evitar dañar el servo
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    // 2. Mapear de grados (0-180) a microsegundos (500-2500)
    uint32_t pulsewidth_us = SERVO_MIN_PULSEWIDTH_US + 
        (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / 180);
    // 3. Convertir microsegundos a valor de Duty Cycle (resolución de 13 bits a 50Hz)
    // Formula: (pulsewidth_us * (2^13)) / (1.000.000 / 50Hz)
    // 1.000.000 us / 50 Hz = 20.000 us por periodo
    uint32_t duty = (pulsewidth_us * (1 << 13)) / 20000;
    
    return duty;
}

void servo_motor_init(void) {
    ESP_LOGI(TAG, "Inicializando PWM para Servomotores...");
    // 1. Configurar el Timer del LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_RESOLUTION,
        .freq_hz          = SERVO_LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    // 2. Configurar el Canal para el Azimut (X)
    ledc_channel_config_t ledc_channel_azimut = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN_AZIMUT,
        .duty           = angle_to_duty(90), // Iniciar en el centro (90°)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_azimut));

    ledc_channel_config_t ledc_channel_elevation = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN_ELEVATION,
        .duty           = angle_to_duty(90),
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_elevation));
    ledc_fade_func_install(0);
}

void servo_motor_set_angle(servo_axis_t axis, int angle_degrees) {
    uint32_t duty = angle_to_duty(angle_degrees);
    ledc_channel_t ch = (axis == SERVO_AXIS_AZIMUT) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;

    ESP_ERROR_CHECK(ledc_set_fade_with_time(SERVO_LEDC_MODE, ch, duty, 40));
    ESP_ERROR_CHECK(ledc_fade_start(SERVO_LEDC_MODE, ch, LEDC_FADE_NO_WAIT));
    
    // ESP_LOGD(TAG, "Servo %d movido a %d grados (Duty: %lu)", axis, angle_degrees, duty);
}