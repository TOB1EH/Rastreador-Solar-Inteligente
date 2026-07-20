# Bot Telegram — Rastreador Solar Inteligente

**Cómo configurar y usar el bot de Telegram para monitorear el rastreador desde cualquier lugar.**

---

## Descripción General

El firmware incluye un bot de Telegram que permite consultar el estado del rastreador solar desde cualquier dispositivo con Telegram. Funciona mediante **polling** (consulta periódica a la API de Telegram) y responde solo a chats privados.

### Comandos Disponibles

| Comando | Descripción |
|---------|-------------|
| `/start` | Mensaje de bienvenida con lista de comandos |
| `/status` | Reporta tensión de batería y panel solar en tiempo real |
| `/help` | Muestra la ayuda con todos los comandos |

### Ejemplo de Respuesta

```
Bateria: 4.12V
Panel Solar: 5.50V
```

---

## Configuracion

### 1. Crear el Bot en Telegram

1. Abri Telegram y buscas **@BotFather**
2. Envia `/newbot`
3. Segui las instrucciones (nombre, username)
4. **BotFather** te va a dar un token similar a: `1234567890:ABCdefGHIjklmNOPqrSTUvwxYZ`

### 2. Configurar el Token en el Firmware

```bash
cd firmware
idf.py menuconfig
```

Navega a:
```
Telegram Bot Configuration  >
    (1234567890:ABCdef...) Bot Token
    (5) Polling interval (seconds)
```

Guarda y sali.

### 3. Compilar y Flashear

```bash
idf.py build flash monitor
```

En el monitor serial deberias ver:
```
TELEGRAM: Esperando WiFi...
TELEGRAM: Bot de Telegram iniciado
TELEGRAM: Update: chat_id=7421578765 type=private
TELEGRAM: Comando: /status (chat: 7421578765)
TELEGRAM: sendMessage: {"ok":true}
```

---

## Arquitectura de Conexion

```
ESP32                              Telegram API
  |                                       |
  |-- GET getUpdates (cada 5s) ---------->|
  |<-- [lista de updates] ---------------|
  |                                       |
  |-- Por cada update nuevo:             |
  |   |-- chat.type == "private"? -- no --|-> ignorar
  |   |-- text == "/status"?             |
  |   |-- Obtener v_bat, v_sol           |
  |   |-- POST sendMessage ------------> |
  |<-- {"ok":true} ---------------------|
```

### Polling vs Webhook

El bot usa **long polling** en lugar de webhook. Esto significa que el ESP32 consulta activamente la API de Telegram cada `N` segundos (default 5) preguntando si hay mensajes nuevos. No requiere puertos abiertos ni certificados SSL en el ESP32, lo que simplifica la configuracion.

### TLS y Certificados

Todas las conexiones con la API de Telegram utilizan HTTPS con verificacion de certificados mediante **`esp_crt_bundle_attach`** de ESP-IDF. Este bundle incluye los certificados raiz de las principales autoridades certificadoras, evitando tener que hardcodear un certificado especifico.

---

## Especificacion Tecnica

| Aspecto | Detalle |
|---------|---------|
| **API Base** | `https://api.telegram.org/bot<TOKEN>/` |
| **Polling** | `getUpdates?offset=<last_id+1>` |
| **Respuesta** | `sendMessage` via HTTP POST con body JSON |
| **Intervalo** | Configurable via menuconfig (default 5s) |
| **Stack task** | 8192 bytes |
| **Prioridad** | 5 |
| **Chat soportados** | Solo privados (ignora grupos/supergrupos) |

### Construccion del Body JSON

```json
{"chat_id":7421578765,"text":"Bateria: 4.12V\nPanel Solar: 5.50V"}
```

### Escape de Caracteres

Los caracteres especiales (`\n`, `"`, `\`) en el texto del mensaje se escapan antes de incrustarlos en el JSON para evitar que la API de Telegram rechace la peticion con error 400:

```c
static void json_escape(const char *in, char *out, size_t out_size) {
    while (*in && out_size > 1) {
        if (*in == '\n') { *out++ = '\\'; *out++ = 'n'; out_size -= 2; }
        else if (*in == '"' || *in == '\\') { *out++ = '\\'; *out++ = *in; out_size -= 2; }
        else { *out++ = *in; out_size--; }
        in++;
    }
    *out = '\0';
}
```

---

## Consideraciones Tecnicas

### chat_id de 64 bits

El `chat_id` en Telegram puede superar el rango de un `int` de 32 bits. Se utiliza `valuedouble` de cJSON y se formatea con `%.0f` para preservar el identificador completo.

### Filtro de Chats Privados

El bot ignora mensajes de grupos o supergrupos. Solo responde a chats privados, lo que evita respuestas no deseadas y reduce el uso del plan gratuito de la API.

### Apertura de Conexiones

Cada ciclo de polling abre y cierra conexiones HTTPS. No se mantienen conexiones persistentes, lo que aumenta la latencia (~1-2s por negociacion TLS) pero simplifica el manejo de errores.

---

## Archivos Involucrados

| Archivo | Rol |
|---------|-----|
| `firmware/components/telegram_bot/telegram_bot.c` | Logica del bot: polling, parseo, comandos |
| `firmware/components/telegram_bot/cJSON.c` / `cJSON.h` | Parser JSON (copia local) |
| `firmware/components/telegram_bot/include/telegram_bot.h` | Declaracion de `telegram_bot_start()` |
| `firmware/components/telegram_bot/CMakeLists.txt` | Registro del componente, requiere mbedtls para TLS |
| `firmware/components/telegram_bot/Kconfig.projbuild` | Configuracion: token, intervalo de polling |

---

## Notas

- El token del bot debe configurarse en menuconfig **antes de compilar**.
- Si el token esta vacio, el bot se desactiva en runtime y muestra un warning.
- El ESP32 debe tener conexion a Internet (WiFi + gateway) para llegar a `api.telegram.org`.
- El consumo de datos es minimo: ~1KB por ciclo de polling.
