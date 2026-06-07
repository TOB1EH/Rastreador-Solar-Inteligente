# 🚀 GUÍA DE INICIO RÁPIDO - RASTREADOR SOLAR INTELIGENTE

**Resumen ejecutivo:** Todo lo que necesitas para empezar el proyecto (hardware, software, pasos iniciales).

---

## 📦 LISTA DE COMPRA - HARDWARE

### Componentes Principales
| Componente | Cantidad | Especificación | Precio Aprox | Proveedores |
|-----------|----------|----------------|--------------|-------------|
| **ESP32** | 1 | DevKit v1 o DOIT | $15–20 | AliExpress, Amazon |
| **Servo Motor** | 1 | SG90 (180°, 5V) | $5–8 | AliExpress, Local |
| **Panel Solar** | 2 | 3V 100mA mini panel | $8–12 | AliExpress |
| **Batería Li-ion** | 2 | 18650 3000mAh | $10–15 | AliExpress, Local |
| **Cargador Li-ion** | 1 | TP4056 USB micro | $2–4 | AliExpress |
| **Módulo FZ0430** | 2 | Sensor Voltaje 0–25V | $12–16 | AliExpress |
| **LDR** | 1 | Fotoresistor 5–10mm | $1–2 | AliExpress, Local |
| **Resistor 10kΩ** | 1 | 1/4W | $0.50 | Local |
| **LCD 16×2** | 1 | Azul I2C PCF8574 | $4–6 | AliExpress |
| **Regulador 5V** | 1 | AMS1117-5.0 | $1–2 | AliExpress |
| **Regulador 3.3V** | 1 | AMS1117-3.3 | $1–2 | AliExpress |
| **Capacitores** | 5 | 100µF, 10µF, 1µF | $2 | Local |
| **Protoboard** | 1 | 830 puntos | $2–3 | AliExpress |
| **Cables Dupont** | 1 paquete | Macho-hembra | $2–3 | AliExpress |
| **Enclosure (opcional)** | 1 | Plástico 150×100mm | $5–10 | AliExpress |

**Costo total estimado: $80–120 USD**

---

### Sensores en Detalle

#### 1. **Módulos FZ0430** (2 unidades)
```
Función: Convertir voltaje 0–25V → 0–5V
Lectura: Panel solar (0–25V) + Batería (3.0–4.2V)
Precisión: ±0.1V (con compensación térmica automática)

Conexión:
  GND (negro)  → ESP32 GND
  IN (rojo)    → Panel/Batería (+)
  OUT (amarillo) → GPIO34 (panel) / GPIO35 (batería)
```

#### 2. **LDR** (1 unidad)
```
Función: Detectar intensidad luz solar
Rango: Luz solar directa (ADC ~3500–4095) a oscuridad (ADC ~0–500)

Conexión (divisor de voltaje):
  5V
   ├─ LDR → GPIO36 (ADC1_CHANNEL_0)
   └─ 10kΩ resistor → GND
```

#### 3. **Servo SG90** (1 unidad)
```
Función: Mover panel horizontal (0–180°)
Rango: 500µs (0°) a 2500µs (180°)

Conexión:
  Rojo (5V)  → 5V regulado
  Negro (GND) → GND
  Amarillo (PWM) → GPIO13
```

#### 4. **LCD 16×2 I2C** (1 unidad)
```
Función: Mostrar estado en tiempo real
Dirección I2C: 0x27 (configurable en PCF8574)

Conexión:
  GND → ESP32 GND
  VCC → 5V regulado
  SDA → GPIO21
  SCL → GPIO22
```

#### 5. **Baterías Li-ion 18650** (2 unidades)
```
Función: Alimentación autónoma 20+ horas
Configuración: En serie (7.4V) o paralelo (3.7V)
Protección: Usar cargador TP4056 con protección de sobrecarga
```

---

## 🖥️ REQUISITOS DE SOFTWARE

### En tu PC (Ubuntu/Linux)

#### 1. **ESP-IDF** (Framework de programación ESP32)
```bash
# Opción A: Instalación asistida (recomendado)
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
source ./export.sh

# Verificar instalación
idf.py --version
```

#### 2. **VSCode + Extensiones**
- Instala VSCode: `sudo apt install code`
- Extensión: **ESP-IDF Tools** (oficial Espressif)
- Extensión: **PlatformIO** (alternativa)

