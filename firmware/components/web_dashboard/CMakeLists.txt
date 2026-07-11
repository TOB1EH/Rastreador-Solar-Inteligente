idf_component_register(SRCS "web_dashboard.c"
                       INCLUDE_DIRS "include"
                       REQUIRES power_monitor esp_http_server udp_logger)
