# 📊 LECTURA DE VOLTAJE: DIVISOR DE TENSIÓN vs MÓDULO SENSOR FZ0430

**Comparativa técnica y recomendaciones**

---

## 🔍 OPCIÓN 1: Divisor de Tensión (Resistores 10k+10k)

### Ventajas
- ✅ Muy barato (2 resistores = $0.05)
- ✅ Sin componentes adicionales
- ✅ Bajo consumo de energía
- ✅ Directo al ADC

### Desventajas
- ❌ Necesita calibración manual
- ❌ Impreciso si varía la temperatura
- ❌ Ocupa espacio en protoboard
- ❌ Requiere cálculos en código
- ❌ Susceptible a ruido

### Circuito
```
Panel (0-5V)
    │
   [R1 = 10kΩ]
    │
    ├─→ GPIO34 (ADC)
    │
   [R2 = 10kΩ]
    │
   GND
```

### Código requerido
```c
// Lectura bruta
int adc_raw = adc_read(GPIO34);

// Conversión manual
float voltage_at_pin = adc_raw * 3.3 / 4095;
float panel_voltage = voltage_at_pin * 2.0;  // Divisor 1:2

// Necesita calibración
if (temperature > 25°C) {
    panel_voltage *= 0.98;  // Compensación térmica
}
```

---

## 🎯 OPCIÓN 2: Módulo Sensor FZ0430 (RECOMENDADO)

### ¿Qué es?
Un módulo de medición de voltaje que:
- Rango: **0V - 25V DC**
- Salida analógica (0-5V)
- Interface I2C (algunas versiones)
- Precisión: ±0.1V
- Temperatura compensada internamente

### Ventajas
- ✅ Precisión garantizada (±0.1V)
- ✅ Rango grande (0-25V)
- ✅ Sin calibración manual
- ✅ Temperatura compensada
- ✅ Bajo ruido
- ✅ Plug & Play
- ✅ Profesional

### Desventajas
- ❌ Costo: ~$5-8
- ❌ Ocupa más espacio
- ❌ Consumo ligeramente mayor (~20mA)
- ❌ Requiere librería específica

### Circuito
```
Panel (0-25V)
    │
 [FZ0430 Sensor]
    │
    ├─ VCC → +5V (ESP32)
    ├─ GND → GND
    ├─ OUT → GPIO34 (ADC) o I2C
    │
    ▼
  ESP32
```

### Conexiones del FZ0430
```
FZ0430 Módulo Sensor
┌─────────────────┐
│  +  │ OUT │ -   │
│  │   │    │  │   │
└────┬───┬────┬────┘
     │   │    │
     │   │    └─→ GND (Negro)
     │   └──────→ GPIO34 o SDA (Amarillo/Verde)
     └──────────→ +5V (Rojo)
```

---

## 📈 COMPARATIVA TÉCNICA

| Parámetro | Divisor 10k+10k | FZ0430 |
|-----------|-----------------|--------|
| **Costo** | $0.05 | $5-8 |
| **Precisión** | ±0.5V | ±0.1V |
| **Rango** | 0-3.3V (limitado) | 0-25V |
| **Calibración** | Manual necesaria | Automática |
| **Temperatura** | No compensada | Compensada |
| **Ruido** | Alto | Bajo |
| **Setup** | Complejo | Simple |
| **Código** | ~15 líneas | ~5 líneas |
| **Fiabilidad** | Media | Alta |

---

## 🔧 IMPLEMENTACIÓN CON FZ0430

### Versión Analógica (Recomendada - Simple)

