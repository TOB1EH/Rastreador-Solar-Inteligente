idf_component_register(SRCS "telegram_bot.c" "cJSON.c"
                       INCLUDE_DIRS "include" "."
                       REQUIRES power_monitor esp_http_client udp_logger)
