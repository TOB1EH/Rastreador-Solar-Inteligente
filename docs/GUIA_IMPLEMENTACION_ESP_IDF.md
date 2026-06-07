# 💻 GUÍA DE IMPLEMENTACIÓN - ESP-IDF + FREERTOS

**Para:** Rastreador Solar Inteligente Eje Único  
**Entorno:** Ubuntu + VSCode + ESP-IDF  
**Objetivo:** Implementación paso a paso del firmware

---

## 📦 INSTALACIÓN INICIAL

### **Paso 1: Instalar ESP-IDF**

```bash
# En tu home (~/)
cd ~
mkdir esp
cd esp

# Clonar ESP-IDF
git clone https://github.com/espressif/esp-idf.git

# Entrar a carpeta
cd esp-idf

# Ejecutar instalador (descarga toolchain)
./install.sh esp32

# Activar environment
source export.sh
```

**Verificar instalación:**
```bash
idf.py --version
# Debería mostrar versión (ej: ESP-IDF v5.0.0)
```

### **Paso 2: Instalar VSCode y Extensiones**

```bash
# Si no está instalado
sudo apt install code

# Abrir VSCode e instalar:
# 1. ESP-IDF Extension (por Espressif)
# 2. C/C++ IntelliSense (Microsoft)
# 3. Serial Monitor (Microsoft)
```

### **Paso 3: Crear Proyecto Base**

```bash
# En tu workspace
cd ~/Documentos/OneDrive/workSpace/Rastreador_Solar_Inteligente

# Crear proyecto
idf.py create-project rastreador_solar

# Entrar
cd rastreador_solar
```

---

## 🏗️ ESTRUCTURA DEL PROYECTO

```
rastreador_solar/
├── CMakeLists.txt                    # Build configuration
├── idf_component.yml                 # Dependencias
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── main.c                        # Punto de entrada
│   ├── config.h                      # Configuración de pines
│   ├── sensor_ldr.c/h                # Lectura LDR
│   ├── servo_motor.c/h               # Control servo
│   ├── display_lcd.c/h               # Display LCD
│   ├── wifi_manager.c/h              # WiFi
│   ├── supabase_client.c/h           # Supabase
│   └── telegram_bot.c/h              # Telegram
└── build/                            # Output (generado)
```

---

## 📝 ARCHIVOS PRINCIPALES

### **1. main/config.h**

```c
#ifndef CONFIG_H
#define CONFIG_H

// ========== GPIO PINS ==========
#define GPIO_LDR_ADC        ADC1_CHANNEL_0    // GPIO36
#define GPIO_SERVO_PIN      13                // GPIO13 PWM

#define GPIO_LCD_SDA        21                // I2C
#define GPIO_LCD_SCL        22                // I2C
#define LCD_I2C_ADDRESS     0x27              // Típico: 0x27 o 0x3F

#define GPIO_TEMP_SENSOR    25                // DS18B20 (1-Wire)
#define GPIO_VOLTAGE_ADC    ADC1_CHANNEL_6   // GPIO34
#define GPIO_CURRENT_ADC    ADC1_CHANNEL_7   // GPIO35

// ========== ALGORITMO ==========
#define LDR_THRESHOLD_PCT   95                // Mantener si está al 95% del máximo
#define SERVO_STEP_DEGREES  10                // Incremento de búsqueda
#define SERVO_MIN_ANGLE     0
#define SERVO_MAX_ANGLE     180

// ========== WIFI ==========
#define WIFI_SSID          "TU_SSID"
#define WIFI_PASSWORD      "TU_PASSWORD"

// ========== SUPABASE ==========
#define SUPABASE_URL        "https://project-id.supabase.co"
#define SUPABASE_ANON_KEY   "eyJhbGc..."

// ========== TELEGRAM ==========
#define TELEGRAM_BOT_TOKEN  "123456:ABC..."
#define TELEGRAM_CHAT_ID    "987654321"

// ========== FREERTOS STACKS ==========
#define STACK_SIZE_SENSOR   2048
#define STACK_SIZE_SERVO    2048
#define STACK_SIZE_DISPLAY  2048
#define STACK_SIZE_WIFI     4096

// ========== TIMINGS ==========
#define TASK_SENSOR_PERIOD_MS   100
#define TASK_SERVO_PERIOD_MS    200
#define TASK_DISPLAY_PERIOD_MS  1000
#define TASK_WIFI_PERIOD_MS     10000

#endif // CONFIG_H
```

