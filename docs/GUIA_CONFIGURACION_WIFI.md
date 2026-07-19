# 📡 Guía de Configuración WiFi

**Cómo conectar el Rastreador Solar a cualquier red WiFi sin recompilar.**

---

## 📖 Descripción General

El ESP32 soporta dos mecanismos para obtener las credenciales WiFi:

| Mecanismo | Prioridad | Persistencia |
|-----------|-----------|--------------|
| **NVS** (Non-Volatile Storage) | Alta | Se conserva entre flashes, se borra con `idf.py erase_flash` |
| **Menuconfig** (`sdkconfig`) | Baja (fallback) | Se sobreescribe al compilar |

**Flujo de boot:**
```
Boot
 ├─ ¿Hay credenciales en NVS?
 │   ├─ Sí → Usar las de NVS
 │   └─ No → ¿Hay datos por USB serial?
 │       ├─ Sí (primeros 5s) → Guardar en NVS, reiniciar
 │       └─ No → Usar las de menuconfig (sdkconfig)
 └─ Conectar a WiFi
```

---

## 🔧 Método 1: Script de Configuración (Recomendado)

El script `daemon_pc/configure_wifi.py` hace **dos cosas**:

1. Escribe las credenciales en `firmware/sdkconfig` (para el próximo `idf.py build flash`)
2. Las envía por USB serial al ESP32 si está conectado (se guardan en NVS)

### Requisitos

```bash
# En el ESP32: tener flasheado un firmware que incluya el modo config serial
# (este feature, en feat/web-dashboard-telegram-bot)

# En la PC: tener nmcli (gestor de redes de Linux)
sudo apt install network-manager  # si no está instalado
```

### Uso interactivo

```bash
# Desde la raíz del proyecto
python daemon_pc/configure_wifi.py
```

Salida típica:
```
Redes WiFi disponibles:
  #  Seg  SSID
  ------------------
   1   SI  Centurion
   2   SI  Elon Musk
   3   NO  Biblioteca_Publica

Seleccioná una red (número): 1
Password para 'Centurion' (Enter si es red abierta):

Red destino: Centurion
  ✓ sdkconfig actualizado: SSID=Centurion
  ▶ Ahora corre: idf.py build flash

  Conectado a /dev/ttyUSB0. Esperando 'WAITING_CONFIG'...
  ✓ Credenciales enviadas por serial.

✓ Listo.
```

### Uso no interactivo

```bash
# Especificar SSID y password directamente
python daemon_pc/configure_wifi.py --ssid "Centurion" --password "clave123"

# Solo modificar sdkconfig (sin enviar por serial)
python daemon_pc/configure_wifi.py --ssid "Centurion" --password "clave123" --no-serial

# Especificar puerto serial manualmente
python daemon_pc/configure_wifi.py --port /dev/ttyUSB0

# Red abierta (sin password)
python daemon_pc/configure_wifi.py --ssid "Biblioteca_Publica"
```

---

## 📟 Método 2: Configuración Manual por Serial

Si el ESP32 no tiene credenciales en NVS, al bootear entra en **modo config serial** por 5 segundos:

```
WAITING_CONFIG
WAITING_CONFIG
WAITING_CONFIG  ← (cada 500ms)
```

Conectarse con cualquier terminal serial (115200 baud) y enviar:

```
WIFI:MiRed|MiPassword123          # Red segura
WIFI:Biblioteca_Publica|          # Red abierta (sin password)
```

El ESP32 responderá `OK` y reiniciará automáticamente.

```bash
# Usando screen
screen /dev/ttyUSB0 115200
# Esperar a ver WAITING_CONFIG, luego escribir:
WIFI:MiRed|MiPassword123
```

---

## 🔄 Método 3: Recompilar con otras credenciales

Si no querés usar el script ni el modo serial, podés cambiar las credenciales directamente en el código y recompilar:

### Opción A: Usando el script (solo escribe sdkconfig)

```bash
python daemon_pc/configure_wifi.py --no-serial --ssid "NuevaRed" --password "clave"
idf.py build flash
```

