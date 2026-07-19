#include "telegram_bot.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "power_monitor.h"
#include "udp_logger.h"

static const char *TAG = "TELEGRAM";

#define BOT_TOKEN CONFIG_TELEGRAM_BOT_TOKEN
#define POLL_SECS CONFIG_TELEGRAM_POLL_SECS

#define API_BASE "https://api.telegram.org/bot"
#define GET_UPDATES "/getUpdates?offset="
#define SEND_MSG   "/sendMessage"

static int last_update_id = 0;

static char *http_request(const char *url) {
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024,
        .buffer_size_tx = 256,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    char *resp = NULL;
    int total = 0;
    int alloc = 512;

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        int content_len = esp_http_client_fetch_headers(client);
        if (content_len > 0) {
            alloc = content_len + 1;
        }
        resp = malloc(alloc);
        if (resp) {
            int r;
            while ((r = esp_http_client_read(client, resp + total, alloc - total - 1)) > 0) {
                total += r;
                if (total >= alloc - 1) {
                    alloc += 512;
                    char *tmp = realloc(resp, alloc);
                    if (!tmp) break;
                    resp = tmp;
                }
            }
            resp[total] = 0;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return resp;
}

static char *http_post(const char *url, const char *body) {
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 512,
        .buffer_size_tx = 512,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    char *resp = NULL;
    int total = 0;
    int alloc = 512;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    int body_len = strlen(body);
    esp_err_t err = esp_http_client_open(client, body_len);
    if (err == ESP_OK) {
        esp_http_client_write(client, body, body_len);
        int content_len = esp_http_client_fetch_headers(client);
        if (content_len > 0) {
            alloc = content_len + 1;
        }
        resp = malloc(alloc);
        if (resp) {
            int r;
            while ((r = esp_http_client_read(client, resp + total, alloc - total - 1)) > 0) {
                total += r;
                if (total >= alloc - 1) {
                    alloc += 512;
                    char *tmp = realloc(resp, alloc);
                    if (!tmp) break;
                    resp = tmp;
                }
            }
            resp[total] = 0;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return resp;
}

static void json_escape(const char *in, char *out, size_t out_size) {
    while (*in && out_size > 1) {
        if (*in == '\n') {
            if (out_size < 3) break;
            *out++ = '\\';
            *out++ = 'n';
            out_size -= 2;
        } else if (*in == '"' || *in == '\\') {
            if (out_size < 3) break;
            *out++ = '\\';
            *out++ = *in;
            out_size -= 2;
        } else {
            *out++ = *in;
            out_size--;
        }
        in++;
    }
    *out = '\0';
}

static void handle_command(const char *cmd, double chat_id) {
    char url[512];
    char response[256];

    if (strcmp(cmd, "/start") == 0) {
        snprintf(response, sizeof(response),
            "\u00a1Hola! Soy el monitor de tu Rastreador Solar Inteligente.\n\n"
            "Comandos:\n"
            "/status - Ver voltajes\n"
            "/help - Ayuda");
    } else if (strcmp(cmd, "/status") == 0) {
        float v_bat = power_monitor_get_battery_voltage();
        float v_sol = power_monitor_get_solar_voltage();
        snprintf(response, sizeof(response),
            "\U0001f50b Bater\u00eda: %.2fV\n\u2600\ufe0f Panel Solar: %.2fV", v_bat, v_sol);
    } else if (strcmp(cmd, "/help") == 0) {
        snprintf(response, sizeof(response),
            "Comandos disponibles:\n"
            "/status - Ver voltajes de bater\u00eda y panel solar\n"
            "/start - Mensaje de bienvenida\n"
            "/help - Esta ayuda");
    } else {
        snprintf(response, sizeof(response),
            "Comando no reconocido: %s\nEnvi\u00e1 /help para ver los comandos disponibles.", cmd);
    }

    snprintf(url, sizeof(url), "%s%s%s", API_BASE, BOT_TOKEN, SEND_MSG);

    char escaped[512];
    json_escape(response, escaped, sizeof(escaped));
    char body[1024];
    snprintf(body, sizeof(body), "{\"chat_id\":%.0f,\"text\":\"%s\"}", chat_id, escaped);

    char *resp = http_post(url, body);
    if (resp) {
        ESP_LOGI(TAG, "sendMessage: %s", resp);
        free(resp);
    } else {
        ESP_LOGE(TAG, "sendMessage: no response from Telegram");
    }
}

static void process_updates(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    if (!ok || !cJSON_IsTrue(ok)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    int len = cJSON_GetArraySize(result);
    for (int i = 0; i < len; i++) {
        cJSON *upd = cJSON_GetArrayItem(result, i);
        cJSON *upd_id = cJSON_GetObjectItem(upd, "update_id");
        if (!upd_id) continue;

        int uid = upd_id->valueint;
        if (uid <= last_update_id) continue;
        last_update_id = uid;

        cJSON *msg = cJSON_GetObjectItem(upd, "message");
        if (!msg) continue;

        cJSON *chat = cJSON_GetObjectItem(msg, "chat");
        if (!chat) continue;
        cJSON *chat_id = cJSON_GetObjectItem(chat, "id");
        if (!chat_id) continue;

        cJSON *chat_type = cJSON_GetObjectItem(chat, "type");
        const char *type_str = cJSON_IsString(chat_type) ? chat_type->valuestring : "unknown";
        ESP_LOGI(TAG, "Update: chat_id=%.0f type=%s", chat_id->valuedouble, type_str);

        if (strcmp(type_str, "private") != 0) {
            ESP_LOGI(TAG, "Ignorando mensaje de chat no privado");
            continue;
        }

        cJSON *text = cJSON_GetObjectItem(msg, "text");
        if (!cJSON_IsString(text)) continue;

        ESP_LOGI(TAG, "Comando: %s (chat: %.0f)", text->valuestring, chat_id->valuedouble);
        handle_command(text->valuestring, chat_id->valuedouble);
    }

    cJSON_Delete(root);
}

static void telegram_bot_task(void *pv) {
    ESP_LOGI(TAG, "Esperando WiFi...");
    udp_logger_wait_connected();
    ESP_LOGI(TAG, "Bot de Telegram iniciado");

    if (strlen(BOT_TOKEN) == 0) {
        ESP_LOGW(TAG, "TELEGRAM_BOT_TOKEN no configurado. Usa menuconfig.");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        char url[512];
        snprintf(url, sizeof(url), "%s%s%s%d", API_BASE, BOT_TOKEN, GET_UPDATES, last_update_id + 1);

        char *resp = http_request(url);
        if (resp) {
            ESP_LOGD(TAG, "getUpdates: %s", resp);
            process_updates(resp);
            free(resp);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_SECS * 1000));
    }
}

void telegram_bot_start(void) {
    xTaskCreate(telegram_bot_task, "telegram_bot", 8192, NULL, 5, NULL);
}
