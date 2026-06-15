#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "ldr_sensor.h"

static const char *TAG = "LDR_TEST";

void app_main(void) {
    ESP_LOGI(TAG, "=== TEST DE SENSORES LDR ===");
    ESP_LOGI(TAG, "Conecta 4 LDRs en GPIO 32, 33, 36, 39");
    ESP_LOGI(TAG, "Tapa cada uno para verificar que se detectan");

    power_monitor_init();
    adc_oneshot_unit_handle_t adc = power_monitor_get_adc_handle();
    ldr_sensor_init(adc);

    ESP_LOGI(TAG, "Leyendo LDRs cada 500ms...");

    while (1) {
        int top = 0, bottom = 0, left = 0, right = 0;
        int az_err = 0, el_err = 0;

        ldr_sensor_get_raw_values(&top, &bottom, &left, &right);
        ldr_sensor_get_errors(&az_err, &el_err);

        ESP_LOGI(TAG, "T:%4d B:%4d L:%4d R:%4d | az_err:%+4d el_err:%+4d",
                 top, bottom, left, right, az_err, el_err);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