### Opción B: Manualmente

1. Editar `firmware/sdkconfig` (buscar `CONFIG_WIFI_SSID` y `CONFIG_WIFI_PASSWORD`)
2. O configurar con `idf.py menuconfig` → "Rastreador Solar Config" → WiFi SSID/Password
3. Compilar y flashear:
   ```bash
   idf.py build flash
   ```

---

## 🗑️ Borrar credenciales de NVS

Si querés que el ESP32 vuelva a usar las credenciales de fábrica (menuconfig):

```bash
# Opción 1: Borrar todo el flash (incluye firmware)
idf.py erase_flash flash

# Opción 2: Solo borrar NVS (sin reflashear)
python -c "
import serial, time
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=5)
time.sleep(2)
ser.write(b'WIFI:RESET|\n')
"
```

Esto fuerza al ESP32 a entrar en modo config serial en el próximo boot.

---

## 🔌 Diagrama de Flujo Completo

```
┌─────────────────────────────────────────────────────────┐
│                 ESP32 Power On                           │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
              ┌─────────────────────┐
              │  nvs_flash_init()    │
              └─────────┬───────────┘
                        │
                        ▼
              ┌─────────────────────┐
              │  ¿Credenciales en   │
              │  NVS?               │
              └─────────┬───────────┘
                   │          │
                  Sí          No
                   │          │
                   ▼          ▼
        ┌──────────────┐  ┌──────────────────────┐
        │ Usar NVS     │  │ Modo config serial    │
        │ SSID/PASS    │  │ (5 segundos)          │
        └──────┬───────┘  └──────────┬────────────┘
               │                     │
               │              ┌──────┴──────┐
               │              │ ¿Recibió     │
               │              │ WIFI:... ?   │
               │              └──────┬──────┘
               │                 │        │
               │                Sí        No
               │                 │        │
               │                 ▼        ▼
               │        ┌──────────┐ ┌──────────────┐
               │        │Guardar   │ │Usar menuconfig│
               │        │en NVS +  │ │(sdkconfig)   │
               │        │reboot    │ └──────┬───────┘
               │        └──────────┘        │
               │                            │
               └──────────────┬─────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ wifi_init_sta()   │
                    │ Conectar a WiFi   │
                    └──────────────────┘
```

---

## 🔐 Formato del protocolo serial

| Dirección | Formato | Ejemplo |
|-----------|---------|---------|
| PC → ESP32 | `WIFI:SSID\|PASSWORD\n` | `WIFI:Centurion\|G7m2kp9q\n` |
| PC → ESP32 | `WIFI:SSID\|\n` (red abierta) | `WIFI:Biblioteca\|\n` |
| ESP32 → PC | `WAITING_CONFIG\n` | (cada 500ms) |
| ESP32 → PC | `OK\n` | (confirmación) |

---

## 📂 Archivos involucrados

| Archivo | Rol |
|---------|-----|
| `firmware/components/udp_logger/udp_logger.c` | Lectura NVS, modo config serial, `udp_logger_save_wifi()` |
| `firmware/components/udp_logger/include/udp_logger.h` | Declaración de `udp_logger_save_wifi()` |
| `daemon_pc/configure_wifi.py` | Script Python: scan redes + escribir sdkconfig + envío serial |
| `firmware/sdkconfig` | Configuración de compilación (SSID/PASSWORD de fallback) |

---

## ⚠️ Notas importantes

- **NVS persiste entre `idf.py flash`** (no se borra al reflashear). Solo `idf.py erase_flash` lo elimina.
- Si el ESP32 ya tiene credenciales en NVS, **no entra en modo config serial**. Usá el script con `--no-serial` y reflasheá si necesitás cambiarlas.
- El script usa `nmcli` para escanear redes desde la PC (no desde el ESP32).
- Para redes abiertas, el campo password se envía vacío: `WIFI:SSID|\n`.
- El venv de Python incluye `pyserial`. Si no funciona, instalálo con `pip install pyserial`.
