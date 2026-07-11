#ifndef UDP_LOGGER_H
#define UDP_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa la conexión WiFi
 */
void udp_logger_init(void);

/**
 * @brief Bloquea hasta que WiFi esté conectado
 */
void udp_logger_wait_connected(void);

/**
 * @brief Tarea de FreeRTOS que escucha peticiones UDP en puerto 8080 y responde con los voltajes
 * 
 * @param pv Parametros de la tarea (usualmente NULL)
 */
void udp_logger_server_task(void *pv);

#ifdef __cplusplus
}
#endif

#endif // UDP_LOGGER_H