---

### **2. main/sensor_ldr.h**

```c
#ifndef SENSOR_LDR_H
#define SENSOR_LDR_H

#include <stdint.h>

// Inicializar ADC para LDR
void sensor_ldr_init(void);

// Leer valor del LDR (0-4095)
uint16_t sensor_ldr_read_raw(void);

// Leer valor filtrado
uint16_t sensor_ldr_read_filtered(void);

// Task de lectura (llamada desde FreeRTOS)
void task_read_sensor(void *pvParameters);

#endif
```

---

### **3. main/sensor_ldr.c**

```c
#include "sensor_ldr.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

static const char *TAG = "SENSOR_LDR";
static adc_oneshot_unit_handle_t adc1_handle;

// Variables globales compartidas
extern int global_luz;
extern QueueHandle_t queue_luz;
extern QueueHandle_t queue_buscar_luz;

static uint16_t luz_anterior = 0;
static uint16_t luz_maxima = 0;

void sensor_ldr_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,  // Rango 0-3.6V
    };
    
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, 
                                               GPIO_LDR_ADC, 
                                               &config));
    
    ESP_LOGI(TAG, "LDR sensor iniciado en GPIO36");
}

uint16_t sensor_ldr_read_raw(void) {
    int adc_reading = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, GPIO_LDR_ADC, &adc_reading));
    return (uint16_t)adc_reading;
}

uint16_t sensor_ldr_read_filtered(void) {
    uint16_t raw = sensor_ldr_read_raw();
    
    // Filtro exponencial móvil: 70% anterior + 30% nuevo
    uint16_t filtered = (luz_anterior * 70 + raw * 30) / 100;
    luz_anterior = filtered;
    
    return filtered;
}

void task_read_sensor(void *pvParameters) {
    sensor_ldr_init();
    
    uint16_t luz_actual = 0;
    int debe_buscar = 1;
    
    while(1) {
        // Leer y filtrar
        luz_actual = sensor_ldr_read_filtered();
        global_luz = luz_actual;
        
        // Comparar con máximo
        if (luz_actual > (luz_maxima * 95 / 100)) {
            // Luz suficiente
            luz_maxima = luz_actual;
            debe_buscar = 0;
            xQueueSend(queue_luz, &luz_actual, 0);
            
            ESP_LOGD(TAG, "LUZ OK: %d (max: %d)", luz_actual, luz_maxima);
        } else {
            // Luz bajó, buscar nueva posición
            if (!debe_buscar) {
                debe_buscar = 1;
                xQueueSend(queue_buscar_luz, &luz_actual, 0);
                ESP_LOGI(TAG, "BUSCAR nueva luz (anterior: %d, actual: %d)", 
                         luz_maxima, luz_actual);
            }
        }
        
        vTaskDelay(TASK_SENSOR_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
```

---

### **4. main/servo_motor.h**

```c
#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H

#include <stdint.h>

// Inicializar PWM para servo
void servo_init(void);

// Establecer ángulo del servo (0-180 grados)
void servo_set_angle(int angle);

// Obtener ángulo actual
int servo_get_angle(void);

// Task de control de servo
void task_control_servo(void *pvParameters);

#endif
```

---

### **5. main/servo_motor.c**

