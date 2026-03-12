/**
 * @file reader_main.cpp
 * @brief Lector automático para tarjetas MIFARE Ultralight - ESP32-S3 CAM
 * @description Sistema automático que detecta tarjetas Ultralight y lee
 *              los 8 caracteres almacenados en las páginas 6 y 7.
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
    Serial.println(F("█    Sistema de Lectura Automática NFC            █"));
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
    Serial.println(F("\n¡Sistema listo! Iniciando modo de lectura Ultralight automática.\n"));
    Serial.println(F("Acerque una tarjeta Ultralight para leer los 8 caracteres almacenados."));
}

// ============================================================================
// Loop principal (modo automático de lectura Ultralight)
// 
// FUNCIONAMIENTO:
// 1. Detecta continuamente la presencia de tarjetas NFC/RFID
// 2. Valida que sea una tarjeta MIFARE Ultralight
// 3. Lee automáticamente la página 6 (4 caracteres)
// 4. Lee automáticamente la página 7 (4 caracteres)
// 5. Concatena ambas lecturas en un código de 8 caracteres
// 6. Muestra el código completo en la terminal
// 7. Ignora tarjetas no Ultralight
// 8. Espera 2 segundos anti-rebote antes de la siguiente detección
// ============================================================================
void loop() {
    // Interrogación continua del lector RC522
    if (rfid.isNewCardPresent()) {
        CardInfo info = rfid.readCardInfo();
        
        // VALIDACIÓN: Solo procesa tarjetas Ultralight
        if (info.detected && info.type == CARD_MIFARE_ULTRALIGHT) {
            Serial.println(F("[INFO] Tarjeta Ultralight detectada."));
            
            // LECTURA PÁGINA 6: Primeros 4 caracteres
            RC522Result page6Result = rfid.readUltralightPage(6);
            
            // LECTURA PÁGINA 7: Últimos 4 caracteres
            RC522Result page7Result = rfid.readUltralightPage(7);
            
            // VALIDACIÓN: Ambas páginas se leyeron correctamente
            if (page6Result.success && page7Result.success) {
                // CONCATENACIÓN: Combina los 8 caracteres
                String readCode = "";
                
                // Agregar caracteres de página 6 (primeros 4)
                for (byte i = 0; i < 4; i++) {
                    readCode += (char)page6Result.data[i];
                }
                
                // Agregar caracteres de página 7 (últimos 4)
                for (byte i = 0; i < 4; i++) {
                    readCode += (char)page7Result.data[i];
                }
                
                // MOSTRAR CÓDIGO: Presenta el código de 8 caracteres completo
                Serial.println(F("┌─────────────────────────────────────┐"));
                Serial.printf( "│ 🔐 CÓDIGO LEÍDO: %s\n", readCode.c_str());
                Serial.println(F("└─────────────────────────────────────┘"));
            } else {
                // MANEJO DE ERRORES: Reporta problemas en lectura
                Serial.println(F("[ERROR] Falló la lectura:"));
                if (!page6Result.success) {
                    Serial.print(F("[ERROR Página 6] "));
                    Serial.println(page6Result.message);
                }
                if (!page7Result.success) {
                    Serial.print(F("[ERROR Página 7] "));
                    Serial.println(page7Result.message);
                }
            }
        } else if (info.detected) {
            // Tarjeta rechazada si no es Ultralight
            Serial.printf("[INFO] Tarjeta detectada: %s (no es Ultralight, ignorando)\n", 
                          info.typeName.c_str());
        }
        
        // Finalización de sesión con tarjeta
        rfid.haltCard();
        
        // ⏱️ ESPERA ANTI-REBOTE: 2 segundos entre detecciones de la misma tarjeta
        delay(2000);
    }
    
    // Revisión frecuente del lector
    delay(100);
}
