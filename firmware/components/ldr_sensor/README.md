	# Montaje Físico de los 4 Sensores LDR

## Esquema de Conexión Eléctrica

Cada LDR necesita un **divisor de tensión** con una resistencia de **10kΩ**:

```
3.3V ────┬─── LDR ────┬─── GND
          │            │
          └─── 10kΩ ───┘
               │
              GPIO
```

Los 4 LDRs se conectan **exactamente igual**, cada uno a su GPIO:

| LDR | GPIO | Resistencia |
|-----|------|-------------|
| Arriba-Izquierda (TL) | GPIO 32 | 10kΩ a GND |
| Arriba-Derecha (TR) | GPIO 33 | 10kΩ a GND |
| Abajo-Izquierda (BL) | GPIO 36 | 10kΩ a GND |
| Abajo-Derecha (BR) | GPIO 39 | 10kΩ a GND |

### Materiales necesarios por LDR
- 1 LDR (fotoresistor 5mm)
- 1 resistencia 10kΩ (¼W)
- Cables jumper o cable fino

## Posición Física Recomendada

Los 4 LDR deben montarse **alrededor del panel solar**, apuntando en la misma dirección que el panel:

```
          ┌──── LDR ARRIBA ────┐
          │                    │
LDR IZQ ──┤      PANEL        ├── LDR DER
          │                    │
          └──── LDR ABAJO ─────┘
```

### Distancia entre LDRs
- **Separación recomendada**: 5 a 10 cm entre cada LDR
- Si están **muy juntos** (< 3 cm): todos ven casi la misma luz y el error diferencial es muy pequeño para detectar dirección
- Si están **muy separados** (> 15 cm): uno puede quedar en sombra del propio panel y desvirtuar la lectura
- Ideal: montarlos cerca del **borde del panel**, no detrás de él

### Orientación
- Los 4 LDR deben apuntar **hacia adelante** (en la misma dirección que la cara activa del panel solar)
- Deben estar en el **mismo plano** que el panel, no inclinados
- Que no tengan obstáculos entre ellos y el sol (sombras del propio soporte)

## Métodos de Montaje

### Opción 1: Soporte en cruz (recomendado)
1. Cortar un cuadrado de **acrílico, cartón rígido o plástico** de ~15×15 cm
2. Perforar 4 agujeros en los extremos formando una cruz
3. Pasar cada LDR por su agujero y soldar los cables por detrás
4. Fijar este soporte al **marco del panel solar** con bridas o cinta doble faz

### Opción 2: Protoboard pegada al panel (prototipo rápido)
1. Colocar los 4 LDR en una **protoboard pequeña**
2. Poner las resistencias de 10kΩ en la misma protoboard
3. Pasar cables a los GPIOs del ESP32
4. Pegar la protoboard al **borde superior del panel** con cinta doble faz

### Opción 3: Impresión 3D (más profesional)
1. Diseñar una pieza con 4 alojamientos para LDR en forma de cruz
2. Integrar al soporte mecánico del panel
3. Los LDR quedan firmes y alineados permanentemente

## Regla de Oro

Los LDR deben **girar con el panel**. Si el panel se mueve gracias al servo, los LDR montados sobre él también giran. Esto es necesario porque cuando el panel queda bien orientado al sol, los 4 LDR ven valores iguales y el error es 0.

## Verificación Rápida

Con el sistema encendido y los LDR conectados:

1. Tapar el LDR de arriba → el error de elevación debe ser positivo
2. Tapar el LDR de abajo → el error de elevación debe ser negativo
3. Tapar el LDR izquierdo → el error de azimut debe ser positivo
4. Tapar el LDR derecho → el error de azimut debe ser negativo
5. Iluminar todos parejo → ambos errores deben ser 0 (o dentro de la deadzone de 200)

---

## Parámetros del Test y Significado

Cuando ejecutás el test (cuando `main.c` solo tiene `power_monitor` + `ldr_sensor`), el monitor serial muestra cada 500ms:

```
I (1500) LDR_TEST: T:1234 B:1200 L:1150 R:1300 | az_err:+150 el_err:+34
```

### Parámetros individuales

