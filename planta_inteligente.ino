#include <LiquidCrystal.h>
#include <DHT.h>
#include <BluetoothSerial.h>

// Pines
#define PIN_SUELO 36    // Sensor de humedad del suelo (analógico)
#define PIN_BOMBA 23    // Minibomba de agua (digital, vía relé)
#define PIN_LDR 33      // Sensor LDR (analógico)
#define PIN_LED 26      // LED (digital)
#define PIN_PIR 27      // Sensor PIR (digital)
#define PIN_BUZZER 25   // Buzzer (digital)
#define PIN_FLAMA 19    // Sensor de flama (digital)
#define PIN_DHT 18      // Sensor DHT11 (digital)
#define PIN_COOLER 14   // Cooler (digital)

// Configurar LCD
LiquidCrystal lcd(15, 2, 4, 16, 17, 5); // RS, E, D4, D5, D6, D7

// Configurar DHT11
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

// Configurar Bluetooth
BluetoothSerial SerialBT;

// Variables globales
int humedad_suelo = 0;
int valor_suelo = 0;
int luz = 0;
int valor_luz = 0;
int movimiento = 0;
int movimiento_anterior = 0; // Para detectar cambios en PIR
bool buzzer_activo = false;  // Estado del buzzer para LCD/Serial
int flama = 0;              // Estado del sensor de flama
float temperatura = 0;      // Temperatura del DHT11 (°C)
float humedad_ambiental = 0; // Humedad ambiental del DHT11 (%)
unsigned long ultima_actualizacion = 0;
int pantalla_actual = 0; // 0: suelo, 1: LDR, 2: PIR, 3: flama, 4: DHT11
bool manualControl = false; // Modo manual (Bluetooth) o automático
bool bombaManual = false;   // Estado manual de la bomba
bool coolerManual = false;  // Estado manual del cooler
bool ledManual = false;     // Estado manual del LED

void setup() {
  // Iniciar Serial
  Serial.begin(9600);
  delay(1000); // Estabilizar Serial
  
  // Iniciar Bluetooth
  SerialBT.begin("ESP32_BTC"); // Nombre del dispositivo Bluetooth
  Serial.println("Bluetooth iniciado: ESP32_Control");
  
  // Configurar pines
  pinMode(PIN_SUELO, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_FLAMA, INPUT);
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_COOLER, OUTPUT);
  digitalWrite(PIN_BOMBA, HIGH); // Apagar bomba
  digitalWrite(PIN_LED, LOW);    // Apagar LED
  digitalWrite(PIN_BUZZER, LOW); // Apagar buzzer
  digitalWrite(PIN_COOLER, LOW); // Apagar cooler
  
  // Iniciar DHT11
  dht.begin();
  
  // Iniciar LCD
  lcd.begin(16, 2);
  lcd.print("Iniciando...");
  Serial.println("Iniciando sistema...");
  SerialBT.println("Iniciando sistema...");
  delay(3000); // Mostrar mensaje inicial
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Humedad:     %");
  lcd.setCursor(0, 1);
  lcd.print("Bomba:      ");
  Serial.println("Sistema listo");
  SerialBT.println("Sistema listo");
}

void leerSuelo() {
  valor_suelo = analogRead(PIN_SUELO);
  humedad_suelo = map(valor_suelo, 1000, 4000, 100, 0); // 1000=100% (húmedo), 4000=0% (seco)
  if (humedad_suelo < 0) humedad_suelo = 0;
  if (humedad_suelo > 100) humedad_suelo = 100;
}

void leerLDR() {
  valor_luz = analogRead(PIN_LDR);
  luz = map(valor_luz, 1000, 4000, 100, 0); // 1000=100% (mucha luz), 4000=0% (oscuridad)
  if (luz < 0) luz = 0;
  if (luz > 100) luz = 100;
}

void leerPIR() {
  movimiento = digitalRead(PIN_PIR); // 1 si detecta movimiento, 0 si no
}

void leerFlama() {
  flama = digitalRead(PIN_FLAMA); // 0 si detecta flama, 1 si no
}

