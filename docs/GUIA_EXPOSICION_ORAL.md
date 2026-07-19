# Exposición Oral — Rastreador Solar Inteligente

**Guía de charla: problemas, decisiones y aprendizajes de ingeniería**

---

## ¿Cómo usar esta guía?

Esto no es un libreto para leer. Son **puntos de conversación** organizados como una charla reflexiva. La idea es contar **qué pasó durante el proyecto**, no cómo funciona el código. Los profesores quieren ver que **aprendieron de los errores**, no que memorizaron APIs. Usen esto como referencia para mantener el hilo, pero háblenlo natural.

---

## 1. Apertura — De qué se trata (2 minutos)

"Buenas, somos Agustín Brambilla y Tobías Funes. Les vamos a contar nuestra experiencia con el proyecto integrador de Sistemas de Tiempo Real: un rastreador solar de dos ejes basado en ESP32."

**Lo esencial que tienen que decir:**
- Es un panel solar que **sigue al sol** en dos direcciones: horizontal (azimut) y vertical (elevación)
- Usa **4 sensores de luz** en las esquinas del panel para detectar de dónde viene más luz
- Con **dos servomotores** corrige la orientación del panel
- Tiene **WiFi** y se puede monitorear desde una PC, un celular (Telegram) o un navegador web

**No digan:** detalles de implementación, nombres de funciones, pines específicos. Eso va después si preguntan.

---

## 2. El origen — ¿Por qué hicimos esto? (1 minuto)

**La idea:**
> Si el sol se mueve durante el día y un panel fijo pierde entre 40 y 60% de energía potencial, ¿por qué no hacer que el panel lo siga?

**El enfoque:**
- Aplicar conceptos de Sistemas de Tiempo Real a un proyecto concreto
- No queríamos un "simulador" o un ejemplo de escritorio — queríamos **algo que se mueva de verdad**
- Elegimos ESP32 porque ya tenía WiFi, ADC, PWM por hardware, y corría FreeRTOS nativamente

---

## 3. El primer golpe — El ESP32 se quemó (3 minutos)

**Este es un punto importante para los profesores. Acá van a ver que aprendieron de un error real de ingeniería.**

### Qué pasó:
- Estábamos probando el sistema con la batería conectada (Li-ion → MT3608 → VIN del ESP32)
- En un momento conectamos también el USB para ver el monitor serial
- El ESP32 **empezó a calentarse** y dejó de responder
- Se quemó el regulador interno y el puente CH340

### Por qué pasó:
- Al conectar USB (5V) y VIN (5V del MT3608) al mismo tiempo, **las dos fuentes compiten**
- El ESP32 no tiene protección contra esta situación — los 5V del USB entran por el pin VBUS y los 5V del MT3608 entran por VIN, se encuentran en el regulador interno y lo fríen

### Lo que aprendimos:
> **Lección de ingeniería 1:** Una regla escrita en un documento ("no conectar VIN y USB juntos") no es suficiente. La próxima vez pondríamos un diodo de protección o un jumper físico.

### Cómo lo solucionamos:
- Compramos otro ESP32 DevKit
- Reforzamos la regla: **o se alimenta por USB, o se alimenta por batería, nunca ambos**
- Documentamos y verificamos el protocolo de alimentación antes de cada prueba

---

## 4. El segundo golpe — Los servos no funcionaban bien (3 minutos)

**Otro error real de hardware que muestra el proceso de troubleshooting.**

### Qué pasó:
- Compramos dos servos SG90 (los más comunes, económicos)
- Al probar el tracking, el panel se movía **de forma errática, a tirones**
- A veces no se movía directamente, otras veces vibraba sin llegar a la posición deseada

### El proceso de diagnóstico (esto es lo que los profesores quieren escuchar):
1. **Primero pensamos que era el código** — revisamos la generación PWM, los timers, la frecuencia
2. **Después pensamos que era la fuente** — medimos el voltaje del MT3608, parecía estable
3. **Probamos con una fuente de laboratorio** (no la batería) — el problema persistía
4. **Cambiamos los servos por unos MG90S** (versión con engranajes metálicos) — **funcionó perfecto**

### Por qué pasó:
- Los SG90 son servos **muy básicos**, con engranajes de plástico y un margen de voltaje ajustado (4.5–5.5V)
- El MT3608 tiene pequeñas fluctuaciones en su salida (propias de un step-up económico)
- Los MG90S tienen **engranajes metálicos**, más torque (2.2 kg·cm vs 1.8 kg·cm), y toleran **4.8V a 6V**
- La señal PWM del LEDC del ESP32, combinada con las fluctuaciones del MT3608, hacía que los SG90 perdieran el paso

> **Lección de ingeniería 2:** No todos los componentes "compatibles" lo son en la práctica. Un servo SG90 funciona perfecto con una fuente de laboratorio estable, pero falla con un step-up económico. Hay que probar en las condiciones reales de operación.

### Dato para mencionar si preguntan:
La diferencia de precio entre SG90 y MG90S es mínima (~$500 vs ~$1000 ARS), pero la diferencia en confiabilidad es enorme.

---

## 5. Decisiones de diseño que tomamos en el camino (4 minutos)

**Acá pueden mencionar decisiones puntuales, pero enmarcadas como "problemas que resolvimos", no como "funcionalidades que implementamos".**

### 5.1 — Por qué movemos un servo a la vez

**El problema:** Cuando movíamos los dos servos al mismo tiempo, el ESP32 se reiniciaba.

