# 📋 ANTEPROYECTO: RASTREADOR SOLAR INTELIGENTE DUAL-EJE

**Documento de Anteproyecto - TP Integrador STR 2026**

---

## 📊 INFORMACIÓN GENERAL DEL PROYECTO

| CAMPO | DESCRIPCIÓN |
|-------|------------|
| **Proyecto Integrador** | STR 2026 - Sistemas de Tiempo Real |
| **Grupo N°** | [Completar con número de grupo] |
| **Nombre del Proyecto** | **SolarTracker Pro v1.0** - Sistema de Seguimiento Solar Automático Dual-Eje |
| **Integrantes** | [Nombres y legajos de integrantes] |

---

## 🎯 PROBLEMÁTICA

La **radiación solar no es constante** durante el día. El sol cambia de posición en dos dimensiones:
- **Horizontalmente** (Este → Oeste) - eje azimutal
- **Verticalmente** (amanecer → mediodía → atardecer) - eje de elevación

Los paneles solares **fijos** capturan significativamente menos energía (típicamente 40-60% menos) que aquellos orientados óptimamente hacia la fuente de luz. 

**Necesidad:** Crear un sistema autónomo que ajuste automáticamente la posición del panel en dos ejes para:
- ✅ Maximizar captura de radiación solar
- ✅ Mejorar rendimiento energético del sistema fotovoltaico
- ✅ Funcionar de forma autónoma sin intervención manual
- ✅ Monitorear y registrar datos en tiempo real

---

## 💡 SOLUCIÓN PROPUESTA

### **Arquitectura General**

```
┌─────────────────────────────────────────────────────────────┐
│                    RASTREADOR SOLAR DUAL-EJE                 │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  SENSORES          PROCESAMIENTO       ACTUADORES            │
│  ─────────────────────────────────────────────────────       │
│                                                               │
│  • 4x LDR (NW,NE,SW,SE)  ┌──────────────────┐              │
│         │                │                  │               │
│         └──→ ESP32 ────→ │ Algoritmo de     │               │
│                │         │ Seguimiento      │               │
│  • Sensor      │         │ (Fuzzy Logic)    │               │
│    Temperatura └────────→│                  │               │
│                         └──────────────────┘               │
│                                │                            │
│                ┌───────────────┼───────────────┐            │
│                ↓               ↓               ↓            │
│         ┌────────────┐ ┌────────────┐ ┌────────────┐        │
│         │ Servo X    │ │ Servo Y    │ │  LCD 16x2  │        │
│         │ (Azimut)   │ │(Elevación) │ │(Métricas)  │        │
│         └────────────┘ └────────────┘ └────────────┘        │
│                ↓               ↓                            │
│         Panel Solar (Dual-Eje)                             │
│                                                               │
│  COMUNICACIÓN                                                │
│  ─────────────────────────────────────────────────           │
│  • Serial (USB) → Daemon C en PC                            │
│  • WiFi (ESP32) → Servidor/Telegram (opcional)             │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 HARDWARE REQUERIDO

### **1. Controlador Principal**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **ESP32** | Versión DOIT DevKit v1 (WiFi + BLE) | 1 | Microcontrolador con WiFi integrado, 36 GPIO, ADC 12-bit |

### **2. Sensores Ópticos**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **LDR (Fotoresistor)** | 5mm, rango 200-20kΩ | 1 | Detectar intensidad de luz para seguimiento |
| **Resistor 10kΩ** | ¼W, tolerancia 5% | 1 | Divisor de tensión para LDR |

### **3. Actuadores (Motor)**
| Componente | Especificación | Cantidad | Eje |
|-----------|--------------|----------|-----|
| **Servo Motor** | Micro servo SG90 o MG90S (180°) | 1 | Azimut (rotación horizontal 0-180°) |

**Nota:** SG90 es suficiente para panel pequeño (100-200g)

### **4. Paneles Solares**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **Panel Solar Mini** | 3V 100-200mA (pequeño educativo) | 2 | Dual-panel para carga + alimentación |

### **5. Sistema de Alimentación**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **Batería Li-ion** | 18650, 3.7V, 3000mAh | 2 | Alimentación del sistema (en serie = 7.4V) |
| **Módulo TP4056** | Cargador solar Li-ion | 1 | Carga controlada desde panel solar |
| **Regulador DC-DC** | Reductor 5V/2A | 1 | Salida 5V para ESP32 y servos |
| **Regulador DC-DC** | Reductor 3.3V/1A | 1 | Salida 3.3V para sensores LDR |
| **Diodo Schottky** | 1N5817 | 1 | Protección entrada solar |
| **Capacitor** | 100µF, 25V | 2 | Desacople y estabilización |

### **6. Pantalla (Display)**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **LCD 16x2** | I2C backpack (azul o verde) | 1 | Mostrar métricas en tiempo real |

### **7. Componentes Auxiliares**
| Componente | Especificación | Cantidad | Razón |
|-----------|--------------|----------|-------|
| **Sensor DS18B20** | Temperatura digital 1-Wire | 1 | Monitoreo térmico (opcional) |
| **Resistencia Pull-up** | 4.7kΩ | 1 | Para bus 1-Wire del sensor temp |
| **Conector USB-C** | Para programación ESP32 | 1 | Descarga de código/datos |
| **Protoboard** | 830 puntos | 1 | Conexiones temporales |
| **Cable jumper** | Macho/Hembra | 50+ | Interconexiones |

### **Lista de Compra Resumida**
```
□ ESP32 DOIT DevKit v1
□ 1x LDR 5mm + 1x Resistor 10kΩ
□ 1x Servo Motor SG90
□ 2x Panel Solar 3V 100mA
□ 2x Batería Li-ion 18650 + Estuche
□ Módulo TP4056
□ 2x Regulador DC-DC (5V y 3.3V)
□ LCD 16x2 con I2C backpack
□ Sensor DS18B20 (opcional)
□ Diodo Schottky 1N5817
□ Capacitores, resistores, cables, protoboard
```

---

## 🌍 EXPLICACIÓN: RASTREADOR EJE ÚNICO (HORIZONTAL)

### **Configuración del Sistema**

```
                    ☀️ (Fuente de luz)
                    |
                    V
            ┌─────────────┐
            │  LDR Sensor │ (Detecta luz)
            └──────┬──────┘
                   |
    Panel Solar ◀──┼──▶ Rota 0° a 180°
    (inclinado 45°)│   (busca máxima luz)
                   |
             ┌─────┴────┐
             │   Servo  │
             │  0-180°  │
             └──────────┘
