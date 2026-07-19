#include "web_dashboard.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "power_monitor.h"
#include "udp_logger.h"

static const char *TAG = "WEB_DASH";

#define DASHBOARD_PORT CONFIG_WEB_DASHBOARD_PORT

static const char *HTML_PAGE =
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>Rastreador Solar</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box;font-family:system-ui,sans-serif}"
"body{background:#0f172a;color:#e2e8f0;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}"
".container{max-width:500px;width:100%}"
"h1{font-size:1.5rem;text-align:center;margin-bottom:24px;color:#fbbf24}"
".grid{display:flex;flex-direction:column;gap:16px}"
".card{background:#1e293b;border-radius:16px;padding:24px;text-align:center;border:1px solid #334155}"
".card .label{font-size:0.85rem;text-transform:uppercase;letter-spacing:1px;color:#94a3b8;margin-bottom:8px}"
".card .value{font-size:2.5rem;font-weight:700}"
".card .unit{font-size:1rem;color:#94a3b8;margin-left:4px}"
".bat .value{color:#22c55e}"
".sol .value{color:#facc15}"
".status{text-align:center;margin-top:16px;font-size:0.85rem;color:#64748b}"
".status .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}"
".status .dot.on{background:#22c55e}"
".status .dot.off{background:#ef4444}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>\u2600\ufe0f Rastreador Solar</h1>"
"<div class='grid'>"
"<div class='card bat'>"
"<div class='label'>\U0001f50b Bater\u00eda</div>"
"<div class='value' id='bat'>--<span class='unit'>V</span></div>"
"</div>"
"<div class='card sol'>"
"<div class='label'>\u2600\ufe0f Panel Solar</div>"
"<div class='value' id='sol'>--<span class='unit'>V</span></div>"
"</div>"
"</div>"
"<div class='status'><span class='dot on' id='dot'></span><span id='ts'>Esperando datos...</span></div>"
"</div>"
"<script>"
"async function fetchStatus(){"
"try{"
"const r=await fetch('/api/status');"
"const d=await r.json();"
"document.getElementById('bat').innerHTML=d.battery.toFixed(2)+'<span class=unit>V</span>';"
"document.getElementById('sol').innerHTML=d.solar.toFixed(2)+'<span class=unit>V</span>';"
"const t=new Date();"
"document.getElementById('ts').textContent='Actualizado: '+t.toLocaleTimeString();"
"document.getElementById('dot').className='dot on';"
"}catch(e){"
"document.getElementById('ts').textContent='Error de conexi\u00f3n';"
"document.getElementById('dot').className='dot off';"
"}}"
"setInterval(fetchStatus,3000);"
"fetchStatus();"
"</script>"
"</body>"
"</html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    return ESP_OK;
}

static esp_err_t api_status_get_handler(httpd_req_t *req) {
    float v_bat = power_monitor_get_battery_voltage();
    float v_sol = power_monitor_get_solar_voltage();

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "{\"battery\":%.2f,\"solar\":%.2f}", v_bat, v_sol);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_api_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = api_status_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_favicon = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL
};

void web_dashboard_start(void) {
    ESP_LOGI(TAG, "Esperando WiFi...");
    udp_logger_wait_connected();
    ESP_LOGI(TAG, "WiFi conectado, iniciando servidor HTTP...");

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = DASHBOARD_PORT;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al iniciar servidor HTTP");
        return;
    }

    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_api_status);
    httpd_register_uri_handler(server, &uri_favicon);

    ESP_LOGI(TAG, "Dashboard HTTP en http://192.168.x.x:%d", DASHBOARD_PORT);
}
