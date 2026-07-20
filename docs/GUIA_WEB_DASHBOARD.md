# Dashboard Web — Rastreador Solar Inteligente

**Servidor HTTP embebido para monitorear voltajes desde cualquier navegador en la misma red.**

---

## Descripcion General

El ESP32 ejecuta un servidor HTTP liviano (basado en `esp_http_server` de ESP-IDF) que expone una interfaz web para monitorear los voltajes del sistema en tiempo real. La pagina se actualiza automaticamente cada 3 segundos sin necesidad de recargar el navegador.

### Endpoints

| Ruta | Metodo | Descripcion |
|------|--------|-------------|
| `/` | GET | Pagina HTML con indicadores visuales |
| `/api/status` | GET | JSON con `{"battery":x.xx,"solar":y.yy}` |
| `/favicon.ico` | GET | 204 No Content (evita error 404) |

---

## Como Acceder

### 1. Obtener la IP del ESP32

Conectate al monitor serial:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Busca una linea como:
```
UDP_LOGGER: Conectado. IP: 192.168.18.149
WEB_DASH: Dashboard HTTP en http://192.168.x.x:80
```

### 2. Abrir en el Navegador

```
http://192.168.18.149
```

### 3. Leer los Indicadores

La pagina muestra:
- **Bateria** — Tension actual en voltios (verde)
- **Panel Solar** — Tension actual en voltios (amarillo)
- **Indicador de conexion** — punto verde (conectado) / rojo (desconectado)
- **Timestamp** — hora de la ultima actualizacion

---

## Arquitectura

```
Navegador                          ESP32
  |                                  |
  |-- GET / ------------------------>|
  |<-- HTML + CSS + JS -------------|
  |                                  |
  |-- GET /api/status -------------->|
  |    (cada 3s via fetch())        |
  |<-- {"battery":4.12,"solar":5.50}|
  |                                  |
  |-- GET /favicon.ico -------------|
  |<-- 204 No Content --------------|
```

### Inicializacion

El servidor se inicia despues de que el WiFi esta conectado:

```c
void web_dashboard_start(void) {
    udp_logger_wait_connected();           // Espera WiFi
    httpd_start(&server, &config);         // Inicia servidor HTTP
    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_api_status);
    httpd_register_uri_handler(server, &uri_favicon);
}
```

Se llama desde `app_main()` en `main/main.c`.

---

## API REST

### `GET /api/status`

**Response:**
```json
{
    "battery": 4.12,
    "solar": 5.50
}
```

Implementacion:
```c
static esp_err_t api_status_get_handler(httpd_req_t *req) {
    float v_bat = power_monitor_get_battery_voltage();
    float v_sol = power_monitor_get_solar_voltage();
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "{\"battery\":%.2f,\"solar\":%.2f}", v_bat, v_sol);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}
```

---

## Interfaz de Usuario

La pagina HTML se sirve completa desde el firmware (~3KB). Incluye:

### HTML
- Tema oscuro (`background: #0f172a`)
- Dos tarjetas: bateria y panel solar
- Indicador de conexion en vivo
- Timestamp de ultima actualizacion

### JavaScript
- `fetch()` cada 3 segundos a `/api/status`
- Actualiza DOM sin recargar la pagina
- Manejo de errores: punto rojo si el servidor no responde

```javascript
async function fetchStatus() {
    try {
        const r = await fetch('/api/status');
        const d = await r.json();
        document.getElementById('bat').innerHTML = d.battery.toFixed(2) + 'V';
        document.getElementById('sol').innerHTML = d.solar.toFixed(2) + 'V';
        document.getElementById('dot').className = 'dot on';
    } catch(e) {
        document.getElementById('ts').textContent = 'Error de conexion';
        document.getElementById('dot').className = 'dot off';
    }
}
setInterval(fetchStatus, 3000);
```

---

## Configuracion

```bash
cd firmware
idf.py menuconfig
```

Navega a:
```
Web Dashboard Configuration  >
    (80) HTTP Server Port
```

Cambiar el puerto (por ejemplo, `8080`) si el puerto 80 ya esta ocupado en la red.

---

## Especificacion Tecnica

| Aspecto | Detalle |
|---------|---------|
| **Servidor** | `esp_http_server` de ESP-IDF |
| **Puerto default** | 80 (configurable) |
| **Stack** | Default de HTTPD (4096+ bytes) |
| **Intervalo refresh** | 3 segundos (hardcodeado en JS) |
| **Tamano pagina** | ~3 KB |
| **Dependencias** | `power_monitor`, `udp_logger`, `esp_http_server` |

---

## Archivos Involucrados

| Archivo | Rol |
|---------|-----|
| `firmware/components/web_dashboard/web_dashboard.c` | Servidor HTTP, endpoints, HTML embebido |
| `firmware/components/web_dashboard/include/web_dashboard.h` | Declaracion de `web_dashboard_start()` |
| `firmware/components/web_dashboard/CMakeLists.txt` | Registro del componente |
| `firmware/components/web_dashboard/Kconfig.projbuild` | Configuracion del puerto HTTP |

---

## Notas

- El dashboard es **solo lectura** (no permite cambiar configuraciones desde el navegador).
- El ESP32 debe estar conectado a la misma red que el dispositivo desde el que se accede.
- Si el puerto 80 no esta disponible, cambiarlo desde menuconfig.
- El favicon handler devuelve `204 No Content` para evitar errores 404 en los logs del navegador.
