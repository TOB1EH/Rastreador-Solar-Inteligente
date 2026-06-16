idf_component_register(SRCS "udp_logger.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_netif lwip nvs_flash esp_event power_monitor)
