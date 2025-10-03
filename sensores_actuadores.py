import serial
import sqlite3
import re
import time
from datetime import datetime

# Configuración
PUERTO_SERIAL = 'COM8'  # COM8 PUERTO
BAUDRATE = 9600
DB_FILE = 'sensores.db'

# Conectar a SQLite
conn = sqlite3.connect(DB_FILE)
cursor = conn.cursor()

# Crear tabla si no existe
cursor.execute('''
CREATE TABLE IF NOT EXISTS datos_sensores (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT,
    humedad_suelo INTEGER,
    valor_suelo_crudo INTEGER,
    estado_bomba INTEGER,  -- 0=Encendida, 1=Apagada (activo-bajo)
    luz INTEGER,
    valor_luz_crudo INTEGER,
    estado_led INTEGER,    -- 0=Apagado, 1=Encendido
    movimiento INTEGER,    -- 0=No Detectado, 1=Detectado
    estado_buzzer INTEGER, -- 0=Apagado, 1=Encendido
    flama INTEGER,         -- 0=Detectada, 1=No Detectada
    temperatura REAL,
    humedad_ambiental REAL,
    estado_cooler INTEGER, -- 0=Encendido, 1=Apagado (activo-bajo)
    modo INTEGER           -- 0=Automático, 1=Manual
)
''')
conn.commit()

# Variables para acumular datos de un ciclo
datos_ciclo = {}

def parsear_linea(linea):
    linea = linea.strip()
    print(f"Línea recibida: '{linea}'")  # Depuración
    if not linea:
        return None
    
    # Patrones regex para extraer datos
    patrones = {
        'humedad': re.match(r'Humedad: *(\d+)% *\(Valor crudo: *(\d+)\) *Bomba: *(Encendida|Apagada)', linea),
        'luz': re.match(r'Luz: *(\d+)% *\(Valor crudo: *(\d+)\) *LED: *(Encendido|Apagado)', linea),
        'pir': re.match(r'PIR: *(Detectado|No Detectado) *Buzzer: *(Encendido|Apagado)', linea),
        'flama': re.match(r'Flama: *(Detectada|No Detectada) *Bomba: *(Encendida|Apagada)', linea),
        'dht': re.match(r'Temp: *(-?[\d.]+)C *Humedad Amb: *(-?[\d.]+)% *Cooler: *(Encendido|Apagado)', linea),
        'modo': re.match(r'Modo: *(Manual|Automático)', linea),
        'fin': re.match(r'---FIN_CICLO---', linea)  # Cambiado a regex para consistencia
    }
    
    for clave, match in patrones.items():
        if match:
            if clave != 'fin':
                print(f"Coincidencia encontrada: {clave}, Grupos: {match.groups()}")  # Patrones con grupos
            else:
                print(f"Coincidencia encontrada: {clave}")
            if clave == 'humedad':
                datos_ciclo['humedad_suelo'] = int(match.group(1))
                datos_ciclo['valor_suelo_crudo'] = int(match.group(2))
                datos_ciclo['estado_bomba'] = 0 if match.group(3) == 'Encendida' else 1
            elif clave == 'luz':
                datos_ciclo['luz'] = int(match.group(1))
                datos_ciclo['valor_luz_crudo'] = int(match.group(2))
                datos_ciclo['estado_led'] = 1 if match.group(3) == 'Encendido' else 0
            elif clave == 'pir':
                datos_ciclo['movimiento'] = 1 if match.group(1) == 'Detectado' else 0
                datos_ciclo['estado_buzzer'] = 1 if match.group(2) == 'Encendido' else 0
            elif clave == 'flama':
                datos_ciclo['flama'] = 0 if match.group(1) == 'Detectada' else 1
            elif clave == 'dht':
                datos_ciclo['temperatura'] = float(match.group(1))
                datos_ciclo['humedad_ambiental'] = float(match.group(2))
                datos_ciclo['estado_cooler'] = 0 if match.group(3) == 'Encendido' else 1
            elif clave == 'modo':
                datos_ciclo['modo'] = 1 if match.group(1) == 'Manual' else 0
            elif clave == 'fin':
                return 'FIN'
    return None

def guardar_datos():
    if len(datos_ciclo) >= 10:  # Aceptar ciclos con al menos 10 campos
        timestamp = datetime.now().isoformat()
        cursor.execute('''
        INSERT INTO datos_sensores 
        (timestamp, humedad_suelo, valor_suelo_crudo, estado_bomba, luz, valor_luz_crudo, 
         estado_led, movimiento, estado_buzzer, flama, temperatura, humedad_ambiental, 
         estado_cooler, modo)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            timestamp, 
            datos_ciclo.get('humedad_suelo'), 
            datos_ciclo.get('valor_suelo_crudo'), 
            datos_ciclo.get('estado_bomba'),
            datos_ciclo.get('luz'), 
            datos_ciclo.get('valor_luz_crudo'), 
            datos_ciclo.get('estado_led'),
            datos_ciclo.get('movimiento'), 
            datos_ciclo.get('estado_buzzer'), 
            datos_ciclo.get('flama'),
            datos_ciclo.get('temperatura'), 
            datos_ciclo.get('humedad_ambiental'),
            datos_ciclo.get('estado_cooler'), 
            datos_ciclo.get('modo')
        ))
        conn.commit()
        print(f"Datos guardados en SQLite en {timestamp}: Humedad suelo {datos_ciclo.get('humedad_suelo', 'N/A')}%, Temp {datos_ciclo.get('temperatura', 'N/A')}°C")
        datos_ciclo.clear()
    else:
        print(f"Datos incompletos, solo {len(datos_ciclo)}/14 campos recibidos.")

# Conectar al serial
try:
    ser = serial.Serial(PUERTO_SERIAL, BAUDRATE, timeout=1)
    print(f"Conectado a {PUERTO_SERIAL}. Guardando datos en {DB_FILE}...")
    time.sleep(2)  # Esperar estabilización del ESP32

    while True:
        linea = ser.readline().decode('utf-8', errors='ignore')
        resultado = parsear_linea(linea)
        if resultado == 'FIN':
            guardar_datos()
        elif resultado is None:
            pass  # Ignorar líneas

except serial.SerialException as e:
    print(f"Error en serial: {e}. Verifica el puerto y conexión USB.")
except KeyboardInterrupt:
    print("\nDeteniendo...")
finally:
    if 'ser' in locals():
        ser.close()
    conn.close()
    print(f"Base de datos cerrada. Archivos guardados en: {DB_FILE}")