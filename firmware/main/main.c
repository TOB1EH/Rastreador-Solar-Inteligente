#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "power_monitor.h"
#include "servo_motor.h"
#include "udp_logger.h"
#include "ldr_sensor.h"

static const char *TAG = "SOLAR_TRACKER";

#define BATTERY_MOVE_ON  3.5f
#define BATTERY_MOVE_OFF 3.2f
#define TRACK_INTERVAL_MS 100
#define ERR_THRESHOLD 20

#define AZIMUTH_MIN 0
#define AZIMUTH_MAX 180
#define ELEVATION_MIN 90
#define ELEVATION_MAX 175

static int current_azimuth = 90;
static int current_elevation = 135;
static bool bat_ok = false;

static int proportional_step(int err, int max_step, int divisor) {
    int abs_err = abs(err);
    if (abs_err < ERR_THRESHOLD) return 0;
    int step = 1 + abs_err / divisor;
    if (step > max_step) step = max_step;
    return step;
}

void tracker_task(void *pv) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();

        if (v_bat < 1.0f) {
            bat_ok = true;
        } else if (!bat_ok && v_bat >= BATTERY_MOVE_ON) {
            bat_ok = true;
            ESP_LOGI(TAG, "Tracking ON (%.2fV)", v_bat);
        } else if (bat_ok && v_bat < BATTERY_MOVE_OFF) {
            bat_ok = false;
            ESP_LOGW(TAG, "Tracking OFF (%.2fV)", v_bat);
        }

        if (bat_ok) {
            int err_x, err_y;
            ldr_sensor_get_errors(&err_x, &err_y);

            int az_step = proportional_step(err_x, 8, 80);
            if (az_step > 0) {
                int new_az = current_azimuth;
                if (err_x > 0) new_az -= az_step;
                else new_az += az_step;
                if (new_az < AZIMUTH_MIN) new_az = AZIMUTH_MIN;
                if (new_az > AZIMUTH_MAX) new_az = AZIMUTH_MAX;
                if (new_az != current_azimuth) {
                    current_azimuth = new_az;
                    servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_azimuth);
                }
            }

            int el_step = proportional_step(err_y, 5, 60);
            if (el_step > 0) {
                int new_el = current_elevation;
                if (err_y > 0) new_el -= el_step;
                else new_el += el_step;
                if (new_el < ELEVATION_MIN) new_el = ELEVATION_MIN;
                if (new_el > ELEVATION_MAX) new_el = ELEVATION_MAX;
                if (new_el != current_elevation) {
                    current_elevation = new_el;
                    servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_elevation);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TRACK_INTERVAL_MS));
    }
}

void app_main(void) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    ESP_LOGI(TAG, "=== RASTREADOR SOLAR INTELIGENTE ===");

    power_monitor_init();
    servo_motor_init();
    ldr_sensor_init(power_monitor_get_adc_handle());
    udp_logger_init();

    servo_motor_set_angle_blocking(SERVO_AXIS_AZIMUT, current_azimuth);
    servo_motor_set_angle_blocking(SERVO_AXIS_ELEVATION, current_elevation);

    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(tracker_task, "tracker", 2048, NULL, 5, NULL, 1);

    while (1) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        int err_x, err_y;
        ldr_sensor_get_errors(&err_x, &err_y);
        int tl, tr, bl, br;
        ldr_sensor_get_individual(&tl, &tr, &bl, &br);
        ESP_LOGI(TAG, "Bat:%.2fV Panel:%.2fV | Az:%3d° El:%3d° | err_x:%-5d err_y:%-5d | TL:%d TR:%d BL:%d BR:%d",
                 v_bat, v_sol, current_azimuth, current_elevation,
                 err_x, err_y, tl, tr, bl, br);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
