# ARQUITECTURA FINAL DEL SISTEMA – RASTREADOR SOLAR DE DOS EJES

---

**PROPÓSITO DEL DOCUMENTO:**
Este documento describe de manera profesional y completa la arquitectura hardware/software definitiva, las decisiones técnicas, riesgos y mitigaciones del Rastreador Solar Autónomo de Dos Ejes basado en ESP32, con monitoreo en la nube y bot Telegram, y la opción de daemon en PC.

---

## 1. INTRODUCCIÓN
- **Objetivo:** Seguimiento solar autónomo en dos ejes (azimut y elevación), optimizando captación solar mediante lógica adaptativa y monitoreo avanzado.
- **Componentes clave:** ESP32, 2x Servos SG90, sensores LDR, LCD 1602 I2C, WiFi, almacenamiento y monitoreo en la nube (Supabase), bot Telegram, y daemon opcional en PC.

---

## 2. ARQUITECTURA GENERAL (BLOQUES)
### 2.1. Diagrama de bloques
- Rápida visualización de módulos y conexiones (incluir gráfico adjunto si es posible).

**Resumen:**
- Paneles solares → TP4056 → Batería (3.7V) → Elevador MT3608 (5.0V) → Alimentación a VIN ESP32, servos SG90 y LCD 1602 I2C.
- ESP32 controla servos, captura datos (voltaje solar/batería, LDR), maneja WiFi y visualización LCD.
- Interfaz a la nube y bot Telegram.
- Daemon PC opcional interactúa por WiFi/LAN.

---

## 3. ARQUITECTURA DE ENERGÍA Y CONEXIONES
### 3.1. Entradas y distribución de potencia
- **Paneles solares:** 1 o 2 paneles 5.5V 100mA conectados en paralelo al TP4056.
- **Baterías:** 1× 3.7V 2200mAh, cargada por TP4056.
- **Elevador de Voltaje:** Módulo MT3608 Step-Up ajustado a 5.0V para alimentar servos, LCD y el ESP32 (vía pin VIN).
- **Divisores resistivos:** Medición de voltajes con factor 1/3 (R1=20kΩ, R2=10kΩ usando resistencias de 10kΩ).

### 3.2. Conexión de componentes
- **ESP32:**
   - GPIOs a servos (PWM), LCD (I2C), LDRs, divisores resistivos.
   - Alimentación por VIN (5.0V desde MT3608).
- **Servos / LCD:**
   - Alimentación directa (5.0V desde MT3608).
- **TP4056:**
   - Entrada desde paneles, salida hacia batería y MT3608.

### 3.3. Resumen de protecciones y filtrados
- Sin capacitores de filtrado en alimentación ni en ADC (se eliminan para reducir componentes, compensado por software).
- Sin uso de módulos FZ0430 (costosos, difíciles de conseguir).

---

## 4. ARQUITECTURA DE CONTROL Y SOFTWARE
### 4.1. ESP32 (ESP-IDF con FreeRTOS)
- Tareas concurrentes: seguimiento de luz (LDR), control de servos, medición de voltajes, visualización LCD, comunicación WiFi/cloud, Telegram bot.
- Lectura periódica, algoritmo de umbral de luz (>5% diferencia, ±10º) para movimiento preciso.

### 4.2. Monitoreo Cloud y Bot Telegram
- REPORTE de estado y métricas a Supabase (voltaje, posición, errores, uptime, consumo estimado).
- Interacción remota/control manual mediante Telegram.

### 4.3. Daemon en PC (opcional)
- Aplicación de escritorio (Python, Node.js o similar): recibe datos por WiFi/LAN, muestra sensores en tiempo real, historial/tendencias, comandos remotos.

---

## 5. LIMITACIONES Y MITIGACIONES
- **Sin FZ0430:** Precisión/estabilidad garantizada con divisores resistivos (factor 1/3) + promedio software.
- **Corriente máxima del MT3608:** Riesgo de caída de tensión si ambos servos, WiFi y LCD operan a máxima demanda simultáneamente. Mitigado por software: secuencia de movimientos (máximo un servo a la vez), monitoreo de voltaje, alertas.
- **Capacidad de batería:** 2200 mAh en una celda, suficiente para operación típica; escalable poniendo una segunda batería en paralelo en el futuro.
- **Daño potencial por sobrecorriente:** Se mitiga con control de software (watchdog, limitación de PWM) y fusible térmico opcional.
- **Ruido ADC sin filtros físicos:** Pequeñas oscilaciones en medición corregidas con un agresivo promedio software (30 muestras).

---

## 6. LISTA DE COMPONENTES FINAL
1. ESP32-DEVKIT V1
2. Servos SG90 ×2
3. Paneles solares 5.5V / 100mA (1 o 2 en paralelo)
4. TP4056 ×1
5. Batería Li-ion 3.7V 2200mAh ×1
6. Módulo Elevador MT3608 (Step-Up) ×1
7. LCD 1602 I2C
8. Fotoresistencias (LDR) ×4
9. Resistencias 10kΩ ×10 (divisores, pull-down)
10. Cables, protoboard, soportes y PCB

---

## 7. DIAGRAMAS
- **Diagrama general de bloques** (adjuntar/añadir más adelante).
- Esquemas eléctricos de conexión.

---

## 8. APÉNDICE Y REFERENCIAS
- Datasheets, enlaces a guías ESP-IDF, Supabase, ejemplos bot Telegram, tutoriales de divisores resistivos, cálculo de autonomía.

---

**Documento preparado para su integración directa en el anteproyecto y revisión profesional.**
