#ifndef LDR_SENSOR_H
#define LDR_SENSOR_H

#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el hardware y la tarea de lectura 
void ldr_sensor_init(adc_oneshot_unit_handle_t adc_handle);

// Devuelve los errores calculados. 

// Valores positivos o negativos indican la dirección. 0 indica "no moverse".
void ldr_sensor_get_errors(int *azimut_err, int *elevacion_err);

// Devuelve los valores crudos promediados (útil para el Telegram/Dashboard)
void ldr_sensor_get_raw_values(int *top, int *bottom, int *left, int *right);

#ifdef __cplusplus
}
#endif

#endif // LDR_SENSOR_H