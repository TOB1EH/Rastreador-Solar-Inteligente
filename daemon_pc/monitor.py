import socket
import time

# Configuración
ESP32_IP = "192.168.18.149"  # IP del ESP32 (verificar en log serial si cambia)
UDP_PORT = 8080            # Mismo puerto que configuramos en el ESP32

def main():
    print(f"Iniciando Daemon PC (Pull Mode)...")
    print(f"Consultando al ESP32 en {ESP32_IP}:{UDP_PORT}...\n")
    print("-" * 50)

    # Crear socket UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Configurar timeout de 2 segundos para no bloquearse
    sock.settimeout(2.0)

    try:
        while True:
            try:
                # Enviar solicitud (payload puede ser cualquier cosa, "GET" es semántico)
                sock.sendto(b"GET", (ESP32_IP, UDP_PORT))
                
                # Esperar respuesta
                data, addr = sock.recvfrom(1024) 
                
                # Decodificar el texto enviado por ESP32
                payload = data.decode('utf-8')
                
                # Formatear e imprimir en pantalla
                partes = payload.split('|')
                if len(partes) == 2:
                    bat_str = partes[0].replace('BAT:', '')
                    sol_str = partes[1].replace('SOL:', '')
                    print(f"📡 [ESP32 en {addr[0]}] -> Batería: {bat_str} V  |  Panel Solar: {sol_str} V")
                else:
                    print(f"📡 Recibido: {payload}")
                    
            except socket.timeout:
                print(f"⏳ Sin respuesta del ESP32 en {ESP32_IP}:{UDP_PORT}")
                
            # Esperar 1 segundo antes de la próxima consulta
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nDaemon detenido por el usuario.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