```c
#include "esp_adc/adc_oneshot.h"

#define GPIO_VOLTAGE_SENSOR   ADC1_CHANNEL_6  // GPIO34
#define SENSOR_SCALE_FACTOR   5.0  // FZ0430: 5V en ADC = 25V real

float read_panel_voltage_fz0430(void) {
    // 1. Leer ADC (0-4095)
    int adc_raw = adc_oneshot_read(adc_handle, GPIO_VOLTAGE_SENSOR);
    
    // 2. Convertir a voltaje en pin (0-3.3V)
    float voltage_at_pin = (float)adc_raw * 3.3 / 4095;
    
    // 3. Convertir a voltaje real del sensor
    // FZ0430 mapea: 25V → 5V salida
    // Por lo tanto: V_real = V_pin × (25V / 5V)
    float panel_voltage = voltage_at_pin * SENSOR_SCALE_FACTOR;
    
    // 4. Opcional: filtro exponencial para eliminar ruido
    static float voltage_prev = 0;
    panel_voltage = voltage_prev * 0.7 + panel_voltage * 0.3;
    voltage_prev = panel_voltage;
    
    return panel_voltage;
}

// En TASK 1 (cada 100ms):
float panel_v = read_panel_voltage_fz0430();
ESP_LOGI(TAG, "Panel voltage: %.2fV", panel_v);
```

### Versión I2C (Precisión Ultra)

Si el FZ0430 tiene salida I2C:

```c
#include "driver/i2c.h"

#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SCL_IO       22
#define I2C_MASTER_SDA_IO       21
#define I2C_MASTER_FREQ_HZ      100000
#define SENSOR_I2C_ADDR         0x40  // Típico FZ0430

float read_voltage_i2c(void) {
    uint8_t data[2];
    
    // Leer 2 bytes del sensor
    esp_err_t ret = i2c_master_read_from_device(I2C_MASTER_NUM,
                                                 SENSOR_I2C_ADDR,
                                                 data, 2,
                                                 pdMS_TO_TICKS(1000));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error I2C");
        return -1;
    }
    
    // Convertir bytes a voltaje
    uint16_t adc_value = (data[0] << 8) | data[1];
    float voltage = adc_value * 25.0 / 4096.0;  // 12-bit resolution
    
    return voltage;
}
```

---

## ⚡ LECTURA DE BATERÍA CON FZ0430

Similar al panel, pero con rango más pequeño:

```c
#define GPIO_BATTERY_SENSOR    ADC1_CHANNEL_7  // GPIO35
#define BATTERY_SENSOR_MAX     5.0  // 5V en ADC

float read_battery_voltage_fz0430(void) {
    int adc_raw = adc_oneshot_read(adc_handle, GPIO_BATTERY_SENSOR);
    float voltage_at_pin = (float)adc_raw * 3.3 / 4095;
    
    // FZ0430 para batería: mapea 0-5V salida a 0-25V entrada
    // Pero batería solo llega a 4.2V, así que:
    // 4.2V entrada → 4.2V × (5V/25V) = 0.84V en ADC
    float battery_voltage = voltage_at_pin * 5.0;
    
    // Límites reales
    if (battery_voltage < 3.0) battery_voltage = 3.0;
    if (battery_voltage > 4.2) battery_voltage = 4.2;
    
    // Convertir a porcentaje
    float battery_percent = (battery_voltage - 3.0) / 1.2 * 100;
    
    return battery_percent;
}
```

---

## 🎯 SOLUCIÓN MIXTA RECOMENDADA (Como en el proyecto que viste)

**Usa DOS módulos FZ0430:**

```
Panel Solar (0-5V)
    │
    └─→ [FZ0430 #1]
           │
           └─→ GPIO34 (ADC)
               ESP32


Batería (3.0-4.2V)
    │
    └─→ [FZ0430 #2]
           │
           └─→ GPIO35 (ADC)
               ESP32
```

### Código combinado

```c
typedef struct {
    float panel_voltage;
    float battery_voltage;
    float battery_percent;
    float power;
} EnergyData;

EnergyData read_energy_data(void) {
    EnergyData data = {0};
    
    // Panel
    int adc_panel = adc_oneshot_read(adc_handle, ADC1_CHANNEL_6);
    data.panel_voltage = (float)adc_panel * 3.3 / 4095 * 5.0;
    
    // Batería
    int adc_battery = adc_oneshot_read(adc_handle, ADC1_CHANNEL_7);
    float battery_v = (float)adc_battery * 3.3 / 4095 * 5.0;
    data.battery_voltage = battery_v;
    data.battery_percent = (battery_v - 3.0) / 1.2 * 100;
    
    // Potencia (V × I)
    float current = read_current_from_shunt();
    data.power = data.panel_voltage * current;
    
    return data;
}

// En TASK 1:
EnergyData energy = read_energy_data();
ESP_LOGI(TAG, "Panel: %.2fV | Battery: %.2fV (%.0f%%) | Power: %.2fW",
         energy.panel_voltage,
         energy.battery_voltage,
         energy.battery_percent,
         energy.power);
global_voltaje = energy.panel_voltage;
global_bateria = energy.battery_percent;
```

