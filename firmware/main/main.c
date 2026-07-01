#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "servo_motor.h"
#include "udp_logger.h"
#include "ldr_sensor.h"

static const char *TAG = "SOLAR_TRACKER";

#define BATTERY_MIN_MOVE_V 3.4f
#define TRACK_INTERVAL_MS 50
#define ERR_THRESHOLD 20

#define AZIMUTH_MIN 0
#define AZIMUTH_MAX 180
#define ELEVATION_MIN 95
#define ELEVATION_MAX 170

static int current_azimuth = 90;
static int current_elevation = 135;

static int proportional_step(int err, int max_step, int divisor) {
    int abs_err = abs(err);
    if (abs_err < ERR_THRESHOLD) return 0;
    int step = 1 + abs_err / divisor;
    if (step > max_step) step = max_step;
    return step;
}

void tracker_task(void *pv) {
    bool move_azimuth = true;

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        int bat_ok = (v_bat >= BATTERY_MIN_MOVE_V) || (v_bat < 1.0f);

        if (bat_ok) {
            int tl, tr, bl, br;
            ldr_sensor_get_individual(&tl, &tr, &bl, &br);

            if (move_azimuth) {
                int err_x = (tl * 2 + bl) - (tr * 2 + br);
                int step = proportional_step(err_x, 5, 150);
                if (step > 0) {
                    if (err_x > 0) current_azimuth -= step;
                    else current_azimuth += step;
                }

                if (current_azimuth < AZIMUTH_MIN) current_azimuth = AZIMUTH_MIN;
                if (current_azimuth > AZIMUTH_MAX) current_azimuth = AZIMUTH_MAX;

                servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_azimuth);
            } else {
                int err_y = (tl + tr) * 2 - (bl + br);
                int step = proportional_step(err_y, 3, 150);
                if (step > 0) {
                    if (err_y > 0) current_elevation -= step;
                    else current_elevation += step;
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
        int tl, tr, bl, br;
        ldr_sensor_get_individual(&tl, &tr, &bl, &br);

        ESP_LOGI(TAG, "Bat:%.2fV Panel:%.2fV | Az:%3d° El:%3d° | TL:%d TR:%d BL:%d BR:%d",
                 v_bat, v_sol, current_azimuth, current_elevation,
                 tl, tr, bl, br);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