| Parámetro | Rango | Significado |
|-----------|-------|-------------|
| **T (Top)** | 0 - 4095 | Promedio de luz de los LDRs **superiores** (TL + TR) / 2 |
| **B (Bottom)** | 0 - 4095 | Promedio de luz de los LDRs **inferiores** (BL + BR) / 2 |
| **L (Left)** | 0 - 4095 | Promedio de luz de los LDRs **izquierdos** (TL + BL) / 2 |
| **R (Right)** | 0 - 4095 | Promedio de luz de los LDRs **derechos** (TR + BR) / 2 |
| **az_err** | -4095 a +4095 | `L - R`. Positivo = más luz a la izquierda. Negativo = más luz a la derecha. |
| **el_err** | -4095 a +4095 | `T - B`. Positivo = más luz arriba. Negativo = más luz abajo. |

Los valores T, B, L, R ya vienen **filtrados** por un filtro complementario (80% histórico + 20% nuevo) para eliminar ruido.

### Zona muerta (deadzone)

Si `abs(error) < 200`, el error se fuerza a 0. Esto evita micro-correcciones constantes por fluctuaciones de luz. El valor de 200 representa ~5% del rango ADC completo (4095).

---

## Integración con el Proyecto Completo (con Servos)

### ¿Cómo se usan estos errores en el tracking?

En `main.c` (modo tracking completo), la tarea `solar_tracker_task` hace esto cada 100ms:

```c
ldr_sensor_get_errors(&az_err, &el_err);

if (az_err != 0 && !low_battery) {
    current_az += (az_err > 0) ? -ANGLE_STEP : ANGLE_STEP;  // mueve en dirección opuesta al error
    servo_motor_set_angle(SERVO_AXIS_AZIMUT, current_az);
}

if (el_err != 0 && !low_battery) {
    current_el += (el_err > 0) ? -ANGLE_STEP : ANGLE_STEP;
    servo_motor_set_angle(SERVO_AXIS_ELEVATION, current_el);
}
```

**Lógica:** si hay más luz a la izquierda (`az_err > 0`), el servo de azimut rota a la izquierda (resta ángulo). Si hay más luz a la derecha (`az_err < 0`), rota a la derecha (suma ángulo). Cuando los errores son 0, el panel está centrado al sol y no se mueve.

### Protección anti-brownout

Si la batería baja de **6.0V**, no se mueve ningún servo aunque haya error, para evitar que el ESP32 se reinicie por falta de tensión.

### Comportamiento esperado durante la integración

| Situación | az_err | el_err | ¿Los servos se mueven? |
|-----------|--------|--------|------------------------|
| Sol directamente al frente | 0 | 0 | No (panel centrado) |
| Sol se desplaza a la derecha | Negativo | 0 | Sí, servo azimut gira a la derecha |
| Sol se desplaza a la izquierda | Positivo | 0 | Sí, servo azimut gira a la izquierda |
| Sol sube (mediodía) | 0 | Positivo | Sí, servo elevación sube |
| Sol baja (tarde) | 0 | Negativo | Sí, servo elevación baja |
| Nublado / luz difusa | ~0 | ~0 | No (dentro de deadzone) |
| Batería baja (< 6.0V) | cualquier | cualquier | No (protección brownout) |

### Verificación del tracking en campo

1. Poné el sistema al sol y observá los servos: deben ajustarse lentamente (pasos de ±2° cada 100ms)
2. Tapá el LDR izquierdo con la mano → el servo de azimut debe girar a la izquierda (buscando más luz)
3. Tapá el LDR de abajo → el servo de elevación debe bajar
4. Dejá todo destapado al sol → los servos se quedan quietos y los errores son ~0
5. Monitoreá los logs:
   ```
   I (1500) SOLAR_TRACKER: az=90 el=90 | az_err=0 el_err=0 | bat=7.42V
   I (1600) SOLAR_TRACKER: az=88 el=90 | az_err=-150 el_err=0 | bat=7.41V
   ```

### Resolución de problemas

| Problema | Posible causa | Solución |
|----------|--------------|----------|
| Todos los valores son 0 | LDRs conectados al revés (GND ↔ 3.3V) o ADC no configurado | Verificar divisor de tensión |
| az_err siempre 0 | Deadzone demasiado grande o LDRs rotos | Tapar un LDR para forzar error > 200 |
| Servo no se mueve aunque az_err ≠ 0 | Batería baja (< 6.0V) | Cargar batería o alimentar por USB |
| Valores T, B, L, R fluctúan mucho | Mala conexión o cable muy largo | Verificar conexiones, usar cables más cortos |
| az_err positivo cuando tapo LDR derecho | LDRs intercambiados | Verificar GPIOs contra la tabla de conexión |
| Valores se ven siempre muy altos (cerca de 4000) | Sin resistencia 10kΩ a GND | Agregar resistencia faltante |