#### 3. **Dependencias de compilación**
```bash
sudo apt install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

### En el ESP32 (Firmware)

- **FreeRTOS**: Incluido en ESP-IDF (multitarea)
- **Drivers**: UART (para upload), I2C (LCD), SPI (opcional)
- **Librerías**: WiFi, HTTPS, JSON (integradas en IDF)

---

## 🔌 DIAGRAMA DE CONEXIONES RESUMIDO

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 DevKit                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ADC Inputs:                                               │
│  ├─ GPIO36 (ADC1_CH0) ← LDR + 10kΩ resistor ← 5V         │
│  ├─ GPIO34 (ADC1_CH6) ← FZ0430 OUT (Panel)                │
│  └─ GPIO35 (ADC1_CH7) ← FZ0430 OUT (Batería)              │
│                                                             │
│  PWM Outputs:                                              │
│  └─ GPIO13 → Servo SG90                                    │
│                                                             │
│  I2C (LCD 16×2):                                           │
│  ├─ GPIO21 (SDA)                                           │
│  └─ GPIO22 (SCL)                                           │
│                                                             │
│  Power:                                                     │
│  ├─ 5V (regulado) → Servo, LCD, FZ0430                    │
│  ├─ 3.3V (regulado) → ESP32 VCC                           │
│  └─ GND (común)                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────┐         ┌──────────────┐
│  Panel Solar    │ 0–25V   │   FZ0430     │
│  (3V nominal)   ├────────┤   Voltage    ├─────┐
│                 │         │   Module     │     │
└─────────────────┘         └──────────────┘     │
                                                  ↓
                                            GPIO34 (ADC)
                                            
┌─────────────────┐         ┌──────────────┐
│  Batería 18650  │ 3–4.2V  │   FZ0430     │
│  (Li-ion)       ├────────┤   Voltage    ├─────┐
│                 │         │   Module     │     │
└─────────────────┘         └──────────────┘     │
                                                  ↓
                                            GPIO35 (ADC)

┌──────────────────┐
│  LDR Sensor      │
│  + 10kΩ 5V div   ├──────────→ GPIO36 (ADC)
└──────────────────┘

┌──────────────────┐
│  Servo SG90      │
│  5V, PWM        ├──────────→ GPIO13
└──────────────────┘

┌──────────────────┐
│  LCD 16×2 I2C    │
│  (PCF8574)       ├──────────→ GPIO21/22
└──────────────────┘
```

---

## 📁 ESTRUCTURA DE CARPETAS DEL PROYECTO

Crear en `/home/tobias/Documentos/OneDrive/workSpace/Rastreador_Solar_Inteligente/`:

```bash
mkdir -p rastreador_solar/{main,components,build,docs}

# Árbol final:
rastreador_solar/
├── main/
│   ├── main.c                      # Función main() + FreeRTOS tasks
│   ├── config.h                    # Configuración centralizada
│   ├── CMakeLists.txt              # Build config
│   ├── Kconfig.projbuild           # Opciones de configuración
│   │
│   ├── energy_sensors.c/h          # Lectura FZ0430 (panel, batería)
│   ├── sensor_ldr.c/h              # Lectura LDR (luz)
│   ├── servo_motor.c/h             # Control PWM del servo
│   ├── display_lcd.c/h             # Control LCD I2C
│   ├── wifi_manager.c/h            # WiFi y Supabase
│   └── telegram_bot.c/h            # Bot Telegram
│
├── components/                     # (Opcional: librerías externas)
│
├── build/                          # Generado por idf.py
│
├── CMakeLists.txt                  # Configuración ESP-IDF raíz
│
└── docs/
    └── (documentación generada)
```

---

## ⚡ PASOS DE INICIO - QUICKSTART

### Paso 1: Preparar entorno
```bash
# En Ubuntu
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
source ./export.sh

# Verificar
idf.py --version  # Debe salir algo como: ESP-IDF v5.0
```

### Paso 2: Crear estructura del proyecto
```bash
cd /home/tobias/Documentos/OneDrive/workSpace/Rastreador_Solar_Inteligente
mkdir -p rastreador_solar/{main,build}
cd rastreador_solar
```

### Paso 3: Crear archivos base
```bash
# main/CMakeLists.txt
cat > main/CMakeLists.txt << 'EOF'
idf_component_register(SRCS "main.c" "energy_sensors.c" "sensor_ldr.c" 
                               "servo_motor.c" "display_lcd.c" "wifi_manager.c" 
                               "telegram_bot.c"
                       INCLUDE_DIRS ".")
EOF

# CMakeLists.txt (raíz)
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rastreador_solar)
EOF

# main/Kconfig.projbuild (opciones configurables)
cat > main/Kconfig.projbuild << 'EOF'
menu "Rastreador Solar Config"
    config WIFI_SSID
        string "WiFi SSID"
        default "TU_SSID"
    
    config WIFI_PASSWORD
        string "WiFi Password"
        default "TU_PASSWORD"
    
    config SUPABASE_URL
        string "Supabase URL"
        default "https://project.supabase.co"
endmenu
EOF
```

