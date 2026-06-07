CENTRO REGIONAL UNIVERSITARIO CÓRDOBA IUA  
FACULTAD DE INGENIERÍA

INGENIERÍA INFORMÁTICA – Sistemas en Tiempo Real

---

# TRABAJO PRÁCTICO INTEGRADOR  
## "Rastreador Solar Inteligente"

**PROFESORES:**  
Storaccio Luis  
Ducloux José María

**GRUPO N° 7**

**ALUMNOS:**  
Brambilla Agustin Eduardo (abrambilla804@alumnos.iua.edu.ar)  
Funes Tobias (tfunes744@alumnos.iua.edu.ar)

---

## 1. Problemática

La radiación solar no es uniforme durante el día. Los paneles solares fijos capturan significativamente menos energía que aquellos orientados activamente hacia la fuente de luz. Se requiere un sistema autónomo, eficiente y de bajo costo, que ajuste la posición del panel en dos ejes (azimut y elevación) para maximizar la captación energética, con monitoreo remoto y métricas de operación accesibles.

---

## 2. Solución (Hardware/Software)

### 2.1 Software

- Firmware desarrollado en **ESP-IDF (FreeRTOS)** sobre **ESP32**.
    - Tareas concurrentes: muestreo de sensores LDR (100 ms), control de servos, monitoreo de voltaje, visualización en LCD 16x2 (I2C), conectividad WiFi a Supabase y bot Telegram.
    - Comunicación entre tareas vía colas FreeRTOS y semáforos para garantizar tiempos deterministas.
    - Umbrales configurables para actuación sobre servos y condiciones de alarma.
- **Daemon en PC (opcional):**
    - Python/C, recibe datos en tiempo real vía socket TCP/WiFi para dashboard local, registro histórico y control manual avanzado.

---

### 2.2 Hardware

- **ESP32-DEVKIT V1**  
  Microcontrolador principal con WiFi/Bluetooth integrado.
- **Dos servomotores SG90**  
  Control de orientación en ejes azimut y elevación.
- **Dos paneles solares de 5.5 V / 100 mA**  
  Proveen energía para carga de baterías.
- **Dos baterías Li-ion recargables 3.7 V 2200 mAh**  
  **Conectadas en serie** para suministrar 7.4 V directo al pin VIN del ESP32 y alimentar todos los periféricos (servos, LCD, etc.).
- **Dos módulos TP4056**  
  Uno para cada batería, recarga independiente y protección.
- **Display LCD 1602 I2C**  
  Visualización de variables críticas y alertas.
- **Sensores LDR (fotoresistores)**  
  Cuatro unidades dispuestas de forma diferencial para detección direccional de luz.
- **Divisores resistivos (10kΩ + 10kΩ)**  
  Para medición segura de voltajes de baterías y paneles por ADC.
- **Capacitor de 1 µF**  
  ÚNICAMENTE en la entrada del ADC para estabilidad de lectura de voltaje (no se usan filtros de potencia ni desacoples en alimentación).
- **Cables, soportes, PCB prototipo**  
  Integración mecánica y eléctrica básica.
- **Sin reguladores de voltaje**  
  Todo el sistema es alimentado directamente por las baterías conectadas en serie para minimizar complejidad, costo y dependencia de componentes difíciles de conseguir.

---

## 3. Requisito Tiempo Real

- **Período de muestreo de sensores (LDR):** 100 ms.
- **Latencia máxima entre detección y ajuste de motores:** ≤ 500 ms.
- **Umbral de eficiencia para disparo:** < 98%.
- **Visualización/alerta en LCD:** cada 1–2 segundos.
- **Reporte cloud / Telegram:** cada 5–10 segundos.
- Todas las tareas de control y muestreo son deterministas y de prioridad fija en FreeRTOS, garantizando sincronización y respuesta en tiempo real a estímulos del entorno.

---

## 4. Métrica de Validación

- Comparación de energía captada (potencia en tiempo real y acumulada) con sistema en dos modos: seguimiento activo y posición fija.  
- Medición del tiempo de reacción ante caída de eficiencia (<98%) y actuación efectiva del motor (≤ 500 ms).
- Registro y análisis de energía almacenada en baterías versus el fotoperíodo y clima.
- Validación de funcionamiento autónomo, conectividad cloud/Telegram y fiabilidad del control, mediante al menos 10 secuencias repetidas.

---

## 5. Estado

- Arquitectura final definida (hardware y software), sin uso de reguladores ni filtros extra de alimentación, conforme a últimos acuerdos y para máxima simplicidad y bajo costo.
- Documentación integral y profesional, alineada con requerimientos académicos y técnicos.
- Plan escalable y abierto para posibles expansiones: integración opcional de daemon PC, mejoras futuras de eficiencia y robustez.

---

### [ANEXOS]

#### **Tabla de componentes final**  

| # | Componente                  | Cantidad | Especificación         | Función Principal                               |
|---|-----------------------------|----------|------------------------|-------------------------------------------------|
| 1 | ESP32-DEVKIT V1             | 1        | WiFi, FreeRTOS         | Control global                                  |
| 2 | Servo SG90                  | 2        | 180°                   | Movimiento en 2 ejes                            |
| 3 | Panel solar                 | 2        | 5.5 V / 100 mA         | Fuente de energía solar                         |
| 4 | TP4056                      | 2        | Carga/protección Li-ion| Recarga segura y protección                     |
| 5 | Batería Li-ion 3.7 V 2200 mAh| 2       | 18650, serie           | Banco energético principal                      |
| 6 | LCD 1602 I2C                | 1        | PCF8574                | Visualización local (estado, alertas)           |
| 7 | Fotoresistores (LDR)        | 4        | ~1 kΩ a pleno sol      | Sensado de luz                                  |
| 8 | Resistencias 10 kΩ          | 8        | 1/4W                   | Divisor de voltaje para ADC                     |
| 9 | Capacitor 1 µF              | 2        | Cerámico, en ADC       | ÚNICO filtrado (solo en medición)               |
|10 | Cable protoboard, soportes   | -        |                        | Integración y montaje                           |

---

#### **Limitaciones, Riesgos y Mitigaciones**

| Restricción/Riesgo                         | Descripción/Impacto                       | Mitigación o Justificación                   |
|--------------------------------------------|-------------------------------------------|---------------------------------------------|
| **Sin regulación de voltaje dedicada**     | Posibles resets, fluctuaciones, brownout   | Secuencia de movimiento (un servo por vez), alertas software, monitoreo y reducción de movimientos ante baja tensión. Documentación explícita al usuario. |
| **Corriente total limitada (pico/pico)**   | Si ambos servos, WiFi y LCD demandan simultáneamente, posible caída de tensión. | Control en el firmware: nunca activar ambos servos a máxima demanda conjunta. Sensado y reacción temprana alojado en el firmware.|
| **Ruido ADC sin filtros en alimentación**  | Pequeñas oscilaciones en medición          | Promedio software, único capacitor 1 µF en ADC, pruebas de calibración específica.|
| **Carga ineficiente en días nublados**     | Energía diaria limitada                   | Monitoreo cloud, notificaciones por Telegram, optimización futura por software. |
| **Integración daemon PC es opcional**      | No afecta autonomía del sistema            | 100% funcionamiento autónomo aun sin PC.     |

---

**Este documento refleja fielmente las decisiones técnicas finales del equipo, y está listo para su revisión, presentación o entrega.**