```

### **Funcionamiento Simple**

1. **Sensor LDR** detecta intensidad de luz
2. **ESP32 compara** luz actual vs luz máxima conocida
3. Si luz disminuye → **rotar servo ±10°** hasta encontrar pico
4. Si encuentra luz mayor → **actualizar nueva posición óptima**
5. **Mostrar en LCD**: voltaje, batería, luz detectada

### **Ventajas de Este Enfoque**

✅ **Simplicidad:** Un solo servo, lógica directa  
✅ **Bajo consumo:** Menos movimiento que dual-eje  
✅ **Rápido de implementar:** Algoritmo básico comparativo  
✅ **Confiable:** Menos componentes = menos fallos  

**Eficiencia energética:** +25-35% respecto a panel fijo (vs +50-60% dual-eje)

---

## 📡 ARQUITECTURA DE SOFTWARE

### **Sistema Autónomo con FreeRTOS**

El ESP32 ejecutará **múltiples tareas concurrentes** bajo control de FreeRTOS:

```
┌─────────────────────────────────────────────────────────────┐
│                   FREERTOS - TASK SCHEDULER                  │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐  ┌──────────────────┐                 │
│  │ TASK 1: SENSOR   │  │ TASK 2: MOTOR    │                 │
│  │ (Lectura LDR)    │  │ (Control Servo)  │                 │
│  │ Prioridad: ALTA  │  │ Prioridad: ALTA  │                 │
│  │ Period: 100ms    │  │ Period: 200ms    │                 │
│  └──────────────────┘  └──────────────────┘                 │
│                                                               │
│  ┌──────────────────┐  ┌──────────────────┐                 │
│  │ TASK 3: DISPLAY  │  │ TASK 4: DATOS    │                 │
│  │ (LCD 16x2)       │  │ (Almacenar/WiFi) │                 │
│  │ Prioridad: MEDIA │  │ Prioridad: BAJA  │                 │
│  │ Period: 1000ms   │  │ Period: 10000ms  │                 │
│  └──────────────────┘  └──────────────────┘                 │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### **Tareas FreeRTOS Específicas**

