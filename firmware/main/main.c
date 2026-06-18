#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo_motor.h"
#include "power_monitor.h"
#include "udp_logger.h"

static const char *TAG = "SERVO_BATT_TEST";

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando prueba de servos (0-180 y 0-90) con monitor UDP en paralelo...");
    
    // Inicializa el sistema de energía
    power_monitor_init();

    // Inicializa la comunicación WiFi y el servidor UDP
    udp_logger_init();
    
    // Inicia la tarea del servidor UDP para que el daemon en PC funcione
    xTaskCreate(udp_logger_server_task, "udp_server", 4096, NULL, 5, NULL);

    // Inicializa el control de los servos
    servo_motor_init();
    
    // Centrar ambos servos de forma segura inicialmente
    servo_motor_set_angle(SERVO_AXIS_AZIMUT, 90);
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, 45);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos para estabilizar
    
    while (1) {
        ESP_LOGI(TAG, "Sweep: Ida Azimut (0->180)...");
        for (int ang = 0; ang <= 180; ang += 1) { 
            servo_motor_set_angle(SERVO_AXIS_AZIMUT, ang);
            vTaskDelay(pdMS_TO_TICKS(50)); // 50ms de pausa por grado (mucho menos estrés)
        }
        
        ESP_LOGI(TAG, "Pausa entre ejes...");
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Sweep: Ida Elevación (0->90)...");
        for (int ang = 0; ang <= 90; ang += 1) { 
            servo_motor_set_angle(SERVO_AXIS_ELEVATION, ang);
            vTaskDelay(pdMS_TO_TICKS(50)); // 50ms de pausa por grado
        }
        
        ESP_LOGI(TAG, "Pausa en el extremo...");
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "Sweep: Vuelta Elevación (90->0)...");
        for (int ang = 90; ang >= 0; ang -= 1) {
            servo_motor_set_angle(SERVO_AXIS_ELEVATION, ang);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        ESP_LOGI(TAG, "Pausa entre ejes...");
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Sweep: Vuelta Azimut (180->0)...");
        for (int ang = 180; ang >= 0; ang -= 1) {
            servo_motor_set_angle(SERVO_AXIS_AZIMUT, ang);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        ESP_LOGI(TAG, "Pausa en el origen...");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}