---

## 🛒 LISTA DE COMPRA ACTUALIZADA

### Opción A: Económica (Divisor de Tensión)
```
□ 4x Resistor 10kΩ       ($0.20)
□ 1x LDR 5mm             ($0.50)
□ Resto de hardware      (ya incluido)
TOTAL EXTRA: ~$0.70
```

### Opción B: Profesional (FZ0430) ⭐ RECOMENDADO
```
□ 2x Módulo FZ0430 0-25V ($12-16)
□ 1x LDR 5mm             ($0.50)
□ Cables/conectores      ($2)
TOTAL EXTRA: ~$14-18
```

### Opción C: Mixta (1 FZ0430 + 1 Divisor)
```
□ 1x Módulo FZ0430       ($6-8)
□ 2x Resistor 10kΩ       ($0.10)
□ 1x LDR 5mm             ($0.50)
TOTAL EXTRA: ~$7-9
```

---

## 📋 RECOMENDACIÓN FINAL

### ✅ USA FZ0430 SI:
- Quieres máxima precisión
- Diseño profesional
- Presupuesto permite ($14-18 extra)
- Quieres código simple
- No quieres hacer calibración

### ✅ USA DIVISOR SI:
- Presupuesto muy limitado (<$1)
- No necesitas ultra precisión
- Quieres aprender electrónica
- Solo es para prototipo

### 🎯 RECOMENDACIÓN: **FZ0430 (Opción B)**

Por que:
1. **Costo no es prohibitivo** ($14-18)
2. **Mucho más preciso** (±0.1V vs ±0.5V)
3. **Plug & Play** (sin calibración)
4. **Código más simple**
5. **Proyecto final se ve profesional**
6. **Igual que el proyecto que viste**

---

## 🔌 DIAGRAMA FINAL CON FZ0430

```
PANEL SOLAR (0-5V)
      │
      └─→ [FZ0430 #1] ─→ GPIO34
              │
           +5V ← ESP32 +5V
           GND ← ESP32 GND


BATERÍA (3-4.2V)
      │
      └─→ [FZ0430 #2] ─→ GPIO35
              │
           +5V ← ESP32 +5V
           GND ← ESP32 GND


LDR
      │
   [R 10k]
      │
      └─→ GPIO36 (ADC)


SERVO
      │
      └─→ GPIO13 (PWM)


LCD I2C
      │
      ├─ SDA → GPIO21
      └─ SCL → GPIO22
```

---

## 📊 TABLA DE CONVERSIÓN FZ0430

| Entrada (V) | Salida ADC | Lectura ESP32 |
|-------------|-----------|---------------|
| 0V | 0 | 0V |
| 5V | 4095 | 3.3V × (25/5) = 16.5V |
| 12V | 1968 | 1.6V × (25/5) = 8V |
| 25V | 4095 | 3.3V × (25/5) = 25V |

**Factor de conversión: `valor_ADC × 3.3 / 4095 × 5.0`**

---

## 💡 VENTAJA PRINCIPAL

El FZ0430 **ya trae interno:**
- Acondicionamiento de señal
- Filtrado de ruido
- Compensación térmica
- Protección contra sobrevoltaje

**Tú solo necesitas:** conectar y leer el ADC.

---

**Conclusión:** Para un proyecto profesional y educativo como el tuyo, **USA FZ0430**. La diferencia de precio ($14-18) es mínima comparada con la mejora en precisión y confiabilidad.

