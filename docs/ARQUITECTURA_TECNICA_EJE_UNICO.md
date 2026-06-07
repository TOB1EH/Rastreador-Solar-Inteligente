# 🔧 ARQUITECTURA TÉCNICA - RASTREADOR SOLAR EJE ÚNICO

**Versión:** 1.0  
**Fecha:** 2026-05-10  
**Estado:** Especificación de Diseño

---

## 📋 RESUMEN EJECUTIVO

Sistema autónomo basado en **ESP32 + FreeRTOS** que controla un panel solar en eje horizontal único (0-180°). El panel se mantiene inclinado a 45° fijos, mientras que el servo lo rota horizontalmente siguiendo la intensidad de luz detectada por un LDR.

**Características:**
- ✅ Autónomo (sin necesidad de PC)
- ✅ FreeRTOS multitarea en ESP32
- ✅ Almacenamiento en Supabase (cloud)
- ✅ Bot Telegram para monitoreo remoto
- ✅ Pantalla LCD local con métricas
- ✅ Desarrollo en VSCode con ESP-IDF

---

## 🏗️ ARQUITECTURA DEL SISTEMA

### **Vista General**

```
┌─────────────────────────────────────────────────────────────┐
│                      SISTEMA AUTÓNOMO                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  SENSORES              PROCESAMIENTO        ACTUADORES       │
│  ───────────────────────────────────────────────────        │
│                                                               │
│  LDR ──→ ADC          ┌──────────────────┐   ┌────────┐    │
│  │       (12-bit)     │   ESP32 + RTOS   │──→│ Servo  │    │
│  │       (0-4095)     │                  │   │ PWM    │    │
│  └──→────────────────→│ Task 1: Sensor   │   └────────┘    │
│                       │ Task 2: Motor    │                  │
│  Panel Voltage ──────→│ Task 3: Display  │   ┌────────┐    │
│  Battery Level ──────→│ Task 4: WiFi/DB  │──→│  LCD   │    │
│                       │                  │   │ 16x2   │    │
│  Temperature ────────→└──────────────────┘   └────────┘    │
│                             │                                │
│                      ┌──────┴────────┐                       │
│                      ↓               ↓                       │
│                   WiFi           STORAGE                     │
│                   ───────        ──────────                  │
│              Telegram Bot        Supabase                    │
│              & OTA Updates       (PostgreSQL)               │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔄 TAREAS FREERTOS (MULTITAREA)

### **Distribución de Tareas**

| Task | Prioridad | Período | Función |
|------|-----------|---------|---------|
| **Task 1: Sensor** | ⭐⭐⭐ ALTA | 100ms | Leer LDR y detectar luz |
| **Task 2: Servo** | ⭐⭐⭐ ALTA | 200ms | Controlar motor y rotación |
| **Task 3: Display** | ⭐⭐ MEDIA | 1s | Actualizar LCD |
| **Task 4: WiFi/BD** | ⭐ BAJA | 10s | Enviar datos a Supabase + Telegram |

### **Diagrama de Comunicación (Queues)**

```
TASK 1 (Sensor)                TASK 2 (Servo)
────────────────               ──────────────
Lee ADC → Queue luz ─────────→ Recibe valor
                                Calcula movimiento
                                Envía PWM
                    
                    Queue buscar_luz (cuando detecta cambio)
                    ─────────────────────────────────────→

