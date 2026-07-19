#include "udp_logger.h"
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "power_monitor.h"

static const char *TAG = "UDP_LOGGER";

#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASSWORD
#define UDP_PORT       CONFIG_UDP_BROADCAST_PORT
#define NVS_NAMESPACE  "wifi_cfg"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static int sock = -1;
static struct sockaddr_in dest_addr;

static char s_wifi_ssid[32] = {0};
static char s_wifi_pass[64] = {0};
static bool s_creds_from_nvs = false;

void udp_logger_save_wifi(const char *ssid, const char *pass) {
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "pass", pass));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    ESP_LOGI(TAG, "WiFi guardado en NVS: %s", ssid);
    esp_restart();
}

static void load_wifi_creds(void) {
    s_creds_from_nvs = false;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        strlcpy(s_wifi_ssid, WIFI_SSID, sizeof(s_wifi_ssid));
        strlcpy(s_wifi_pass, WIFI_PASS, sizeof(s_wifi_pass));
        return;
    }
    size_t len = sizeof(s_wifi_ssid);
    err = nvs_get_str(nvs, "ssid", s_wifi_ssid, &len);
    if (err != ESP_OK) {
        nvs_close(nvs);
        strlcpy(s_wifi_ssid, WIFI_SSID, sizeof(s_wifi_ssid));
        strlcpy(s_wifi_pass, WIFI_PASS, sizeof(s_wifi_pass));
        return;
    }
    len = sizeof(s_wifi_pass);
    nvs_get_str(nvs, "pass", s_wifi_pass, &len);
    nvs_close(nvs);
    s_creds_from_nvs = true;
    ESP_LOGI(TAG, "Credenciales desde NVS: %s", s_wifi_ssid);
}

static void config_mode_serial(void) {
    ESP_LOGI(TAG, "Sin NVS. Modo config serial (5s)...");
    TickType_t start = xTaskGetTickCount();
    char buf[128];
    int pos = 0;

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(5000)) {
        printf("WAITING_CONFIG\n");
        vTaskDelay(pdMS_TO_TICKS(500));

        struct pollfd pfd = { .fd = 0, .events = POLLIN };
        while (poll(&pfd, 1, 50) > 0) {
            char c;
            if (read(0, &c, 1) != 1) break;
            if (c == '\n' || c == '\r') {
                buf[pos] = '\0';
                pos = 0;
                if (strncmp(buf, "WIFI:", 5) == 0) {
                    char *ssid = buf + 5;
                    char *pass = strchr(ssid, '|');
                    if (pass) *pass++ = '\0';
                    printf("OK\n");
                    ESP_LOGI(TAG, "Config via serial: SSID=%s", ssid);
                    udp_logger_save_wifi(ssid, pass ? pass : "");
                    return;
                }
            } else if (pos < (int)sizeof(buf) - 1) {
                buf[pos++] = c;
            }
            if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(5000)) break;
            pfd.fd = 0;
            pfd.events = POLLIN;
            pfd.revents = 0;
        }
    }
    ESP_LOGW(TAG, "Timeout config serial. Usando menuconfig.");
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi desconectado. Reintentando...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, s_wifi_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, s_wifi_pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi iniciado, conectando a SSID: %s ...", s_wifi_ssid);

    ESP_LOGI(TAG, "WiFi init finalizado. Conectando a SSID:%s...", s_wifi_ssid);
}

void udp_logger_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_wifi_creds();
    if (!s_creds_from_nvs) {
        config_mode_serial();
    }
    wifi_init_sta();
}

void udp_logger_wait_connected(void) {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

void udp_logger_server_task(void *pv) {
    char rx_buffer[128];
    char tx_buffer[128];
    
    // Esperar a que el WiFi esté conectado antes de crear el socket
    ESP_LOGI(TAG, "Esperando conexión WiFi para iniciar servidor UDP...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(UDP_PORT);

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Fallo al crear socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Fallo al hacer bind del socket: errno %d", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Servidor UDP escuchando en el puerto %d", UDP_PORT);

        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            
            // Recibir consulta
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            
            if (len < 0) {
                ESP_LOGE(TAG, "Fallo en recvfrom: errno %d", errno);
                break; // Romper el loop interno para recrear el socket
            } else {
                rx_buffer[len] = 0; // Null-terminator
                ESP_LOGD(TAG, "Recibido %d bytes, consulta: %s", len, rx_buffer);
                
                // Obtener datos
                float v_bat = power_monitor_get_battery_voltage();
                float v_sol = power_monitor_get_solar_voltage();
                
                // Formatear respuesta
                snprintf(tx_buffer, sizeof(tx_buffer), "BAT:%.2f|SOL:%.2f", v_bat, v_sol);
                
                // Enviar respuesta a la IP/Puerto origen
                int err = sendto(sock, tx_buffer, strlen(tx_buffer), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Fallo al enviar respuesta: errno %d", errno);
                } else {
                    ESP_LOGI(TAG, "Respuesta enviada: %s", tx_buffer);
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Cerrando socket y reiniciando servidor...");
            close(sock);
            sock = -1;
        }
    }
    vTaskDelete(NULL);
}
