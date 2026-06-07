# 🗄️ GUÍA: SUPABASE + TELEGRAM BOT

**Para:** Rastreador Solar Inteligente  
**Objetivo:** Almacenamiento cloud de datos y control remoto por Telegram

---

## 📊 SUPABASE - BASE DE DATOS

### **¿Qué es Supabase?**

Supabase es un backend open-source como Firebase, pero basado en PostgreSQL.

**Ventajas para nuestro proyecto:**
- ✅ Gratis (hasta 5GB)
- ✅ REST API automática
- ✅ PostgreSQL (SQL potente)
- ✅ Autenticación integrada
- ✅ Realtime (datos en vivo)

### **Paso 1: Crear Cuenta y Proyecto**

1. Ir a https://supabase.com
2. Click "Sign up"
3. Crear cuenta con email/GitHub
4. Click "New Project"
5. Llenar datos:
   ```
   Name: rastreador-solar-proyecto
   Database password: [GENERA UNO SEGURO]
   Region: [elige más cercano]
   ```
6. Esperar ~2 minutos a que se cree

### **Paso 2: Obtener Credenciales**

Una vez creado el proyecto:

1. Click en "Settings" (abajo izquierda)
2. Click "API"
3. Copiar:
   - **URL:** `https://[project-id].supabase.co`
   - **ANON KEY:** `eyJhbGc...` (está bajo "anon" public)

**Guardar en `config.h`:**
```c
#define SUPABASE_URL "https://xxxxxxxxxxxx.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGc..."
```

### **Paso 3: Crear Tabla de Datos**

1. En Supabase, click "SQL Editor" (izquierda)
2. Click "New Query"
3. Copiar y ejecutar:

```sql
-- Crear tabla para datos solares
CREATE TABLE solar_data (
    -- ID único
    id BIGSERIAL PRIMARY KEY,
    
    -- Timestamp (cuando se capturó)
    timestamp BIGINT NOT NULL,
    
    -- Datos del panel solar
    voltaje FLOAT NOT NULL,
    corriente FLOAT NOT NULL,
    potencia FLOAT NOT NULL,
    
    -- Estado del sistema
    bateria_porcentaje FLOAT NOT NULL,
    luz_raw INTEGER NOT NULL,
    angulo_servo INTEGER NOT NULL,
    temperatura FLOAT NOT NULL,
    
    -- Metadata
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    
    -- Índices para búsquedas rápidas
    CONSTRAINT valid_voltage CHECK (voltaje >= 0),
    CONSTRAINT valid_battery CHECK (bateria_porcentaje >= 0 AND bateria_porcentaje <= 100),
    CONSTRAINT valid_angle CHECK (angulo_servo >= 0 AND angulo_servo <= 180)
);

-- Crear índice para búsquedas por timestamp
CREATE INDEX idx_solar_timestamp ON solar_data(timestamp);

-- Crear índice para búsquedas recientes
CREATE INDEX idx_solar_created ON solar_data(created_at DESC);

-- Crear vista para últimas 24 horas
CREATE VIEW solar_data_24h AS
SELECT * FROM solar_data
WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '24 hours'
ORDER BY created_at DESC;

-- Dar permisos de lectura/escritura a usuario anónimo
ALTER TABLE solar_data ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Allow public insert" ON solar_data
    FOR INSERT TO anon WITH CHECK (true);

CREATE POLICY "Allow public read" ON solar_data
    FOR SELECT TO anon USING (true);
```

**Resultado:** Tabla visible en "Table Editor" con columnas.

### **Paso 4: Insertar Datos desde ESP32**

#### **Opción A: Usando librería Arduino Supabase**

En `main/CMakeLists.txt` agregar:
```cmake
idf_component_register(
    SRCS "main.c" "sensor_ldr.c" "servo_motor.c" ...
    INCLUDE_DIRS "."
)
```

En `main/supabase_client.c`:

```c
#include "supabase_client.h"
#include "config.h"
#include "esp_http_client.h"
#include <cJSON.h>
#include <esp_log.h>

static const char *TAG = "SUPABASE";

esp_http_client_handle_t client = NULL;

// Callback para respuestas HTTP
static esp_err_t http_event_handle(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void supabase_init(void) {
    // Crear cliente HTTP
    esp_http_client_config_t config = {
        .url = SUPABASE_URL,
        .event_handler = http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .skip_cert_validation = false,
    };
    
    client = esp_http_client_init(&config);
    ESP_LOGI(TAG, "Cliente Supabase inicializado");
}

void supabase_insert(const char *table, 
                     float voltaje, 
                     float corriente, 
                     float bateria,
                     int luz,
                     int angulo,
                     float temperatura) {
    
    // Crear JSON con datos
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "timestamp", (double)time(NULL));
    cJSON_AddNumberToObject(json, "voltaje", voltaje);
    cJSON_AddNumberToObject(json, "corriente", corriente);
    cJSON_AddNumberToObject(json, "potencia", voltaje * corriente);
    cJSON_AddNumberToObject(json, "bateria_porcentaje", bateria);
    cJSON_AddNumberToObject(json, "luz_raw", luz);
    cJSON_AddNumberToObject(json, "angulo_servo", angulo);
    cJSON_AddNumberToObject(json, "temperatura", temperatura);
    
    char *json_str = cJSON_Print(json);
    
    // Construir URL
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", SUPABASE_URL, table);
    
    // Configurar petición POST
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", 
                              "Bearer " SUPABASE_ANON_KEY);
    esp_http_client_set_post_field(client, json_str, strlen(json_str));
    
    // Enviar
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 201) {
            ESP_LOGI(TAG, "✓ Datos insertados correctamente");
        } else {
            ESP_LOGW(TAG, "Status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Error HTTP: %s", esp_err_to_name(err));
    }
    
    // Limpiar
    cJSON_Delete(json);
    free(json_str);
}
```

#### **Opción B: Usar cURL (si tienes PC con Python)**

```python
import requests
import time

SUPABASE_URL = "https://xxxx.supabase.co"
ANON_KEY = "eyJhbGc..."

def insert_data(voltaje, corriente, bateria, luz, angulo, temp):
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {ANON_KEY}"
    }
    
    data = {
        "timestamp": int(time.time()),
        "voltaje": voltaje,
        "corriente": corriente,
        "potencia": voltaje * corriente,
        "bateria_porcentaje": bateria,
        "luz_raw": luz,
        "angulo_servo": angulo,
        "temperatura": temp
    }
    
    response = requests.post(
        f"{SUPABASE_URL}/rest/v1/solar_data",
        json=data,
        headers=headers
    )
    
    print(f"Status: {response.status_code}")
    return response.json()
```

### **Paso 5: Visualizar Datos**

En el dashboard de Supabase:

1. Click "Table Editor"
2. Seleccionar tabla "solar_data"
3. Ver datos insertados en tiempo real

**Crear gráfica:**
1. Click "SQL Editor"
2. Ejecutar:
```sql
SELECT 
    DATE_TRUNC('hour', created_at) as hora,
    AVG(voltaje) as voltaje_promedio,
    AVG(potencia) as potencia_promedio,
    COUNT(*) as mediciones
FROM solar_data
WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '24 hours'
GROUP BY DATE_TRUNC('hour', created_at)
ORDER BY hora DESC;
```

---

## 📱 TELEGRAM BOT

### **¿Qué es un Bot de Telegram?**

Un bot que recibe comandos (ej: `/status`) y responde con información del rastreador.

**Ventajas:**
- ✅ Interfaz móvil
- ✅ Notificaciones push
- ✅ Control remoto del servo
- ✅ Consultar datos en vivo

### **Paso 1: Crear Bot**

1. Abrir Telegram
2. Buscar **@BotFather**
3. Enviar `/newbot`
4. Elegir nombre (ej: `SolarTrackerBot`)
5. Elegir nombre usuario (ej: `@solar_tracker_bot`)
6. **Recibirás el TOKEN:** `123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11`

