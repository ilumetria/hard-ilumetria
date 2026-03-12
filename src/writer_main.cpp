/**
 * @file writer_main.cpp
 * @brief Escritor automático para tarjetas MIFARE Ultralight - ESP32-S3 CAM
 * @description Sistema automático que detecta tarjetas Ultralight y permite
 *              escribir 8 caracteres alfanuméricos divididos en dos páginas.
 * 
 * Conexiones ESP32-S3 CAM -> RC522:
 *   GPIO10 -> SDA (SS)
 *   GPIO12 -> SCK
 *   GPIO11 -> MOSI
 *   GPIO13 -> MISO
 *   GPIO9  -> RST
 *   3.3V   -> 3.3V
 *   GND    -> GND
 * 
 * @author Adrian Nava
 * @date 2026-03-11
 */

#include <Arduino.h>
#include "RC522Module.h"

// ============================================================================
// Instancia global del módulo RC522
// Maneja toda la comunicación con el lector RFID/NFC
// ============================================================================
RC522Module rfid;

// ============================================================================
// Prototipos de funciones
// ============================================================================
// Lee una línea completa del puerto serial (hasta Enter/Retorno)
// Retorna: String con los caracteres leídos (sin salto de línea)
// Timeout: 30 segundos de inactividad
String readSerialLine();

// ============================================================================
// Setup - Inicialización del sistema
// Ejecutado UNA SOLA VEZ al arrancar el dispositivo
// ============================================================================
void setup() {
    // Iniciar comunicación serial a 115200 baudios (USB CDC en ESP32-S3)
    Serial.begin(115200);
    delay(2000);  // Esperar 2 segundos para que el puerto USB esté listo
    
    // Banner de bienvenida
    Serial.println(F("\n\n"));
    Serial.println(F("████████████████████████████████████████████████████"));
    Serial.println(F("█                                                  █"));
    Serial.println(F("█    ESP32-S3 CAM + RC522 RFID/NFC Module         █"));
    Serial.println(F("█    Sistema de Lectura y Escritura NFC           █"));
    Serial.println(F("█                                                  █"));
    Serial.println(F("████████████████████████████████████████████████████\n"));
    
    // Inicializar el módulo RC522 con validación de conexión
    if (!rfid.begin()) {
        Serial.println(F("\n[FATAL] No se pudo inicializar el RC522."));
        Serial.println(F("Verifica las conexiones y reinicia el dispositivo."));
        while (true) {
            delay(1000);
        }
    }
    
    // Mensaje de inicio - Sistema listo para operar
    Serial.println(F("\n¡Sistema listo! Iniciando modo de escritura Ultralight automática.\n"));
    Serial.println(F("Acerque una tarjeta Ultralight y luego escriba 8 caracteres alfanuméricos por el puerto serie cuando se solicite."));
}

