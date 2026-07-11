#include "ldr_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include <stdlib.h>

static const char *TAG = "LDR_SENSOR";

// Pines ADC1 para los LDRs (cableado físico real)
#define ADC_CHAN_LDR_TOP_LEFT     ADC_CHANNEL_3 // GPIO 39
#define ADC_CHAN_LDR_TOP_RIGHT    ADC_CHANNEL_5 // GPIO 33
#define ADC_CHAN_LDR_BOT_LEFT     ADC_CHANNEL_4 // GPIO 32
#define ADC_CHAN_LDR_BOT_RIGHT    ADC_CHANNEL_0 // GPIO 36 (VP)

#define SAMPLE_PERIOD_MS 50
#define DEADZONE_THRESHOLD 0

static adc_oneshot_unit_handle_t adc1_handle = NULL;

// Variables globales para compartir los resultados
static int current_azimut_err = 0;
static int current_elevacion_err = 0;
static int avg_top = 0, avg_bot = 0, avg_left = 0, avg_right = 0;
static int avg_tl = 0, avg_tr = 0, avg_bl = 0, avg_br = 0;

static void ldr_sensor_task(void *pvParameters) {
    while (1) {
        int tl, tr, bl, br;
        
        esp_err_t ret;
        int retries;
        retries = 3;
        do {
            ret = adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_TOP_LEFT, &tl);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "TL ADC timeout, retrying...");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } while (ret != ESP_OK && retries-- > 0);
        if (ret != ESP_OK) { ESP_LOGE(TAG,"TL ADC failed"); tl = 0; }
        retries = 3;
        do {
            ret = adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_TOP_RIGHT, &tr);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "TR ADC timeout, retrying...");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } while (ret != ESP_OK && retries-- > 0);
        if (ret != ESP_OK) { ESP_LOGE(TAG,"TR ADC failed"); tr = 0; }
        retries = 3;
        do {
            ret = adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_BOT_LEFT, &bl);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "BL ADC timeout, retrying...");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } while (ret != ESP_OK && retries-- > 0);
        if (ret != ESP_OK) { ESP_LOGE(TAG,"BL ADC failed"); bl = 0; }
        retries = 3;
        do {
            ret = adc_oneshot_read(adc1_handle, ADC_CHAN_LDR_BOT_RIGHT, &br);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "BR ADC timeout, retrying...");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } while (ret != ESP_OK && retries-- > 0);
        if (ret != ESP_OK) { ESP_LOGE(TAG,"BR ADC failed"); br = 0; }
        
        // Filtro de media móvil exponencial 50/50 (responde en ~2 lecturas)
        avg_tl = (avg_tl + tl) / 2;
        avg_tr = (avg_tr + tr) / 2;
        avg_bl = (avg_bl + bl) / 2;
        avg_br = (avg_br + br) / 2;
        avg_top   = (avg_top + (tl + tr) / 2) / 2;
        avg_bot   = (avg_bot + (bl + br) / 2) / 2;
        avg_left  = (avg_left + (tl + bl) / 2) / 2;
        avg_right = (avg_right + (tr + br) / 2) / 2;
        
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

void ldr_sensor_get_individual(int *tl, int *tr, int *bl, int *br) {
    if (tl) *tl = avg_tl;
    if (tr) *tr = avg_tr;
    if (bl) *bl = avg_bl;
    if (br) *br = avg_br;
}