**Guardar en `config.h`:**
```c
#define TELEGRAM_BOT_TOKEN "123456:ABC-DEF1234..."
```

### **Paso 2: Obtener Chat ID**

1. Hablar con tu bot (enviar mensaje)
2. Ir a esta URL (en navegador):
```
https://api.telegram.org/bot[TU_TOKEN]/getUpdates
```

3. Ver respuesta JSON, buscar:
```json
"chat":{"id":987654321}
```

4. Copiar ese ID (ej: `987654321`)

**Guardar en `config.h`:**
```c
#define TELEGRAM_CHAT_ID "987654321"
```

### **Paso 3: Implementar Bot en ESP32**

En `main/telegram_bot.c`:

```c
#include "telegram_bot.h"
#include "config.h"
#include "esp_http_client.h"
#include <cJSON.h>
#include <esp_log.h>

static const char *TAG = "TELEGRAM";
static long last_update_id = 0;

void telegram_send_message(const char *message) {
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Construir URL
    char url[512];
    snprintf(url, sizeof(url), 
        "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s",
        TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, message);
    
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✓ Mensaje enviado a Telegram");
    } else {
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}

void telegram_check_commands(int *angulo_servo) {
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Obtener updates
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.telegram.org/bot%s/getUpdates?offset=%ld",
        TELEGRAM_BOT_TOKEN, last_update_id + 1);
    
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }
    
    int content_length = esp_http_client_get_content_length(client);
    char *buffer = malloc(content_length + 1);
    esp_http_client_read_response(client, (uint8_t *)buffer, content_length);
    buffer[content_length] = 0;
    
    // Parsear JSON
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        ESP_LOGE(TAG, "JSON inválido");
        free(buffer);
        esp_http_client_cleanup(client);
        return;
    }
    
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (cJSON_IsArray(result)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, result) {
            cJSON *update_id = cJSON_GetObjectItem(item, "update_id");
            cJSON *message = cJSON_GetObjectItem(item, "message");
            cJSON *text = cJSON_GetObjectItem(message, "text");
            
            if (text && text->valuestring) {
                ESP_LOGI(TAG, "Comando recibido: %s", text->valuestring);
                
                // Procesar comandos
                if (strcmp(text->valuestring, "/status") == 0) {
                    // Enviar estado
                    extern float global_voltaje;
                    extern float global_bateria;
                    extern int global_luz;
                    extern int global_angulo;
                    
                    char respuesta[256];
                    snprintf(respuesta, sizeof(respuesta),
                        "⚡ RASTREADOR SOLAR\n"
                        "Voltaje: %.2fV\n"
                        "Batería: %.0f%%\n"
                        "Luz: %d\n"
                        "Ángulo: %d°",
                        global_voltaje, global_bateria,
                        global_luz, global_angulo);
                    
                    telegram_send_message(respuesta);
                }
                else if (strcmp(text->valuestring, "/manual_0") == 0) {
                    *angulo_servo = 0;
                    telegram_send_message("🔄 Servo a 0°");
                }
                else if (strcmp(text->valuestring, "/manual_90") == 0) {
                    *angulo_servo = 90;
                    telegram_send_message("🔄 Servo a 90°");
                }
                else if (strcmp(text->valuestring, "/manual_180") == 0) {
                    *angulo_servo = 180;
                    telegram_send_message("🔄 Servo a 180°");
                }
                else if (strcmp(text->valuestring, "/help") == 0) {
                    telegram_send_message(
                        "/status - Ver estado\n"
                        "/manual_0 - Girar a 0°\n"
                        "/manual_90 - Girar a 90°\n"
                        "/manual_180 - Girar a 180°");
                }
                
                last_update_id = update_id->valueint;
            }
        }
    }
    
    cJSON_Delete(root);
    free(buffer);
    esp_http_client_cleanup(client);
}

void task_telegram_bot(void *pvParameters) {
    while(1) {
        extern int global_angulo;
        telegram_check_commands(&global_angulo);
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);  // Cada 5 segundos
    }
}
```

