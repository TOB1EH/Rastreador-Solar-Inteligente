#ifndef UDP_LOGGER_H
#define UDP_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

void udp_logger_init(void);

void udp_logger_wait_connected(void);

void udp_logger_server_task(void *pv);

void udp_logger_save_wifi(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif

#endif // UDP_LOGGER_H
