# web_dashboard

Servidor HTTP embebido que expone un dashboard web con el estado del tracker solar.

## API

```c
void web_dashboard_start(void);
```
Arranca el servidor HTTP en `CONFIG_WEB_DASHBOARD_PORT` (defecto 80). Registra los endpoints y crea la tarea del servidor.

## Dependencias

- `esp_http_server`, `power_monitor`, `udp_logger`

## Endpoints

| Ruta | Descripción |
|------|-------------|
| `GET /` | Página HTML con JS que auto-refresca cada 3s. Muestra voltajes, errores LDR, ángulos de servos y estado WiFi. |
| `GET /api/status` | JSON con todos los valores del sistema. |
| `GET /favicon.ico` | Retorna 204 (sin contenido). |

## Configuración (Kconfig)

| Opción | Defecto | Descripción |
|--------|---------|-------------|
| `WEB_DASHBOARD_PORT` | `80` | Puerto del servidor HTTP |

## Estructura del JSON `/api/status`

```json
{
  "battery": 7.42,
  "solar": 5.15,
  "azimuth": 90,
  "elevation": 110,
  "err_x": 0,
  "err_y": -15,
  "wifi_rssi": -45,
  "timestamp_ms": 1234567
}
```
