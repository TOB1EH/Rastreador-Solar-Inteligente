#include "ldr_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include <stdlib.h>

static const char *TAG = "LDR_SENSOR";

// Pines ADC1 para los LDRs
#define ADC_CHAN_LDR_TOP_LEFT     ADC_CHANNEL_4 // GPIO 32
#define ADC_CHAN_LDR_TOP_RIGHT    ADC_CHANNEL_5 // GPIO 33
#define ADC_CHAN_LDR_BOT_LEFT     ADC_CHANNEL_0 // GPIO 36 (VP)
#define ADC_CHAN_LDR_BOT_RIGHT    ADC_CHANNEL_3 // GPIO 39 (VN)
#define SAMPLE_PERIOD_MS 50
#define DEADZONE_THRESHOLD 200 // Umbral de tolerancia (~5% de 4095)

static adc_oneshot_unit_handle_t adc1_handle = NULL;

// Variables globales para compartir los resultados
static int current_azimut_err = 0;
static int current_elevacion_err = 0;
static int avg_top = 0, avg_bot = 0, avg_left = 0, avg_right = 0;

static void ldr_sensor_task(void *pvParameters) {
    while (1) {
        int tl, tr, bl, br;
        
        // Leer los 4 sensores
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_TOP_LEFT, &tl));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_TOP_RIGHT, &tr));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_BOT_LEFT, &bl));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_BOT_RIGHT, &br));
        
        // Filtro pasabajos súper simple (Complementario) para suavizar
        avg_top   = (avg_top * 4 + (tl + tr) / 2) / 5;
        avg_bot   = (avg_bot * 4 + (bl + br) / 2) / 5;
        avg_left  = (avg_left * 4 + (tl + bl) / 2) / 5;
        avg_right = (avg_right * 4 + (tr + br) / 2) / 5;
        
        // Calcular errores
        int err_y = avg_top - avg_bot;     // Elevación
        int err_x = avg_left - avg_right;  // Azimut
        
        // Aplicar Zona Muerta (Threshold)
        if (abs(err_y) < DEADZONE_THRESHOLD) err_y = 0;
        if (abs(err_x) < DEADZONE_THRESHOLD) err_x = 0;
        
        // Actualizar variables globales
        current_elevacion_err = err_y;
        current_azimut_err = err_x;
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

void ldr_sensor_init(adc_oneshot_unit_handle_t adc_handle) {
    ESP_LOGI(TAG, "Inicializando LDR Sensors...");
    adc1_handle = adc_handle;
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_LDR_TOP_LEFT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_LDR_TOP_RIGHT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_LDR_BOT_LEFT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_LDR_BOT_RIGHT, &config));
    xTaskCreatePinnedToCore(ldr_sensor_task, "ldr_task", 4096, NULL, 5, NULL, 1);
}

void ldr_sensor_get_errors(int *azimut_err, int *elevacion_err) {
    if (azimut_err) *azimut_err = current_azimut_err;
    if (elevacion_err) *elevacion_err = current_elevacion_err;
}

void ldr_sensor_get_raw_values(int *top, int *bottom, int *left, int *right) {
    if (top) *top = avg_top;
    if (bottom) *bottom = avg_bot;
    if (left) *left = avg_left;
    if (right) *right = avg_right;
}