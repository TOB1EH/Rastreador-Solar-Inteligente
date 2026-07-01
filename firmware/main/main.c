#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "servo_motor.h"
#include "udp_logger.h"
#include "ldr_sensor.h"

static const char *TAG = "SOLAR_TRACKER";

#define BATTERY_MIN_MOVE_V 3.4f
#define ANGLE_STEP 3
#define TRACK_INTERVAL_MS 50
#define HYST_START 350
#define HYST_STOP 150

static int current_azimuth = 90;
static int moving_dir = 0;

void tracker_task(void *pv) {
    ESP_LOGI(TAG, "Iniciando tracker por LDR (solo azimut)...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        int az_err, el_err;
        ldr_sensor_get_errors(&az_err, &el_err);

        if (v_bat >= BATTERY_MIN_MOVE_V) {
            int prev = current_azimuth;

            if (moving_dir == 0) {
                if (az_err > HYST_START) moving_dir = -1;
                else if (az_err < -HYST_START) moving_dir = 1;
            } else if (moving_dir == -1) {
                if (az_err <= HYST_STOP) moving_dir = 0;
                else current_azimuth -= ANGLE_STEP;
            } else if (moving_dir == 1) {
                if (az_err >= -HYST_STOP) moving_dir = 0;
                else current_azimuth += ANGLE_STEP;
            }

            if (current_azimuth < 0) { current_azimuth = 0; moving_dir = 0; }
            if (current_azimuth > 180) { current_azimuth = 180; moving_dir = 0; }

            if (current_azimuth != prev) {
                servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_azimuth);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TRACK_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== RASTREADOR SOLAR INTELIGENTE (tracking LDR) ===");

    power_monitor_init();
    servo_motor_init();
    ldr_sensor_init(power_monitor_get_adc_handle());
    udp_logger_init();
    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(tracker_task, "tracker", 2048, NULL, 5, NULL, 1);

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        int az_err, el_err;
        ldr_sensor_get_errors(&az_err, &el_err);
        ESP_LOGI(TAG, "Az=%d | az_err=%d el_err=%d | Bat=%.2fV Sol=%.2fV",
                 current_azimuth, az_err, el_err, v_bat, v_sol);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