void leerDHT() {
  float nueva_temperatura = dht.readTemperature(); // Leer temperatura (°C)
  float nueva_humedad = dht.readHumidity();       // Leer humedad ambiental (%)
  if (isnan(nueva_temperatura) || isnan(nueva_humedad)) {
    Serial.println("Error al leer el DHT11");
    SerialBT.println("Error al leer el DHT11");
  } else {
    temperatura = nueva_temperatura;
    humedad_ambiental = nueva_humedad;
    Serial.print("DHT11 OK - Temp: ");
    Serial.print(temperatura);
    Serial.print("C, Hum: ");
    Serial.print(humedad_ambiental);
    Serial.println("%");
    SerialBT.print("DHT11 OK - Temp: ");
    SerialBT.print(temperatura);
    SerialBT.print("C, Hum: ");
    SerialBT.print(humedad_ambiental);
    SerialBT.println("%");
  }
}

void controlarBomba() {
  if (manualControl && bombaManual) {
    digitalWrite(PIN_BOMBA, LOW); // Encender bomba
    Serial.println("Bomba: Encendida (Manual)");
    SerialBT.println("Bomba: Encendida (Manual)");
  } else if (manualControl) {
    digitalWrite(PIN_BOMBA, HIGH); // Apagar bomba
    Serial.println("Bomba: Apagada (Manual)");
    SerialBT.println("Bomba: Apagada (Manual)");
  } else if (flama == 0 || humedad_suelo == 0) { // Flama detectada o suelo seco
    digitalWrite(PIN_BOMBA, LOW); // Encender bomba
    Serial.println("Bomba: Encendida (Automático)");
    SerialBT.println("Bomba: Encendida (Automático)");
  } else { // Sin flama y suelo con humedad
    digitalWrite(PIN_BOMBA, HIGH); // Apagar bomba
    Serial.println("Bomba: Apagada (Automático)");
    SerialBT.println("Bomba: Apagada (Automático)");
  }
}

void controlarLED() {
  if (manualControl && ledManual) {
    digitalWrite(PIN_LED, HIGH); // Encender LED
    Serial.println("LED: Encendido (Manual)");
    SerialBT.println("LED: Encendido (Manual)");
  } else if (manualControl) {
    digitalWrite(PIN_LED, LOW); // Apagar LED
    Serial.println("LED: Apagado (Manual)");
    SerialBT.println("LED: Apagado (Manual)");
  } else if (luz < 30) { // Poca luz
    digitalWrite(PIN_LED, HIGH); // Encender LED
    Serial.println("LED: Encendido (Automático)");
    SerialBT.println("LED: Encendido (Automático)");
  } else { // Suficiente luz
    digitalWrite(PIN_LED, LOW); // Apagar LED
    Serial.println("LED: Apagado (Automático)");
    SerialBT.println("LED: Apagado (Automático)");
  }
}

void controlarBuzzer() {
  if (movimiento == 1 && movimiento_anterior == 0) { // Transición de sin movimiento a movimiento detectado
    tone(PIN_BUZZER, 1000, 500); // Tono de 1000 Hz durante 500 ms
    buzzer_activo = true;        // Marcar buzzer como activo
    delay(500);                  // Esperar la duración del tono
    noTone(PIN_BUZZER);          // Detener el tono
  } else {
    buzzer_activo = false;       // Buzzer inactivo después del beep
  }
  movimiento_anterior = movimiento; // Actualizar estado anterior
}

void controlarCooler() {
  if (manualControl && coolerManual) {
    digitalWrite(PIN_COOLER, LOW); // Encender cooler
    Serial.print("Cooler ON (Manual, Temp: ");
    Serial.print(temperatura);
    Serial.print("C, Pin: ");
    Serial.print(digitalRead(PIN_COOLER));
    Serial.println(")");
    SerialBT.print("Cooler ON (Manual, Temp: ");
    SerialBT.print(temperatura);
    SerialBT.println("C)");
  } else if (manualControl) {
    digitalWrite(PIN_COOLER, HIGH); // Apagar cooler (activo-bajo)
    Serial.print("Cooler OFF (Manual, Temp: ");
    Serial.print(temperatura);
    Serial.print("C, Pin: ");
    Serial.print(digitalRead(PIN_COOLER));
    Serial.println(")");
    SerialBT.print("Cooler OFF (Manual, Temp: ");
    SerialBT.print(temperatura);
    SerialBT.println("C)");
  } else if (temperatura >= 25) { // Temperatura alta
    digitalWrite(PIN_COOLER, LOW); // Encender cooler
    Serial.print("Cooler ON (Automático, Temp: ");
    Serial.print(temperatura);
    Serial.print("C, Pin: ");
    Serial.print(digitalRead(PIN_COOLER));
    Serial.println(")");
    SerialBT.print("Cooler ON (Automático, Temp: ");
    SerialBT.print(temperatura);
    SerialBT.println("C)");
  } else { // Temperatura baja
    digitalWrite(PIN_COOLER, HIGH); // Apagar cooler
    Serial.print("Cooler OFF (Automático, Temp: ");
    Serial.print(temperatura);
    Serial.print("C, Pin: ");
    Serial.print(digitalRead(PIN_COOLER));
    Serial.println(")");
    SerialBT.print("Cooler OFF (Automático, Temp: ");
    SerialBT.print(temperatura);
    SerialBT.println("C)");
  }

}