```c
#include "servo_motor.h"
#include "config.h"
#include "esp_pwm_cmt.h"  // O esp_ledc.h para LEDC PWM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <math.h>

static const char *TAG = "SERVO_MOTOR";

extern int global_luz;
extern int global_angulo;
extern QueueHandle_t queue_buscar_luz;

static int angulo_actual = 90;
static int angulo_anterior = 90;

// Convertir ángulo (0-180) a duty cycle PWM (1000-2000 µs en LEDC)
static uint32_t angle_to_pulse_width(int angle) {
    // SG90: 1000µs = 0°, 1500µs = 90°, 2000µs = 180°
    // Con LEDC a 50Hz: duty = (pulse_width / 20000) * 1023
    
    uint32_t pulse_us = 1000 + (angle * 1000 / 180);  // 1000-2000 µs
    uint32_t duty = (pulse_us * 1023) / 20000;         // Escala 0-1023
    
    return duty;
}

void servo_init(void) {
    // Configurar LEDC PWM
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,   // 10 bits (0-1023)
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,                          // 50Hz para servo
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);
    
    // Configurar canal PWM
    ledc_channel_config_t ch_conf = {
        .gpio_num = GPIO_SERVO_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = angle_to_pulse_width(90),       // Comenzar en 90°
        .hpoint = 0
    };
    ledc_channel_config(&ch_conf);
    
    ESP_LOGI(TAG, "Servo inicializado en GPIO%d", GPIO_SERVO_PIN);
}

void servo_set_angle(int angle) {
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    
    uint32_t duty = angle_to_pulse_width(angle);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    
    angulo_actual = angle;
    global_angulo = angle;
    
    ESP_LOGD(TAG, "Servo movido a: %d°", angle);
}

int servo_get_angle(void) {
    return angulo_actual;
}

void task_control_servo(void *pvParameters) {
    servo_init();
    
    int luz_recibida = 0;
    int nueva_luz = 0;
    int intento_angle = 0;
    uint16_t luz_actual = 0;
    
    while(1) {
        // ¿Hay comando de búsqueda?
        if (xQueueReceive(queue_buscar_luz, &luz_recibida, 0)) {
            ESP_LOGI(TAG, "Iniciando búsqueda de luz desde ángulo %d°", 
                     angulo_actual);
            
            // ==================== ALGORITMO DE BÚSQUEDA ====================
            // Probar rotar +10 grados
            intento_angle = angulo_actual + SERVO_STEP_DEGREES;
            if (intento_angle <= SERVO_MAX_ANGLE) {
                servo_set_angle(intento_angle);
                vTaskDelay(500 / portTICK_PERIOD_MS);  // Esperar movimiento
                
                // Leer luz en nueva posición
                // (En producción, sería mejor promediar múltiples lecturas)
                nueva_luz = global_luz;
                
                if (nueva_luz > luz_recibida * 1.05) {  // 5% mejora
                    ESP_LOGI(TAG, "✓ Mejor en +%d°: %d vs %d", 
                             SERVO_STEP_DEGREES, nueva_luz, luz_recibida);
                    angulo_anterior = intento_angle;
                    continue;  // Esperar próxima iteración
                }
            }
            
            // No mejoró, probar -10 grados (2 veces: -10 y -20)
            intento_angle = angulo_anterior - SERVO_STEP_DEGREES;
            if (intento_angle >= SERVO_MIN_ANGLE) {
                servo_set_angle(intento_angle);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
                nueva_luz = global_luz;
                if (nueva_luz > luz_recibida * 1.05) {
                    ESP_LOGI(TAG, "✓ Mejor en -%d°: %d vs %d", 
                             SERVO_STEP_DEGREES, nueva_luz, luz_recibida);
                    angulo_anterior = intento_angle;
                    continue;
                }
            }
            
            // Seguir probando -20 grados
            intento_angle = angulo_anterior - 2 * SERVO_STEP_DEGREES;
            if (intento_angle >= SERVO_MIN_ANGLE) {
                servo_set_angle(intento_angle);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
                nueva_luz = global_luz;
                if (nueva_luz > luz_recibida * 1.05) {
                    ESP_LOGI(TAG, "✓ Mejor en -%d°: %d vs %d", 
                             2*SERVO_STEP_DEGREES, nueva_luz, luz_recibida);
                    angulo_anterior = intento_angle;
                    continue;
                }
            }
            
            // Ninguna dirección mejoró, volver al centro
            ESP_LOGW(TAG, "No encontró mejora, volviendo a 90°");
            servo_set_angle(90);
            angulo_anterior = 90;
            // ================================================================
        }
        
        vTaskDelay(TASK_SERVO_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
```

