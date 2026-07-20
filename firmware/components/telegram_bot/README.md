# telegram_bot

Cliente de Telegram Bot API para consultar el estado del tracker desde cualquier lugar.

## API

```c
void telegram_bot_start(void);
```
Inicializa el cliente HTTPS, registra el certificado TLS via `esp_crt_bundle_attach`, y crea la tarea de polling que consulta `getUpdates` cada `CONFIG_TELEGRAM_POLL_SECS` segundos.

## Dependencias

- `power_monitor`, `esp_http_client`, `udp_logger`, `mbedtls` (TLS)

## Configuración (Kconfig)

| Opción | Defecto | Descripción |
|--------|---------|-------------|
| `TELEGRAM_BOT_TOKEN` | `""` | Token del bot (de @BotFather) |
| `TELEGRAM_POLL_SECS` | `5` | Intervalo entre polls a la API |

## Comandos soportados

| Comando | Respuesta |
|---------|-----------|
| `/start` | Mensaje de bienvenida con instrucciones |
| `/status` | Estado actual: voltajes, errores LDR, ángulos de servos |

## Detalles técnicos

- Solo responde a mensajes de chat privado (filtra grupos).
- Chat ID se almacena como `double` y se envía con `%.0f` (soporta 64-bit).
- JSON escape manual para caracteres especiales en mensajes.
- Peticiones POST con `esp_http_client_open` + `esp_http_client_write`.
- Certificado TLS via `esp_crt_bundle_attach` (no usa CA fija).