void manejarBluetooth() {
  if (SerialBT.available()) {
    String comando = SerialBT.readStringUntil('\n');
    comando.trim(); // Eliminar espacios o caracteres de nueva línea
    Serial.print("Comando recibido: ");
    Serial.println(comando);
    
    if (comando == "B1") {
      manualControl = true;
      bombaManual = true;
      SerialBT.println("Bomba encendida manualmente");
    } else if (comando == "B0") {
      manualControl = true;
      bombaManual = false;
      SerialBT.println("Bomba apagada manualmente");
    } else if (comando == "C1") {
      manualControl = true;
      coolerManual = true;
      SerialBT.println("Cooler encendido manualmente");
    } else if (comando == "C0") {
      manualControl = true;
      coolerManual = false;
      SerialBT.println("Cooler apagado manualmente");
    } else if (comando == "L1") {
      manualControl = true;
      ledManual = true;
      SerialBT.println("LED encendido manualmente");
    } else if (comando == "L0") {
      manualControl = true;
      ledManual = false;
      SerialBT.println("LED apagado manualmente");
    } else if (comando == "A") {
      manualControl = false;
      SerialBT.println("Modo automático activado");
    } else {
      SerialBT.println("Comando inválido. Usa: B1, B0, C1, C0, L1, L0, A");
    }
  }
}

void mostrarLCD() {
  if (pantalla_actual == 0) {
    // Mostrar datos del sensor de suelo y bomba
    lcd.setCursor(0, 0);
    lcd.print("Humedad:     %");
    lcd.setCursor(9, 0);
    if (humedad_suelo < 10) lcd.print(" ");
    lcd.print(humedad_suelo);
    lcd.print("%");
    
    lcd.setCursor(0, 1);
    lcd.print("Bomba:      ");
    lcd.setCursor(7, 1);
    lcd.print(digitalRead(PIN_BOMBA) ? "OFF" : "ON "); // Relé
  } else if (pantalla_actual == 1) {
    // Mostrar datos del LDR y LED
    lcd.setCursor(0, 0);
    lcd.print("Luz:         %");
    lcd.setCursor(5, 0);
    if (luz < 10) lcd.print(" ");
    lcd.print(luz);
    lcd.print("%");
    
    lcd.setCursor(0, 1);
    lcd.print("LED:        ");
    lcd.setCursor(5, 1);
    lcd.print(digitalRead(PIN_LED) ? "ON " : "OFF");
  } else if (pantalla_actual == 2) {
    // Mostrar datos del PIR y buzzer
    lcd.setCursor(0, 0);
    lcd.print(movimiento == 1 ? "PIR: Detectado " : "PIR: No Detecta");
    lcd.setCursor(0, 1);
    lcd.print("Buzzer:     ");
    lcd.setCursor(8, 1);
    lcd.print(buzzer_activo ? "ON " : "OFF");
  } else if (pantalla_actual == 3) {
    // Mostrar datos del sensor de flama y bomba
    lcd.setCursor(0, 0);
    lcd.print(flama == 0 ? "Flama: Detectada" : "Flama: No Detecta");
    lcd.setCursor(0, 1);
    lcd.print("Bomba:      ");
    lcd.setCursor(7, 1);
    lcd.print(digitalRead(PIN_BOMBA) ? "OFF" : "ON "); // Relé activo-bajo
  } else {
    // Mostrar datos del DHT11 y cooler
    lcd.setCursor(0, 0);
    lcd.print("Temp:    C Hum:  %");
    lcd.setCursor(6, 0);
    if (temperatura < 10) lcd.print(" ");
    lcd.print((int)temperatura);
    lcd.setCursor(13, 0);
    if (humedad_ambiental < 10) lcd.print(" ");
    lcd.print((int)humedad_ambiental);
    lcd.setCursor(0, 1);
    lcd.print("Cooler:     ");
    lcd.setCursor(8, 1);
    lcd.print(digitalRead(PIN_COOLER) ? "OFF " : "ON");
  }
}

