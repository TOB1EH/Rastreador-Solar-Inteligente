#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "ldr_sensor.h"
#include "servo_motor.h"

static const char *TAG = "SOLAR_TRACKER";

#define TRACKER_PERIOD_MS       100
#define BROWNOUT_VOLTAGE_THRESH 6.0f
#define ANGLE_STEP               2
#define ANGLE_MIN                 0
#define ANGLE_MAX               180

static int current_az = 90;
static int current_el = 90;

static void clamp_angle(int *angle) {
    if (*angle < ANGLE_MIN) *angle = ANGLE_MIN;
    if (*angle > ANGLE_MAX) *angle = ANGLE_MAX;
}

static void solar_tracker_task(void *pvParameters) {
    ESP_LOGI(TAG, "Tracker task started");
    while (1) {
        int az_err = 0, el_err = 0;
        ldr_sensor_get_errors(&az_err, &el_err);

        float vbat = power_monitor_get_battery_voltage();
        bool low_battery = (vbat < BROWNOUT_VOLTAGE_THRESH);

        if (az_err != 0 && !low_battery) {
            current_az += (az_err > 0) ? -ANGLE_STEP : ANGLE_STEP;
            clamp_angle(&current_az);
            servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
        }

        if (el_err != 0 && !low_battery) {
            current_el += (el_err > 0) ? -ANGLE_STEP : ANGLE_STEP;
            clamp_angle(&current_el);
            servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
        }

        if (az_err != 0 || el_err != 0) {
            ESP_LOGI(TAG, "az=%d el=%d | az_err=%d el_err=%d | bat=%.2fV",
                     current_az, current_el, az_err, el_err, vbat);
        }

        vTaskDelay(pdMS_TO_TICKS(TRACKER_PERIOD_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Rastreador Solar Inteligente - Iniciando...");
    power_monitor_init();
    adc_oneshot_unit_handle_t adc = power_monitor_get_adc_handle();
    ldr_sensor_init(adc);
    servo_motor_init();

    xTaskCreatePinnedToCore(solar_tracker_task, "tracker", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Sistema listo");
}
