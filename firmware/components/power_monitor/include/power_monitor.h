#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el hardware del ADC y arranca la tarea de monitoreo en FreeRTOS
 */
void power_monitor_init(void);

/**
 * @brief Obtiene el voltaje estable (filtrado) de la batería principal
 * @return Voltaje en Voltios (ej. 7.42)
 */
float power_monitor_get_battery_voltage(void);

/**
 * @brief Obtiene el voltaje estable (filtrado) del panel solar
 * @return Voltaje en Voltios (ej. 5.15)
 */
float power_monitor_get_solar_voltage(void);

/**
 * @brief Obtiene el handle compartido del ADC_UNIT_1
 * @return adc_oneshot_unit_handle_t para usar en otros componentes
 */
adc_oneshot_unit_handle_t power_monitor_get_adc_handle(void);

#ifdef __cplusplus
}

#endif
#endif // POWER_MONITOR_H