### Paso 4: Conectar ESP32 y compilar
```bash
# Detectar puerto
ls /dev/ttyUSB* # Generalmente /dev/ttyUSB0

# Configurar proyecto
idf.py set-target esp32
idf.py menuconfig  # Opcional: configurar parámetros

# Compilar
idf.py build

# Flashear (cargar en ESP32)
idf.py -p /dev/ttyUSB0 flash

# Ver logs en tiempo real
idf.py -p /dev/ttyUSB0 monitor
```

---

## 🎯 FLUJO DE LECTURAS EN TIEMPO REAL

Una vez flasheado, el ESP32 ejecutará **4 tareas FreeRTOS en paralelo:**

```
┌─────────────────────────────────────────────────────────────┐
│              FREERTOS SCHEDULER (Concurrente)              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Task 1: SENSOR (100ms) ─────────────────────────────────── │
│  ├─ Lee LDR → Intensidad luz (0–100%)                     │
│  ├─ Lee FZ0430 Panel → Voltaje panel (0–25V)              │
│  └─ Lee FZ0430 Batería → Voltaje batería (3.0–4.2V)       │
│                                                             │
│ Task 2: SERVO (200ms) ────────────────────────────────── │
│  ├─ Compara lecturas de luz → Busca máximo                │
│  ├─ Calcula ángulo óptimo (0–180°)                        │
│  └─ Envía PWM al servo                                     │
│                                                             │
│ Task 3: DISPLAY (1s) ──────────────────────────────────── │
│  ├─ Muestra en LCD: Voltaje panel, Batería %, Ángulo      │
│  └─ Actualiza cada 1 segundo                               │
│                                                             │
│ Task 4: WIFI/CLOUD (10s) ─────────────────────────────── │
│  ├─ Lee del archivo de configuración (SSID, password)     │
│  ├─ Conecta a WiFi                                         │
│  ├─ Envía datos a Supabase (voltajes, energía, timestamp) │
│  └─ Escucha comandos de Telegram bot                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 MÉTRICAS QUE OBTIENDES

| Métrica | Fuente | Unidad | Rango |
|---------|--------|--------|-------|
| Voltaje Panel | FZ0430 + GPIO34 | V | 0–25V |
| Voltaje Batería | FZ0430 + GPIO35 | V | 3.0–4.2V |
| Porcentaje Batería | Mapeo (3.0V=0%, 4.2V=100%) | % | 0–100% |
| Intensidad Luz | LDR + GPIO36 | % | 0–100% |
| Ángulo Servo | PWM GPIO13 | ° | 0–180° |
| Potencia Panel | V × I (si tienes sensor I) | W | 0–50W |
| Energía Acumulada | Integral P(t) | Wh | - |

---

## 🔧 CONFIGURACIÓN INICIAL (config.h)

```c
// Reemplazar en main/config.h ANTES de compilar:

#define WIFI_SSID                 "Mi_Red_WiFi"
#define WIFI_PASSWORD             "MiContraseña123"

#define SUPABASE_URL              "https://myproject.supabase.co"
#define SUPABASE_ANON_KEY         "eyJhbGc..."

#define TELEGRAM_BOT_TOKEN        "1234567890:ABCxyz..."
#define TELEGRAM_CHAT_ID          "987654321"
```

---

## 🐛 TROUBLESHOOTING

| Problema | Solución |
|----------|----------|
| No detecta ESP32 | `sudo usermod -a -G dialout $USER` + reiniciar sesión |
| Error de compilación | Verificar `idf.py --version` y `source $IDF_PATH/export.sh` |
| LCD no muestra nada | Verificar dirección I2C con `i2cdetect -y 1` |
| Servo no se mueve | Verificar conexión de 5V y GPIO13 |
| WiFi no conecta | Revisar SSID, password y disponibilidad de red |
| Lecturas erráticas | Agregar capacitor 100µF cerca del FZ0430 |

---

## 📚 SIGUIENTES PASOS

1. **Comprar componentes** (lista de compra arriba)
2. **Instalar ESP-IDF** en Ubuntu
3. **Crear estructura carpetas** (Paso 2)
4. **Cargar archivos base** (Paso 3)
5. **Implementar módulos C** (energy_sensors.c, sensor_ldr.c, etc.)
6. **Configurar Supabase** (crear tabla, obtener credenciales)
7. **Crear Telegram bot** (contactar @BotFather)
8. **Flasear ESP32** (Paso 4)
9. **Testing en vivo** (verificar lecturas, movimiento servo, cloud)

---

## 📖 REFERENCIAS

- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/
- **FreeRTOS**: https://www.freertos.org/
- **FZ0430 Datasheet**: [Buscar en AliExpress o Datasheet site]
- **Supabase Docs**: https://supabase.com/docs
- **Telegram Bot API**: https://core.telegram.org/bots/api

---

**¿Necesitas ayuda con algún paso específico? Cuéntame cuál es tu siguiente movimiento.**
