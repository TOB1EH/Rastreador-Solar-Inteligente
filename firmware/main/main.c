#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "power_monitor.h"
#include "udp_logger.h"

static const char *TAG = "POWER_TEST";

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando Test de Monitoreo de Energía...");
    
    // Inicializa la lectura ADC y la tarea que aplica el filtro de 30 muestras
    power_monitor_init();

    // Inicializa el WiFi y el UDP Logger
    udp_logger_init();
    
    // Inicia la tarea del servidor UDP
    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);
    
    while(1) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        
        ESP_LOGI(TAG, "Batería: %.2f V | Panel Solar: %.2f V", v_bat, v_sol);
        
        // Espera 1 segundo antes de la próxima impresión
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
