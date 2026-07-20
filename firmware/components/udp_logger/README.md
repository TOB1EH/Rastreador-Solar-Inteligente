# udp_logger

Gestión de WiFi (STA) y servidor UDP en modo Pull para el monitor remoto.

## API

```c
void udp_logger_init(void);
```
Inicializa WiFi en modo STA, lee credenciales de NVS o sdkconfig, y arranca la conexión. Si no hay credenciales guardadas, entra en modo configuración serial por 5s escuchando `WIFI:SSID|PASS\n` e imprime `WAITING_CONFIG` cada 500ms.

```c
void udp_logger_wait_connected(void);
```
Bloquea hasta que WiFi esté conectado y tenga IP (usa event group interno).

```c
void udp_logger_server_task(void *pv);
```
Tarea que ejecuta el servidor UDP en puerto `CONFIG_UDP_BROADCAST_PORT` (defecto 8080). Atiende peticiones Pull del daemon Python.

```c
void udp_logger_save_wifi(const char *ssid, const char *pass);
```
Guarda SSID y password en NVS y reinicia el ESP32 automáticamente.

## Dependencias

- `esp_wifi`, `esp_netif`, `lwip`, `nvs_flash`, `esp_event`, `power_monitor`

## Configuración (Kconfig)

| Opción | Defecto | Descripción |
|--------|---------|-------------|
| `WIFI_SSID` | `"mi_red_wifi"` | SSID de la red WiFi |
| `WIFI_PASSWORD` | `"mi_contraseña"` | Contraseña WiFi |
| `UDP_BROADCAST_PORT` | `8080` | Puerto UDP del servidor |

## Prioridad de credenciales

1. NVS (persistente entre reinicios)
2. Modo configuración serial (5s al primer boot)
3. sdkconfig / menuconfig (fallback)

## Modo configuración serial

- ESP32 imprime `WAITING_CONFIG` cada 500ms durante 5s
- Enviar `WIFI:SSID|PASSWORD\n` por puerto serie
- ESP32 responde `OK` y guarda en NVS, luego reinicia
- `daemon_pc/configure_wifi.py` automatiza este proceso escaneando redes vía `nmcli`