---

### **6. main/main.c**

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

// Headers de módulos
#include "config.h"
#include "sensor_ldr.h"
#include "servo_motor.h"

static const char *TAG = "MAIN";

// ========== VARIABLES GLOBALES ==========
int global_luz = 0;
int global_angulo = 90;
float global_voltaje = 0.0;
float global_bateria = 100.0;
float global_corriente = 0.0;
float global_temperatura = 25.0;

// ========== QUEUES ==========
QueueHandle_t queue_luz;
QueueHandle_t queue_buscar_luz;

void app_main(void) {
    ESP_LOGI(TAG, "===== RASTREADOR SOLAR INICIANDO =====");
    
    // Crear colas FreeRTOS
    queue_luz = xQueueCreate(5, sizeof(int));
    queue_buscar_luz = xQueueCreate(5, sizeof(int));
    
    // Crear tareas FreeRTOS
    xTaskCreatePinnedToCore(task_read_sensor,
                           "TASK_SENSOR",
                           STACK_SIZE_SENSOR,
                           NULL,
                           3,      // Prioridad ALTA
                           NULL,
                           0);     // Core 0
    
    xTaskCreatePinnedToCore(task_control_servo,
                           "TASK_SERVO",
                           STACK_SIZE_SERVO,
                           NULL,
                           3,      // Prioridad ALTA
                           NULL,
                           1);     // Core 1
    
    // (Otros tasks para LCD, WiFi, etc.)
    
    ESP_LOGI(TAG, "===== TODAS LAS TAREAS CREADAS =====");
}
```

---

## 🔨 COMPILACIÓN Y DESCARGA

### **1. Configurar ESP32**

```bash
# En carpeta del proyecto rastreador_solar/
idf.py set-target esp32
```

### **2. Configuración Inicial**

```bash
idf.py menuconfig
# Navegar: Component config → ESP32-specific
# Buscar ajustes de memoria/WiFi si es necesario
# Guardar y salir
```

### **3. Compilar**

```bash
idf.py build
# Debería completarse sin errores
```

### **4. Descargar en ESP32**

```bash
# Conectar ESP32 por USB
# Identificar puerto
ls /dev/ttyUSB*  # Típicamente /dev/ttyUSB0

# Descargar
idf.py -p /dev/ttyUSB0 flash

# Ver monitor de salida
idf.py -p /dev/ttyUSB0 monitor
```

### **5. Salida Esperada**

```
I (234) MAIN: ===== RASTREADOR SOLAR INICIANDO =====
I (245) SERVO_MOTOR: Servo inicializado en GPIO13
I (256) SENSOR_LDR: LDR sensor iniciado en GPIO36
I (267) MAIN: ===== TODAS LAS TAREAS CREADAS =====
I (278) SENSOR_LDR: LUZ OK: 2345 (max: 2345)
I (500) SERVO_MOTOR: BUSCAR nueva luz (anterior: 2345, actual: 1234)
...
```

---

## 🐛 TROUBLESHOOTING

| Problema | Solución |
|----------|----------|
| **Permiso denegado /dev/ttyUSB0** | `sudo usermod -a -G dialout $USER` y reiniciar sesión |
| **Error: "python3: No such file"** | `sudo apt install python3 python3-pip` |
| **Servo no se mueve** | Verificar GPIO (debe ser 13) y PWM frecuencia (50Hz) |
| **LDR siempre 0** | Verificar divisor de tensión y conexión ADC |
| **Compilation error: "esp_ledc.h not found"** | Verificar IDF_PATH: `source ~/esp/esp-idf/export.sh` |

---

## 📚 PRÓXIMOS PASOS

1. ✅ Implementar `display_lcd.c` (I2C LCD)
2. ✅ Implementar `wifi_manager.c` (Conexión WiFi)
3. ✅ Implementar `supabase_client.c` (HTTP POST)
4. ✅ Implementar `telegram_bot.c` (Bot API)
5. 🧪 Pruebas de integración
6. 📊 Análisis de eficiencia

---

**Versión:** 1.0 - Guía Básica  
**Actualización:** 2026-05-10