### **Paso 4: Comandos Disponibles**

**Mensajes que puedes enviar al bot:**

```
/status       → Ver estado del rastreador
/manual_0     → Girar servo a 0°
/manual_90    → Girar servo a 90° (centro)
/manual_180   → Girar servo a 180°
/help         → Mostrar ayuda
```

**Respuesta esperada:**
```
⚡ RASTREADOR SOLAR
Voltaje: 3.25V
Batería: 85%
Luz: 3500
Ángulo: 90°
```

---

## 🔐 SEGURIDAD

### **Cuidados Importantes**

1. **No compartir credenciales:**
   - ANON_KEY y BOT_TOKEN son secretos
   - No subirlos a GitHub públicamente

2. **Usar variables de entorno:**
   ```bash
   export SUPABASE_KEY="eyJhbGc..."
   export TELEGRAM_TOKEN="123456:ABC..."
   ```

3. **En producción:**
   - Usar Row Level Security (RLS) en Supabase
   - Limitar acceso a la tabla solar_data

### **Row Level Security en Supabase**

```sql
-- Solo permitir inserción, no actualización/borrado
CREATE POLICY "Insert only" ON solar_data
    FOR INSERT TO anon WITH CHECK (true);

CREATE POLICY "Read only" ON solar_data
    FOR SELECT TO anon USING (true);

-- Denegar UPDATE y DELETE
ALTER TABLE solar_data ENABLE ROW LEVEL SECURITY;
```

---

## 📈 DASHBOARD PYTHON (Visualización)

Si quieres graficar datos en tu PC:

```python
import matplotlib.pyplot as plt
import pandas as pd
from supabase import create_client
import os

# Conectar a Supabase
url = os.getenv("SUPABASE_URL")
key = os.getenv("SUPABASE_KEY")
supabase = create_client(url, key)

# Obtener datos
data = supabase.table("solar_data").select("*").execute()

# Convertir a DataFrame
df = pd.DataFrame(data.data)
df['created_at'] = pd.to_datetime(df['created_at'])

# Graficar
fig, axes = plt.subplots(2, 2, figsize=(12, 8))

# Voltaje vs Tiempo
axes[0, 0].plot(df['created_at'], df['voltaje'])
axes[0, 0].set_title('Voltaje del Panel')
axes[0, 0].set_ylabel('Voltios')

# Potencia
axes[0, 1].plot(df['created_at'], df['potencia'], color='orange')
axes[0, 1].set_title('Potencia Generada')
axes[0, 1].set_ylabel('Watts')

# Ángulo del servo
axes[1, 0].plot(df['created_at'], df['angulo_servo'], color='green')
axes[1, 0].set_title('Posición del Servo')
axes[1, 0].set_ylabel('Grados')

# Luz detectada
axes[1, 1].plot(df['created_at'], df['luz_raw'], color='red')
axes[1, 1].set_title('Intensidad de Luz')
axes[1, 1].set_ylabel('Valor ADC')

plt.tight_layout()
plt.show()
```

**Instalar dependencias:**
```bash
pip install supabase python-dotenv matplotlib pandas
```

---

## ✅ CHECKLIST DE CONFIGURACIÓN

- [ ] Crear cuenta Supabase
- [ ] Crear tabla `solar_data`
- [ ] Copiar URL y ANON_KEY
- [ ] Crear Bot Telegram con @BotFather
- [ ] Obtener TOKEN del bot
- [ ] Obtener CHAT_ID
- [ ] Agregar credenciales a `config.h`
- [ ] Compilar proyecto ESP32
- [ ] Descargar a ESP32
- [ ] Probar inserción de datos
- [ ] Verificar en tabla Supabase
- [ ] Enviar comando `/status` al bot
- [ ] Recibir respuesta de Telegram

---

**Versión:** 1.0  
**Actualización:** 2026-05-10