#### **TASK 1: Lectura de Sensores**
```c
void task_read_sensor(void *pvParameters) {
    int luz_actual = 0;
    int luz_maxima = 0;
    
    while(1) {
        // Leer LDR
        luz_actual = adc_read_channel(ADC_LDR);
        
        // Comparar con máximo
        if (luz_actual > luz_maxima * 0.95) {
            // Luz suficiente, mantenemos posición
            luz_maxima = luz_actual;
            xQueueSend(queue_luz, &luz_actual, 0);
        } else {
            // Luz menor, señalar para buscar nuevo máximo
            xQueueSend(queue_buscar_luz, &luz_actual, 0);
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS); // Cada 100ms
    }
}
```

#### **TASK 2: Control del Servo**
```c
void task_control_servo(void *pvParameters) {
    int debe_buscar = 0;
    int angulo_actual = 90; // Centro
    int angulo_anterior = 90;
    
    while(1) {
        // Revisar si debe buscar nueva posición
        if (xQueueReceive(queue_buscar_luz, &debe_buscar, 0)) {
            // Rotar ±10° y medir
            if (luz_en_angulo[angulo_actual + 10] > 
                luz_en_angulo[angulo_actual]) {
                angulo_actual += 10;
            } else if (luz_en_angulo[angulo_actual - 10] > 
                       luz_en_angulo[angulo_actual]) {
                angulo_actual -= 10;
            }
            
            // Limitar a 0-180
            if (angulo_actual < 0) angulo_actual = 0;
            if (angulo_actual > 180) angulo_actual = 180;
            
            // Mover servo solo si hay cambio
            if (angulo_actual != angulo_anterior) {
                pwm_set_servo(GPIO_SERVO, angulo_actual);
                angulo_anterior = angulo_actual;
            }
        }
        
        vTaskDelay(200 / portTICK_PERIOD_MS); // Cada 200ms
    }
}
```

#### **TASK 3: Display LCD**
```c
void task_display_lcd(void *pvParameters) {
    float voltaje = 0;
    float bateria = 0;
    int luz = 0;
    
    while(1) {
        // Obtener datos de otros tasks
        xQueueReceive(queue_voltaje, &voltaje, 0);
        xQueueReceive(queue_bateria, &bateria, 0);
        xQueueReceive(queue_luz, &luz, 0);
        
        // Mostrar en LCD
        lcd_clear();
        lcd_printf("V:%.2fV  B:%.0f%%", voltaje, bateria);
        lcd_set_cursor(1, 0);
        lcd_printf("LUZ:%d", luz);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Cada 1s
    }
}
```

#### **TASK 4: Almacenamiento y WiFi**
```c
void task_data_management(void *pvParameters) {
    // Conectar WiFi
    wifi_init_sta();
    
    // Conectar a Supabase
    supabase_init();
    
    while(1) {
        // Cada 10 segundos, enviar datos
        float data = {
            .timestamp = get_timestamp(),
            .voltaje = global_voltaje,
            .bateria = global_bateria,
            .luz = global_luz,
            .angulo_servo = global_angulo
        };
        
        // Guardar en Supabase
        supabase_insert_row("solar_data", &data);
        
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Cada 10s
    }
}
```

### **Estructura del Proyecto ESP-IDF**

```
rastreador_solar/
├── CMakeLists.txt
├── idf_component.yml
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                      # Inicialización y creación de tasks
│   ├── sensor_task.c/h             # Lectura LDR
│   ├── servo_task.c/h              # Control servo PWM
│   ├── display_task.c/h            # LCD I2C
│   ├── wifi_task.c/h               # Conexión WiFi
│   ├── supabase_client.c/h         # Cliente Supabase
│   ├── telegram_bot.c/h            # Bot Telegram (opcional)
│   └── config.h                    # Configuración GPIO y parámetros
├── components/
│   ├── ldr_sensor/
│   ├── servo_pwm/
│   └── lcd_i2c/
└── build/
```