void depurarSerial() {
  Serial.print("Humedad: ");
  Serial.print(humedad_suelo);
  Serial.print("% (Valor crudo: ");
  Serial.print(valor_suelo);
  Serial.print(") Bomba: ");
  Serial.println(digitalRead(PIN_BOMBA) ? "Apagada" : "Encendida");
  SerialBT.print("Humedad: ");
  SerialBT.print(humedad_suelo);
  SerialBT.print("% Bomba: ");
  SerialBT.println(digitalRead(PIN_BOMBA) ? "Apagada" : "Encendida");
  
  Serial.print("Luz: ");
  Serial.print(luz);
  Serial.print("% (Valor crudo: ");
  Serial.print(valor_luz);
  Serial.print(") LED: ");
  Serial.println(digitalRead(PIN_LED) ? "Encendido" : "Apagado");
  SerialBT.print("Luz: ");
  SerialBT.print(luz);
  SerialBT.print("% LED: ");
  SerialBT.println(digitalRead(PIN_LED) ? "Encendido" : "Apagado");
  
  Serial.print("PIR: ");
  Serial.print(movimiento == 1 ? "Detectado" : "No Detectado");
  Serial.print(" Buzzer: ");
  Serial.println(buzzer_activo ? "Encendido" : "Apagado");
  SerialBT.print("PIR: ");
  SerialBT.print(movimiento == 1 ? "Detectado" : "No Detectado");
  SerialBT.print(" Buzzer: ");
  SerialBT.println(buzzer_activo ? "Encendido" : "Apagado");
  
  Serial.print("Flama: ");
  Serial.print(flama == 0 ? "Detectada" : "No Detectada");
  Serial.print(" Bomba: ");
  Serial.println(digitalRead(PIN_BOMBA) ? "Apagada" : "Encendida");
  SerialBT.print("Flama: ");
  SerialBT.print(flama == 0 ? "Detectada" : "No Detectada");
  SerialBT.print(" Bomba: ");
  SerialBT.println(digitalRead(PIN_BOMBA) ? "Apagada" : "Encendida");
  
  Serial.print("Temp: ");
  Serial.print(temperatura);
  Serial.print("C Humedad Amb: ");
  Serial.print(humedad_ambiental);
  Serial.print("% Cooler: ");
  Serial.println(digitalRead(PIN_COOLER) ? "Encendido" : "Apagado");
  SerialBT.print("Temp: ");
  SerialBT.print(temperatura);
  SerialBT.print("C Humedad Amb: ");
  SerialBT.print(humedad_ambiental);
  SerialBT.print("% Cooler: ");
  SerialBT.println(digitalRead(PIN_COOLER) ? "Encendido" : "Apagado");
  
  Serial.print("Modo: ");
  Serial.println(manualControl ? "Manual" : "Automático");
  SerialBT.print("Modo: ");
  SerialBT.println(manualControl ? "Manual" : "Automático");
  Serial.println("---FIN_CICLO---");
  SerialBT.println("---FIN_CICLO---");
}

void loop() {
  leerSuelo();
  leerLDR();
  leerPIR();
  leerFlama();
  leerDHT();
  manejarBluetooth();
  controlarBomba();
  controlarLED();
  controlarBuzzer();
  controlarCooler();
  
  // Alternar pantallas cada 2 segundo
  unsigned long ahora = millis();
  if (ahora - ultima_actualizacion >= 2000) {
    lcd.clear(); // Limpiar para evitar residuos
    pantalla_actual = (pantalla_actual + 1) % 5; // Ciclar entre 0, 1, 2, 3, 4
    ultima_actualizacion = ahora;
  }
  
  mostrarLCD();
  depurarSerial();
  delay(1200); // Actualizar datos cada 1.2 segundos para estabilidad del DHT11
}