**La causa:** Los servos en movimiento consumen ~250mA cada uno. El pico de 500mA + el consumo del ESP32 (~200mA con WiFi activo) hacía caer la tensión del MT3608 por debajo del umbral de operación del ESP32.

**La solución:** Alternar los movimientos: primero azimut, esperar 100ms, después elevación. El pico de corriente se reduce a la mitad.

### 5.2 — Por qué no usamos colas FreeRTOS (Queues)

**El problema:** En los TPs anteriores usaban colas FreeRTOS para todo. En este proyecto no las usamos.

**La justificación:** Usamos variables globales con getters. En el ESP32, las escrituras de `int` y `float` son atómicas (32 bits, alineadas). Las tareas que escriben y las que leen están en una relación simple (productor → consumidor), sin contención. Una cola sería más "correcta" pero agregaría complejidad innecesaria.

**Si los profesores preguntan "¿y por qué no usaron colas?":** Reconozcan que es una simplificación, que para este caso funciona, pero que si el proyecto escalara (más sensores, más frecuencia de muestreo) migrarían a colas.

### 5.3 — Por qué filtramos las lecturas del ADC por software

**El problema:** Las lecturas del ADC del ESP32 son ruidosas (±5-10 cuentas).

**La solución:** En el monitor de batería usamos un promedio de 30 muestras (responde en 1.5 segundos — las baterías no cambian rápido). En los sensores LDR usamos un filtro exponencial 50/50 (responde más rápido — la luz puede cambiar por nubes parciales).

**La decisión consciente:** No pusimos capacitores en el ADC para ahorrar componentes. El filtro por software fue suficiente.

### 5.4 — La histéresis de batería

**El problema:** Si poníamos un solo umbral (ej: "apagar a 3.3V"), cuando el tracking se apagaba, el voltaje subía porque los servos dejaban de consumir, y el sistema volvía a encenderse → ciclo infinito.

**La solución:** Dos umbrales: tracking ON a 3.5V, tracking OFF a 3.2V. La banda de 0.3V evita la oscilación.

---

## 6. Lo que funciona bien (1 minuto)

**Sean honestos pero muestren resultados. Digan lo que sí lograron:**

- El sistema **sigue la luz** correctamente. Con una linterna, los servos se orientan hacia ella.
- El **monitoreo remoto funciona** — tanto por UDP (PC), como por web (navegador) y por Telegram (celular).
- La **gestión de batería** funciona — el sistema se apaga solo cuando la batería está baja y se reanuda cuando se carga.
- El **control proporcional** evita movimientos bruscos: a mayor error, mayor corrección, pero acotada.

---

## 7. Lo que falta / lo que harían diferente (1 minuto)

**Esto también suma puntos con los profesores — muestra que piensan como ingenieros, no como estudiantes que entregan y se olvidan.**

### Trabajo futuro:
- **Deep sleep:** El ESP32 consume al pedo entre ciclos de tracking (100ms). Podríamos dormirlo y despertarlo cada 5 segundos. La autonomía pasaría de ~8 horas a >24 horas.
- **Almacenamiento local:** Si el WiFi no está disponible, los datos se pierden. Podríamos usar la flash del ESP32 (SPIFFS/NVS) como buffer y sincronizar después.
- **PID:** El control proporcional funciona, pero si quisiéramos más precisión (ej: concentradores solares), un PID sería mejor.

### Lo que harían diferente:
- **Protección física en la alimentación:** Un diodo o un jumper para evitar que se repita lo del ESP32 quemado. No dejar la protección solo en un documento.
- **Probar los servos antes de diseñar el tracking:** Si hubiéramos probado los SG90 a fondo al principio, nos habríamos dado cuenta del problema antes y no habríamos perdido tiempo debugueando el código.
- **Armar la estructura mecánica primero:** Perdimos tiempo programando funciones que después tuvimos que ajustar porque la mecánica del panel no era la que esperábamos.

---

## 8. Cierre — Frase final (30 segundos)

**Algo como:**

> "Para nosotros este proyecto fue mucho más que programar un ESP32. Fue aprender que en ingeniería lo importante no es que las cosas funcionen en el simulador o en la teoría, sino que funcionen cuando las ponés al sol, con una batería medio cargada, con un step-up chino, y con servos que no siempre responden como el datasheet dice. Ahí es donde realmente se aprenden los sistemas de tiempo real."

---

## Apéndice: Posibles preguntas de los profesores

| Pregunta | Cómo responder |
|----------|---------------|
| "¿Por qué no usaron PID?" | El sol se mueve muy lento (0.25°/min). No necesitamos acción integral ni derivativa. El proporcional es suficiente y más simple. |
| "¿Por qué no usaron colas FreeRTOS?" | Variables atómicas + getters. Para este caso alcanza. Si hubiera más contención o datos complejos, usaríamos colas. |
| "¿Por qué no hicieron un PCB?" | Tiempo y costo. La protoboard permite iterar rápido. Si fuera un producto, sí haríamos PCB. |
| "¿El sistema es de tiempo real?" | Sí, soft real-time. La latencia sensor→motor es ~150ms, muy por debajo de los 500ms requeridos. |
| "¿Qué harían si tuvieran que hacerlo de nuevo?" | Protección física en alimentación, probar servos antes, armar la mecánica primero. |
| "¿Cuánto dura la batería?" | En teoría ~11 horas. En la práctica ~6-8 horas por pérdidas y picos. |

---

*Documento preparado para la exposición oral — STR 2026*
*No leer textual. Usar como referencia de los temas a tocar.*