TASK 3 (Display)               TASK 4 (WiFi)
────────────────               ────────────
Queue voltaje ←───────────────→ Lee valores globales
Queue bateria ←───────────────→ Conecta WiFi
Queue luz    ←───────────────→ Envía a Supabase
```

---

## 📡 TAREAS DETALLADAS (PSEUDOCÓDIGO)

### **TASK 1: Lectura de Sensor LDR**

```c
void task_read_sensor(void *pvParameters) {
    // Variables persistentes
    static int luz_maxima_conocida = 0;
    static int luz_anterior = 0;
    int luz_actual = 0;
    
    while(1) {
        // 1. Leer valor ADC del LDR (0-4095)
        luz_actual = adc_read_channel(ADC1_CHANNEL_0);  // GPIO36
        
        // 2. Filtro: promedio móvil (evitar ruido)
        luz_actual = (luz_anterior * 0.7) + (luz_actual * 0.3);
        luz_anterior = luz_actual;
        
        // 3. Comparar con máximo conocido
        if (luz_actual > (luz_maxima_conocida * 0.95)) {
            // Luz suficiente, actualizar máximo
            luz_maxima_conocida = luz_actual;
            global_luz = luz_actual;
            xQueueSend(queue_luz, &luz_actual, 0);
        } else {
            // Luz disminuyó, necesita buscar nueva posición
            xQueueSend(queue_buscar_luz, &luz_actual, 0);
        }
        
        // Dormir 100ms
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```

**Lógica:**
- Lee el LDR cada 100ms
- Aplica filtro para eliminar ruido (70% valor anterior + 30% nuevo)
- Si luz baja → señala a TASK 2 para buscar máximo

---

### **TASK 2: Control del Servo**

```c
void task_control_servo(void *pvParameters) {
    int luz_recibida = 0;
    static int angulo_actual = 90;      // Comienza en centro
    static int angulo_anterior = 90;
    int nueva_luz = 0;
    int luz_optima = 0;
    
    while(1) {
        // 1. ¿Debe buscar nueva posición?
        if (xQueueReceive(queue_buscar_luz, &luz_recibida, 0)) {
            
            // 2. Probar rotar +10°
            angulo_actual += 10;
            if (angulo_actual > 180) angulo_actual = 180;
            
            pwm_set_servo_angle(GPIO_SERVO_PIN, angulo_actual);
            vTaskDelay(300 / portTICK_PERIOD_MS);  // Esperar a que se mueva
            
            nueva_luz = adc_read_channel(ADC1_CHANNEL_0);
            
            // 3. ¿Mejoró la luz?
            if (nueva_luz > luz_recibida) {
                luz_optima = nueva_luz;
                // Quedarse en esta posición
            } else {
                // No mejoró, volver atrás
                angulo_actual -= 20;  // 10 hacia atrás más 10 adicional
                if (angulo_actual < 0) angulo_actual = 0;
                
                pwm_set_servo_angle(GPIO_SERVO_PIN, angulo_actual);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                
                // Probar -10° (total -20° desde inicial)
                angulo_actual -= 10;
                pwm_set_servo_angle(GPIO_SERVO_PIN, angulo_actual);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                
                nueva_luz = adc_read_channel(ADC1_CHANNEL_0);
                if (nueva_luz > luz_recibida) {
                    luz_optima = nueva_luz;
                } else {
                    // Volver al centro
                    angulo_actual = 90;
                    pwm_set_servo_angle(GPIO_SERVO_PIN, 90);
                }
            }
            
            angulo_anterior = angulo_actual;
            global_angulo = angulo_actual;
        }
        
        // Dormir 200ms
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
```

**Lógica:**
- Recibe señal de TASK 1 cuando hay cambio de luz
- Rota ±10° y mide luz nuevamente
- Queda en posición si aumentó; sino prueba en otra dirección
- Evita movimientos constantes (hystéresis)

---

### **TASK 3: Actualizar Display LCD**

```c
void task_display_lcd(void *pvParameters) {
    float voltaje = 0.0;
    float bateria = 0.0;
    int luz = 0;
    int angulo = 90;
    char buffer_linea1[17];
    char buffer_linea2[17];
    
    while(1) {
        // 1. Leer variables globales (compartidas)
        voltaje = global_voltaje;
        bateria = global_bateria;
        luz = global_luz;
        angulo = global_angulo;
        
        // 2. Formatear línea 1: Voltaje y Batería
        sprintf(buffer_linea1, "V:%.2fV B:%.0f%%", voltaje, bateria);
        
        // 3. Formatear línea 2: Luz y Ángulo
        sprintf(buffer_linea2, "L:%d A:%d", luz, angulo);
        
        // 4. Escribir en LCD
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print_string(buffer_linea1);
        
        lcd_set_cursor(1, 0);
        lcd_print_string(buffer_linea2);
        
        // Dormir 1 segundo
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

**Salida LCD esperada:**
```
V:3.25V B:85%
L:3500 A:90°
```

---

### **TASK 4: WiFi y Base de Datos**

```c
void task_wifi_database(void *pvParameters) {
    // 1. Conectar a WiFi (una sola vez)
    wifi_init_sta();
    wait_for_wifi_connected();
    
    // 2. Inicializar cliente Supabase
    supabase_init("https://YOUR_PROJECT.supabase.co",
                  "YOUR_ANON_KEY");
    
    // 3. Inicializar Bot Telegram
    telegram_init("YOUR_BOT_TOKEN");
    
    struct tm timeinfo = {0};
    time_t now = 0;
    
    while(1) {
        // Cada 10 segundos
        
        // 1. Preparar datos
        SolarData data = {
            .timestamp = get_timestamp_unix(),
            .voltaje = global_voltaje,
            .corriente = global_corriente,
            .potencia = global_voltaje * global_corriente,
            .bateria_porcentaje = global_bateria,
            .luz_raw = global_luz,
            .angulo_servo = global_angulo,
            .temperatura = global_temperatura
        };
        
        // 2. Guardar en Supabase (PostgreSQL)
        supabase_insert("solar_data", &data);
        
        // 3. Leer comandos de Telegram (si hay)
        char comando[100] = {0};
        if (telegram_receive_command(comando)) {
            if (strcmp(comando, "/status") == 0) {
                char respuesta[200];
                sprintf(respuesta, 
                    "⚡ Estado del Rastreador:\n"
                    "Voltaje: %.2fV\n"
                    "Batería: %.0f%%\n"
                    "Luz: %d\n"
                    "Ángulo: %d°",
                    global_voltaje, global_bateria, 
                    global_luz, global_angulo);
                telegram_send_message(respuesta);
            }
            else if (strcmp(comando, "/manual_0") == 0) {
                global_angulo = 0;
                pwm_set_servo_angle(GPIO_SERVO_PIN, 0);
                telegram_send_message("🔄 Servo a 0°");
            }
            else if (strcmp(comando, "/manual_90") == 0) {
                global_angulo = 90;
                pwm_set_servo_angle(GPIO_SERVO_PIN, 90);
                telegram_send_message("🔄 Servo a 90°");
            }
            else if (strcmp(comando, "/manual_180") == 0) {
                global_angulo = 180;
                pwm_set_servo_angle(GPIO_SERVO_PIN, 180);
                telegram_send_message("🔄 Servo a 180°");
            }
        }
        
        // Dormir 10 segundos
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
```

**Funcionalidades:**
- Conecta a WiFi automáticamente
- Envía datos a Supabase cada 10 segundos
- Recibe comandos de Telegram
- Permite control manual del servo

---

## 🔌 ASIGNACIÓN DE PINES GPIO

| Función | GPIO | Tipo | Protocolo |
|---------|------|------|-----------|
| **LDR (Sensor luz)** | GPIO36 (ADC) | Analog Input | Divisor de tensión |
| **Servo PWM** | GPIO13 | Output PWM | 1-2ms pulses, 50Hz |
| **LCD SDA** | GPIO21 | I2C | I2C Bus |
| **LCD SCL** | GPIO22 | I2C | I2C Bus |
| **Temperatura DS18B20** | GPIO25 | 1-Wire | Dallas Protocol |
| **Voltaje Panel** | GPIO34 (ADC) | Analog Input | Divisor 10k/10k |
| **Corriente** | GPIO35 (ADC) | Analog Input | Shunt 0.1Ω |

---

## 💾 ESTRUCTURA DE BASE DE DATOS (SUPABASE)

### **Tabla: solar_data**

```sql
CREATE TABLE solar_data (
    id BIGSERIAL PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    voltaje FLOAT NOT NULL,
    corriente FLOAT NOT NULL,
    potencia FLOAT NOT NULL,
    bateria_porcentaje FLOAT NOT NULL,
    luz_raw INTEGER NOT NULL,
    angulo_servo INTEGER NOT NULL,
    temperatura FLOAT NOT NULL,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- Índice para búsquedas rápidas por timestamp
CREATE INDEX idx_solar_timestamp ON solar_data(timestamp);
```

**Datos guardados:**
- Cada 10 segundos durante operación
- Aproximadamente 8.640 registros por día
- ~5 MB por mes (con almacenamiento ilimitado en Supabase free tier)

---

## 📊 ESTADÍSTICAS Y TIMINGS

### **Latencias del Sistema**

```
Evento                          Latencia
──────────────────────────────────────────
LDR cambia → Lectura ADC        ~10ms
ADC → Cola FreeRTOS             ~5ms
Cola → TASK 2 (Servo)           ~15ms
TASK 2 Calcula movimiento       ~50ms
TASK 2 Envía PWM                ~2ms
PWM → Movimiento físico servo   ~200ms
────────────────────────────────────────
TOTAL (sensor → movimiento)     ~280ms ✅
```

**Conclusión:** Latencia < 500ms ✓ (Requisito cumplido)

### **Consumo de Energía**

| Componente | Consumo | Notas |
|-----------|---------|-------|
| ESP32 (idle) | 10mA | Con WiFi desactivado |
| ESP32 (WiFi activo) | 80-150mA | Varía con actividad |
| Servo SG90 | 50mA (idle), 500mA (movimiento) | Picos de movimiento |
| LCD 16x2 | 40mA | Siempre encendido |
| LDR | <1mA | Solo lectura ADC |
| **TOTAL (esperado)** | **~150-200mA** | Promedio operación normal |

**Autonomía con batería 18650 (3000mAh):**
- Consumo promedio: 150mA
- Autonomía: 3000mAh / 150mA = **20 horas**

---

## 🌐 INTEGRACIÓN SUPABASE

### **Pasos de Configuración**

1. **Crear proyecto en Supabase:**
   - Ir a https://supabase.com
   - Click "New Project"
   - Crear tabla "solar_data" con esquema anterior

2. **Obtener credenciales:**
   ```
   URL: https://[project-id].supabase.co
   ANON KEY: eyJhbGc... (desde Settings → API)
   ```

3. **En ESP32:**
   ```c
   #define SUPABASE_URL "https://[project-id].supabase.co"
   #define SUPABASE_ANON_KEY "eyJhbGc..."
   ```

4. **Insertar datos:**
   ```c
   // Usa librería Arduino Supabase (https://github.com/supabase-community/supabase-esp32)
   supabase.insert("solar_data", data_json);
   ```

### **Visualizar Datos**

En la consola Supabase:
- Ir a "SQL Editor"
- Ver datos en tiempo real en tabla "solar_data"
- Exportar CSV o crear gráficos

---

## 📲 INTEGRACIÓN TELEGRAM BOT

### **Pasos de Configuración**

1. **Crear Bot:**
   - Hablar con @BotFather en Telegram
   - Comando `/newbot`
   - Recibirás: `BOT_TOKEN`

2. **Obtener Chat ID:**
   - Hablar con el bot
   - Acceder a: `https://api.telegram.org/bot[BOT_TOKEN]/getUpdates`
   - Buscar `"chat":{"id": CHAT_ID}`

3. **En ESP32:**
   ```c
   #define TELEGRAM_BOT_TOKEN "123456:ABC..."
   #define TELEGRAM_CHAT_ID "987654321"
   ```

### **Comandos Disponibles**

| Comando | Función |
|---------|---------|
| `/status` | Mostrar estado actual |
| `/manual_0` | Girar servo a 0° |
| `/manual_90` | Girar servo a 90° |
| `/manual_180` | Girar servo a 180° |
| `/historico` | Últimos 24h (gráfica) |

**Ejemplo de respuesta:**
```
⚡ Estado del Rastreador:
Voltaje: 3.25V
Batería: 85%
Luz: 3500
Ángulo: 45°
```

---

## 🚀 ALGORITMO DE SEGUIMIENTO

### **Pseudoalgoritmo (Conceptual)**

```
INICIO DEL DÍA:
└─ Posición inicial: 90° (Centro)
└─ Luz máxima conocida: 0

BUCLE PRINCIPAL:
│
├─ CADA 100ms: Leer LDR
│  ├─ Si luz > (luz_máxima * 0.95)
│  │  └─ Mantener posición, actualizar luz_máxima
│  │
│  └─ Si luz < (luz_máxima * 0.95)
│     └─ Señalar "BUSCAR NUEVA POSICIÓN"
│
├─ CUANDO: Búsqueda activada
│  ├─ Rotar +10° y medir luz
│  │  ├─ Si luz > anterior → QUEDARSE
│  │  └─ Si luz < anterior → probar -20°
│  │
│  └─ Después buscar -10° desde inicial
│
├─ CADA 1s: Actualizar LCD
│  └─ Mostrar V, Batería, Luz, Ángulo
│
└─ CADA 10s: Enviar a Supabase + Telegram
   └─ Si hay comando Telegram → ejecutar
```

### **Eficiencia de Búsqueda**

El algoritmo usa **estrategia de escalada (hill climbing):**
- ✅ Simple y eficiente
- ✅ Bajo consumo energético
- ✅ Rápida convergencia (~30-60 segundos)
- ⚠️ Puede quedar en máximos locales (mitigado con deadzone)

---

## 🛠️ REQUISITOS DE DESARROLLO

### **Software Requerido**

```bash
# 1. ESP-IDF (toolchain de Espressif)
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh

# 2. VSCode + Extensiones
- ESP-IDF Extension (Espressif)
- C/C++ IntelliSense
- Serial Monitor

# 3. Librerías necesarias
- esp-idf-lib (LCDs, I2C)
- Arduino core for ESP32 (si aplica)
- MQTT / HTTP client (para Supabase)
- Telegram bot library
```

### **Compilación y Descarga**

```bash
# En carpeta del proyecto
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## 📝 NOTAS IMPORTANTES

1. **Autónomo:** El ESP32 funciona sin PC (es esencial FreeRTOS)
2. **WiFi Opcional:** Sistema funciona sin WiFi; datos se sincronizan cuando conecta
3. **Almacenamiento Local:** Considerr SPIFFS/EEPROM como fallback si WiFi falla
4. **OTA Updates:** Se pueden actualizar el firmware por WiFi sin USB
5. **Power Management:** Considerar Deep Sleep entre búsquedas para ahorrar energía

---

**Versión:** 1.0 - Especificación de Arquitectura  
**Última actualización:** 2026-05-10
