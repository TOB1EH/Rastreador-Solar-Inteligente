#!/usr/bin/env python3
"""
Configura el WiFi del Rastreador Solar Inteligente.

Hace DOS cosas:
  1) Escribe las credenciales en firmware/sdkconfig (para el próximo build/flash).
  2) Las envía por USB serial al ESP32 si está conectado (se guardan en NVS).

Uso:
  python daemon_pc/configure_wifi.py
  python daemon_pc/configure_wifi.py --port /dev/ttyUSB0
  python daemon_pc/configure_wifi.py --ssid "MiRed" --password "clave" --no-scan
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import time

FIRMWARE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware")


def detect_port():
    ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    return ports[0] if ports else None


def scan_wifi_nmcli():
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "SSID,SECURITY", "device", "wifi", "list"],
            capture_output=True, text=True, timeout=15
        )
        seen = set()
        networks = []
        for line in result.stdout.strip().split("\n"):
            if ":" not in line:
                continue
            ssid, security = line.split(":", 1)
            ssid = ssid.strip()
            if not ssid or ssid == "--" or ssid in seen:
                continue
            seen.add(ssid)
            is_secure = security != "--" and "open" not in security.lower()
            networks.append({"ssid": ssid, "secure": is_secure})
        return networks
    except FileNotFoundError:
        return None
    except subprocess.TimeoutExpired:
        return None


def select_network_interactive(networks):
    title = f"{'#':>3}  {'Seg':>3}  SSID"
    print(f"\nRedes WiFi disponibles:\n{title}\n" + "-" * len(title))
    for i, net in enumerate(networks):
        lock = "SI" if net["secure"] else "NO"
        print(f"  {i+1:>2}   {lock:>3}  {net['ssid']}")
    print()
    while True:
        try:
            choice = int(input("Seleccioná una red (número): "))
            if 1 <= choice <= len(networks):
                return networks[choice - 1]
        except ValueError:
            pass
        print(f"  Elegí entre 1 y {len(networks)}.")


def ask_password(ssid):
    pw = input(f"Password para '{ssid}' (Enter si es red abierta): ")
    return pw


def write_sdkconfig(ssid, password, sdkconfig_path):
    if not os.path.exists(sdkconfig_path):
        print(f"  ✗ No se encuentra {sdkconfig_path}")
        return False

    with open(sdkconfig_path, "r") as f:
        content = f.read()

    content = re.sub(
        r'^CONFIG_WIFI_SSID=.*',
        f'CONFIG_WIFI_SSID="{ssid}"',
        content,
        flags=re.MULTILINE
    )
    content = re.sub(
        r'^CONFIG_WIFI_PASSWORD=.*',
        f'CONFIG_WIFI_PASSWORD="{password}"',
        content,
        flags=re.MULTILINE
    )

    with open(sdkconfig_path, "w") as f:
        f.write(content)

    print(f"  ✓ sdkconfig actualizado: SSID={ssid}")
    return True


def send_serial(port, ssid, password, baud=115200):
    try:
        import serial
    except ImportError:
        print("  - pyserial no instalado. Omitiendo envío serial.")
        print("    Instalálo con: pip install pyserial")
        return False

    try:
        ser = serial.Serial(port, baud, timeout=3)
    except serial.SerialException as e:
        print(f"  ✗ No se pudo abrir {port}: {e}")
        return False

    print(f"  Conectado a {port}. Esperando 'WAITING_CONFIG'...")
    deadline = time.time() + 12
    while time.time() < deadline:
        try:
            line = ser.readline().decode("utf-8", errors="replace").strip()
        except serial.SerialTimeoutException:
            continue
        if "WAITING_CONFIG" in line:
            msg = f"WIFI:{ssid}|{password}\n" if password else f"WIFI:{ssid}|\n"
            ser.write(msg.encode())
            print("  ✓ Credenciales enviadas por serial.")
            ser.close()
            return True
        time.sleep(0.1)

    print("  ✗ No se recibió WAITING_CONFIG del ESP32.")
    print("    Asegurate de que el ESP esté conectado y recién booteado.")
    ser.close()
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Configurar WiFi del Rastreador Solar Inteligente"
    )
    parser.add_argument("--port", help="Puerto serial del ESP32 (auto-detect si no se especifica)")
    parser.add_argument("--ssid", help="SSID de la red WiFi (omite escaneo)")
    parser.add_argument("--password", default="", help="Password de la red WiFi")
    parser.add_argument("--project-dir", default=FIRMWARE_DIR, help="Directorio del proyecto firmware")
    parser.add_argument("--no-serial", action="store_true", help="Solo escribir sdkconfig, sin enviar por serial")
    args = parser.parse_args()

    sdkconfig_path = os.path.join(args.project_dir, "sdkconfig")
    if not os.path.exists(sdkconfig_path):
        print(f"Error: no se encuentra {sdkconfig_path}")
        print("Ejecutá el script desde el directorio raíz del proyecto.")
        sys.exit(1)

    # 1. Obtener SSID
    ssid = args.ssid
    password = args.password

    if not ssid:
        networks = scan_wifi_nmcli()
        if networks is None:
            ssid = input("SSID de la red WiFi: ")
        elif not networks:
            print("  No se detectaron redes WiFi.")
            ssid = input("SSID de la red WiFi: ")
        else:
            net = select_network_interactive(networks)
            ssid = net["ssid"]
            if net["secure"]:
                password = ask_password(ssid)
            else:
                print(f"  Red abierta, sin password.")
                password = ""
    elif not password and not args.no_serial:
        password = ask_password(ssid)

    # 2. Escribir en sdkconfig
    print(f"\nRed destino: {ssid}")
    ok = write_sdkconfig(ssid, password, sdkconfig_path)
    if ok:
        print("  ▶ Ahora corre: idf.py build flash")
    else:
        print("  ▶ Editá manualmente CONFIG_WIFI_SSID en firmware/sdkconfig")

    # 3. Enviar por serial (opcional)
    if not args.no_serial:
        port = args.port or detect_port()
        if port:
            print()
            send_serial(port, ssid, password)
        else:
            print("\n  - No se detectó puerto serial. Omitiendo envío.")
            print("    Conectá el ESP32 por USB y pasá --port /dev/ttyUSB0")

    print("\n✓ Listo.")


if __name__ == "__main__":
    main()
