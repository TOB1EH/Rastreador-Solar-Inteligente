#include "power_monitor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "POWER_MONITOR";

// Definición de Pines. Usamos ADC1 que no interfiere con el Wi-Fi
#define ADC_CHAN_BATTERY ADC_CHANNEL_6 // Corresponde al GPIO34 en ESP32
#define ADC_CHAN_SOLAR   ADC_CHANNEL_7 // Corresponde al GPIO35 en ESP32

// Configuración del filtro por software
#define SAMPLES_COUNT 30
#define SAMPLE_PERIOD_MS 50

// Factor del nuevo divisor (R1=20k, R2=10k). Voltaje Entrada = Voltaje ADC * 3.0
#define VOLTAGE_DIVIDER_FACTOR 3.0f

// Variables donde se almacenará el resultado final (accesibles desde los getters)
static float battery_voltage_avg = 0.0f;
static float solar_voltage_avg = 0.0f;

// Handles del hardware ADC
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

// Buffers para el filtro de Media Móvil
static int bat_raw_buffer[SAMPLES_COUNT] = {0};
static int sol_raw_buffer[SAMPLES_COUNT] = {0};
static int buffer_index = 0;
static bool buffer_filled = false;

// --- Función interna para calibrar el ADC (Recomendado en ESP32) ---
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit, .atten = atten, .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        calibrated = (ret == ESP_OK);
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit, .atten = atten, .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        calibrated = (ret == ESP_OK);
    }
#endif
    *out_handle = handle;
    return calibrated;
}

// --- Tarea FreeRTOS ---
static void power_monitor_task(void *pvParameters) {
    while (1) {
        int bat_raw, sol_raw;
        
        // 1. Leer ADC
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_BATTERY, &bat_raw));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN_SOLAR, &sol_raw));
        // 2. Insertar en el Buffer Circular
        bat_raw_buffer[buffer_index] = bat_raw;
        sol_raw_buffer[buffer_index] = sol_raw;
        
        buffer_index++;
        if (buffer_index >= SAMPLES_COUNT) {
            buffer_index = 0;
            buffer_filled = true;
        }
        // 3. Calcular Promedio Crudo
        int limit = buffer_filled ? SAMPLES_COUNT : buffer_index;
        if (limit > 0) {
            long bat_sum = 0, sol_sum = 0;
            for (int i = 0; i < limit; i++) {
                bat_sum += bat_raw_buffer[i];
                sol_sum += sol_raw_buffer[i];
            }
            int bat_avg_raw = bat_sum / limit;
            int sol_avg_raw = sol_sum / limit;
            // 4. Convertir a Milivoltios (usando la curva de calibración del chip)
            int bat_mv = 0, sol_mv = 0;
            if (do_calibration) {
                adc_cali_raw_to_voltage(adc1_cali_handle, bat_avg_raw, &bat_mv);
                adc_cali_raw_to_voltage(adc1_cali_handle, sol_avg_raw, &sol_mv);
            } else {
                bat_mv = (bat_avg_raw * 3300) / 4095; // Fallback teórico
                sol_mv = (sol_avg_raw * 3300) / 4095;
            }
            // 5. Aplicar Divisor Físico y guardar en Voltios reales
            battery_voltage_avg = (bat_mv / 1000.0f) * VOLTAGE_DIVIDER_FACTOR;
            solar_voltage_avg = (sol_mv / 1000.0f) * VOLTAGE_DIVIDER_FACTOR;
        }
        // 6. Dormir Tarea por 50ms
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

// --- Funciones Públicas ---
void power_monitor_init(void) {
    ESP_LOGI(TAG, "Inicializando ADC para Monitoreo de Energía...");
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    // Configurar atenuación a 12dB (Para medir hasta ~3.1V en el pin)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_BATTERY, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN_SOLAR, &config));
    do_calibration = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);
    // Arrancar la tarea en el Core 1 (prioridad intermedia)
    xTaskCreatePinnedToCore(power_monitor_task, "power_monitor_task", 4096, NULL, 5, NULL, 1);
}

float power_monitor_get_battery_voltage(void) {
    return battery_voltage_avg;
}

float power_monitor_get_solar_voltage(void) {
    return solar_voltage_avg;
}

adc_oneshot_unit_handle_t power_monitor_get_adc_handle(void) {
    return adc1_handle;
}