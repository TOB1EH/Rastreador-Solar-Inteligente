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
- Paneles solares → TP4056 ×2 → Baterías en SERIE (7.4V) → Alimentación directa a VIN ESP32, servos SG90 y LCD 1602 I2C.
- ESP32 controla servos, captura datos (voltaje solar/batería, LDR), maneja WiFi y visualización LCD.
- Interfaz a la nube y bot Telegram.
- Daemon PC opcional interactúa por WiFi/LAN.

---

## 3. ARQUITECTURA DE ENERGÍA Y CONEXIONES
### 3.1. Entradas y distribución de potencia
- **Paneles solares:** 2× 5.5V 100mA conectados en paralelo; alimentan y recargan dos baterías independientes.
- **Baterías:** 2× 3.7V 2200mAh EN SERIE (7.4V total), cargadas por TP4056 individual.
- **Sin reguladores de voltaje:** Todo el sistema se alimenta directamente desde la salida serie de las baterías (7.4V) al pin VIN del ESP32 y periféricos. Esto reduce costo, complejidad y dependencia de componentes.
- **Divisores resistivos:** Medición de voltajes con 2× (10kΩ + 10kΩ) y capacitor de 1µF en ADC (mejor estabilidad).

### 3.2. Conexión de componentes
- **ESP32:**
   - GPIOs a servos (PWM), LCD (I2C), LDRs, divisores resistivos.
   - Alimentación por VIN (7.4V directo desde baterías serie).
- **Servos / LCD:**
   - Alimentación directa desde VIN (7.4V), utilizando el regulador interno del ESP32 para 5V/3.3V según corresponda.
- **TP4056:**
   - Uno a cada batería; su salida conecta a la arquitectura serie.

### 3.3. Resumen de protecciones y filtrados
- Sin capacitores de filtrado en alimentación (se eliminan para reducir componentes).
- ÚNICAMENTE capacitor 1µF en cada entrada ADC para reducir ruido en medición de voltaje.
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
- **Sin FZ0430:** Precisión/estabilidad garantizada con divisores resistivos (10kΩ + 10kΩ) + filtrado por capacitor 1µF en ADC y promedio software.
- **Sin reguladores externos:** Riesgo de brownout si ambos servos, WiFi y LCD operan a máxima demanda simultáneamente. Mitigado por software: secuencia de movimientos (máximo un servo a la vez), monitoreo de voltaje, alertas y reducción de actividad ante baja tensión.
- **Capacidad de baterías:** 2200 mAh por celda, suficiente para operación típica; autonomía variable según clima y frecuencia de movimiento.
- **Daño potencial por sobrecorriente:** Se mitiga con control de software (watchdog, limitación de PWM) y fusible térmico opcional.
- **Ruido ADC sin filtros en alimentación:** Pequeñas oscilaciones en medición corregidas con promedio software y capacitor 1µF dedicado.

---

## 6. LISTA DE COMPONENTES FINAL
1. ESP32-DEVKIT V1
2. Servos SG90 ×2
3. Paneles solares 5.5V / 100mA ×2
4. TP4056 ×2
5. Baterías Li-ion 3.7V 2200mAh ×2
6. LCD 1602 I2C
7. Fotoresistencias (LDR) ×4
8. Resistencias 10kΩ ×8 (divisores)
9. Capacitores 1µF ×2 (solo en ADC)
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
