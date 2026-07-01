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
#define AZIMUTH_STEP 3
#define ELEVATION_STEP 1
#define TRACK_INTERVAL_MS 50
#define ERR_THRESHOLD 30

#define AZIMUTH_MIN 0
#define AZIMUTH_MAX 180
#define ELEVATION_MIN 95
#define ELEVATION_MAX 170

static int current_azimuth = 90;
static int current_elevation = 135;

void tracker_task(void *pv) {
    bool move_azimuth = true;

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        int bat_ok = (v_bat >= BATTERY_MIN_MOVE_V) || (v_bat < 1.0f);

        if (bat_ok) {
            int err_x, err_y;
            ldr_sensor_get_errors(&err_x, &err_y);

            if (move_azimuth) {
                if (err_x > ERR_THRESHOLD) {
                    current_azimuth -= AZIMUTH_STEP;
                } else if (err_x < -ERR_THRESHOLD) {
                    current_azimuth += AZIMUTH_STEP;
                }

                if (current_azimuth < AZIMUTH_MIN) current_azimuth = AZIMUTH_MIN;
                if (current_azimuth > AZIMUTH_MAX) current_azimuth = AZIMUTH_MAX;

                servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_azimuth);
            } else {
                if (err_y > ERR_THRESHOLD) {
                    current_elevation += ELEVATION_STEP;
                } else if (err_y < -ERR_THRESHOLD) {
                    current_elevation -= ELEVATION_STEP;
                }

                if (current_elevation < ELEVATION_MIN) current_elevation = ELEVATION_MIN;
                if (current_elevation > ELEVATION_MAX) current_elevation = ELEVATION_MAX;

                servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_elevation);
            }

            move_azimuth = !move_azimuth;
        }

        vTaskDelay(pdMS_TO_TICKS(TRACK_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== RASTREADOR SOLAR INTELIGENTE ===");

    power_monitor_init();
    servo_motor_init();
    ldr_sensor_init(power_monitor_get_adc_handle());
    udp_logger_init();

    servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_azimuth);
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_elevation);

    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(tracker_task, "tracker", 2048, NULL, 5, NULL, 1);

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        int err_x, err_y;
        ldr_sensor_get_errors(&err_x, &err_y);
        ESP_LOGI(TAG, "Az=%d El=%d | err_x=%-5d err_y=%-5d | Bat=%.2fV Sol=%.2fV",
                 current_azimuth, current_elevation,
                 err_x, err_y, v_bat, v_sol);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
