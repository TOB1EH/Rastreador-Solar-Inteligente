#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "ldr_sensor.h"
#include "servo_motor.h"
#include "udp_logger.h"

static const char *TAG = "SOLAR_TRACKER";

// Posiciones actuales de los servos
static int current_az = 90;
static int current_el = 45;

// Cuántos grados mover por cada ciclo de corrección
#define LDR_STEP_DEG 1

// Límites mecánicos
#define AZ_MIN 0
#define AZ_MAX 180
#define EL_MIN 0
#define EL_MAX 90

void tracker_task(void *pv) {
    ESP_LOGI(TAG, "Tracker iniciado. Posición inicial → Az: %d°, El: %d°", current_az, current_el);
    servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        int az_err, el_err;
        ldr_sensor_get_errors(&az_err, &el_err);

        if (az_err != 0 || el_err != 0) {
            ESP_LOGI(TAG, "Errores → az_err:%+4d el_err:%+4d | moviendo...", az_err, el_err);

            // Mover UN servo a la vez (anti-brownout)
            if (az_err != 0) {
                if (az_err > 0) current_az -= LDR_STEP_DEG;
                else            current_az += LDR_STEP_DEG;
                if (current_az < AZ_MIN) current_az = AZ_MIN;
                if (current_az > AZ_MAX) current_az = AZ_MAX;
                servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
                vTaskDelay(pdMS_TO_TICKS(80));
            }

            if (el_err != 0) {
                if (el_err > 0) current_el += LDR_STEP_DEG;
                else            current_el -= LDR_STEP_DEG;
                if (current_el < EL_MIN) current_el = EL_MIN;
                if (current_el > EL_MAX) current_el = EL_MAX;
                servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
                vTaskDelay(pdMS_TO_TICKS(80));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== RASTREADOR SOLAR INTELIGENTE ===");

    power_monitor_init();
    ldr_sensor_init(power_monitor_get_adc_handle());
    servo_motor_init();
    udp_logger_init();
    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(tracker_task, "tracker", 4096, NULL, 5, NULL, 1);

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        ESP_LOGI(TAG, "Monitor: Batería: %.2f V | Panel Solar: %.2f V | Az:%d° El:%d°",
                 v_bat, v_sol, current_az, current_el);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