// ============================================================================
// Loop principal (modo automático de escritura Ultralight)
// 
// FUNCIONAMIENTO:
// 1. Detecta continuamente la presencia de tarjetas NFC/RFID
// 2. Valida que sea una tarjeta MIFARE Ultralight
// 3. Solicita 8 caracteres alfanuméricos por puerto serial
// 4. Filtra entrada (solo acepta a-z, A-Z, 0-9)
// 5. Escribe 4 caracteres en página 6
// 6. Escribe 4 caracteres en página 7
// 7. Confirma exitosamente y espera 1 segundo
// 8. Ignora tarjetas no Ultralight
// ============================================================================
void loop() {
    // Interrogación continua del lector RC522
    if (rfid.isNewCardPresent()) {
        CardInfo info = rfid.readCardInfo();
        
        // VALIDACIÓN: Solo procesa tarjetas Ultralight
        if (info.detected && info.type == CARD_MIFARE_ULTRALIGHT) {
            Serial.println(F("[INFO] Tarjeta Ultralight detectada."));
            Serial.println(F("Ingresa 8 caracteres alfanuméricos por el serial:"));
            
            String input = "";
            
            // LOOP DE ENTRADA: Valida y filtra caracteres alfanuméricos
            // Rechaza hasta que tenga 8 caracteres válidos (a-z, A-Z, 0-9)
            while (true) {
                input = readSerialLine();
                
                // FILTRADO: Extrae solo caracteres alfanuméricos
                String filtered = "";
                for (int i = 0; i < input.length(); i++) {
                    char c = input[i];
                    if (isalnum(c)) filtered += c;  // Solo a-z, A-Z, 0-9
                }
                
                // Acepta si tiene al menos 8 caracteres válidos
                if (filtered.length() >= 8) {
                    input = filtered.substring(0, 8);  // Toma solo los primeros 8
                    break;
                }
                Serial.println(F("[ERROR] Se requieren 8 caracteres alfanuméricos. Intenta de nuevo:"));
            }

            // SEPARACIÓN DE DATOS: Divide los 8 caracteres en dos grupos de 4
            // Página 6 recibe caracteres [0-3]
            // Página 7 recibe caracteres [4-7]
            byte page6[4];
            byte page7[4];
            for (int i = 0; i < 4; i++) {
                page6[i] = (byte)input[i];      // Primeros 4 caracteres
                page7[i] = (byte)input[i + 4];  // Últimos 4 caracteres
            }

            // ESCRITURA: Graba ambas páginas
            RC522Result r6 = rfid.writeUltralightPage(6, page6);
            RC522Result r7 = rfid.writeUltralightPage(7, page7);
            
            // CONFIRMACIÓN: Verifica éxito en ambas páginas
            if (r6.success && r7.success) {
                Serial.println(F("[OK] Escritura de 8 caracteres exitosa (páginas 6 y 7)."));
                
                // ⏱️ ESPERA DE 1 SEGUNDO: Periodo de confirmación antes de seguir
                delay(1000);
            } else {
                // MANEJO DE ERRORES: Reporta problemas en escritura
                Serial.println(F("[ERROR] Falló la escritura:"));
                if (!r6.success) Serial.println(r6.message);
                if (!r7.success) Serial.println(r7.message);
            }
        } else {
            // Tarjeta rechazada si no es Ultralight
            Serial.println(F("[INFO] Tarjeta detectada no es Ultralight, ignorando."));
        }
        
        // Finalización de sesión con tarjeta
        rfid.haltCard();
        
        // ⏱️ ESPERA ANTI-REBOTE: 1 segundo entre detecciones de la misma tarjeta
        delay(2000);
    }
    
    // Revisión frecuente del lector
    delay(100);
}

// ============================================================================
// Función: readSerialLine()
// 
// PROPÓSITO: Leer una línea completa del puerto serial del usuario
//
// ENTRADA: Puerto serial con caracteres ASCII
//
// PROCESO:
// 1. Espera datos en el buffer serial
// 2. Acumula caracteres hasta recibir Enter o Retorno (CR/LF)
// 3. HACE ECO de cada carácter en la terminal conforme se teclea
// 4. Retorna la cadena sin el salto de línea
//
// TIMEOUT: 30 segundos de inactividad (~240 iteraciones * 125ms)
// Si pasa el timeout, retorna la entrada acumulada hasta el momento
//
// EJEMPLO: Usuario escribe "ABCD1234" → Terminal muestra "ABCD1234"
// ============================================================================
String readSerialLine() {
    String input = "";
    unsigned long startTime = millis();
    
    // Bucle de lectura: continúa hasta Enter o hasta timeout
    while (millis() - startTime < 30000) {
        if (Serial.available()) {
            char c = Serial.read();
            
            // Detecta Enter (LF=10) o Retorno (CR=13)
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    Serial.println();  // Salto de línea tras el último carácter
                    return input;  // Retorna con contenido
                }
            } else {
                input += c;  // Acumula carácter
                Serial.print(c);  // 🔤 ECO: Muestra el carácter en terminal
            }
        }
        delay(10);  // Pequeña pausa para no saturar CPU
    }
    
    // Si se alcanza timeout, retorna lo acumulado
    Serial.println();
    return input;
}