---

## ⏱️ REQUISITO DE TIEMPO REAL

| Parámetro | Valor | Justificación |
|-----------|-------|---------------|
| **Latencia sensor→motor** | < 500 ms | El sol se mueve lentamente; cualquier respuesta < 500ms es imperceptible |
| **Actualización LCD** | Cada 1-2 seg | Mostrar datos sin parpadeos |
| **Envío Serial** | Cada 500 ms | Suficiente para graficar tendencias |
| **Sincronización WiFi** | Cada 5-10 min | No sobrecargar red; datos resumidos |
| **Muestreo de sensores** | Cada 100 ms | Detectar cambios rápidos de iluminación |

---

## ✅ MÉTRICA DE VALIDACIÓN

### **Test 1: Comparación de Energía (Principal)**
```
Procedimiento:
1. Colocar un panel FIJO orientado al Sur
2. Ejecutar rastreador durante 8 horas (9:00-17:00)
3. Medir energía acumulada en ambos

Criterio de éxito:
Rastreador genera ≥ 40% más energía que panel fijo
(Típicamente 50-60% en cielo despejado)
```

### **Test 2: Precisión de Seguimiento**
```
Procedimiento:
1. En diferentes horas (9, 12, 15 hs) medir ángulos reales
2. Comparar con ángulos esperados (cálculo solar)

Criterio de éxito:
Error ≤ ±10° en azimut y elevación
```

### **Test 3: Autonomía del Sistema**
```
Procedimiento:
1. Cargar batería completamente (con panel solar)
2. Desconectar alimentación externa
3. Medir tiempo hasta que sistema se apague

Criterio de éxito:
Autonomía mínima > 4 horas en seguimiento continuo
```

### **Test 4: Comunicación**
```
Procedimiento:
1. Conectar WiFi del ESP32
2. Medir tiempo de conexión
3. Verificar recepción de datos en servidor

Criterio de éxito:
Conexión establece en < 3 segundos
Pérdida de paquetes < 1%
```

---

## 🔄 ESTADO DEL PROYECTO

**ESTADO ACTUAL:** 📋 En Desarrollo - Fase de Anteproyecto

| Fase | Estado | Descripción |
|------|--------|-------------|
| **Diseño** | ✅ Completado | Arquitectura definida, componentes seleccionados |
| **Adquisición Hardware** | ⏳ Pendiente | Compra de componentes |
| **Desarrollo Firmware** | ⏳ Pendiente | Implementar algoritmo en C para ESP32 |
| **Desarrollo Software PC** | ⏳ Pendiente | Daemon C + Dashboard Python |
| **Integración** | ⏳ Pendiente | Ensamblar sistema completo |
| **Testing** | ⏳ Pendiente | Realizar pruebas de validación |
| **Documentación** | ⏳ En progreso | Informe final y manual de usuario |

---

## 📋 PRÓXIMOS PASOS

1. ✅ **Revisar y aprobar anteproyecto** con cátedra
2. 🛒 **Adquirir componentes** según lista de compra
3. 🔧 **Diseñar circuito esquemático** y validar con multímetro
4. 💻 **Desarrollar firmware ESP32** (módulo por módulo)
5. 🔌 **Integración del hardware** en estructura mecánica
6. 📊 **Implementar visualización** en PC (Python)
7. 🧪 **Realizar pruebas** y ajustes
8. 📝 **Documentación final** e informe de resultados

---

## 📚 REFERENCIAS Y RECURSOS

- **ESP32 Documentation:** https://docs.espressif.com/projects/esp-idf/
- **Servo Motors:** SG90 specifications, PWM control (1-2 ms pulses)
- **LDR Sensing:** Analog voltage divider circuits
- **Solar Tracking Algorithms:** Fuzzy logic, comparación de cuadrantes
- **Python Plotting:** Matplotlib, Plotly para gráficas en tiempo real

---

**Documento preparado:** Fecha actual
**Versión:** 1.0 - Anteproyecto
**Estado:** 🟡 Pendiente de Aprobación
