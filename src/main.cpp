/**
 * @file main.cpp
 * @brief Ejemplo de uso del módulo RC522 con ESP32-S3 CAM
 * @description Menú interactivo por Serial para lectura y escritura NFC/RFID
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
 * @date 2026-03-08
 */

#include <Arduino.h>
#include "RC522Module.h"

// ============================================================================
// Instancia global del módulo
// ============================================================================
RC522Module rfid;

// ============================================================================
// Prototipos de funciones del menú
// ============================================================================
void showMenu();
void menuDetectCard();
void menuReadBlock();
void menuWriteText();
void menuReadText();
void menuDumpCard();
void menuReadUltralight();
void menuWriteUltralight();
void menuContinuousRead();
String readSerialLine();
int readSerialInt();

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);  // Esperar a que el Serial esté listo (USB CDC)
    
    Serial.println(F("\n\n"));
    Serial.println(F("████████████████████████████████████████████████████"));
    Serial.println(F("█                                                  █"));
    Serial.println(F("█    ESP32-S3 CAM + RC522 RFID/NFC Module         █"));
    Serial.println(F("█    Sistema de Lectura y Escritura NFC           █"));
    Serial.println(F("█                                                  █"));
    Serial.println(F("████████████████████████████████████████████████████\n"));
    
    // Inicializar módulo RC522
    if (!rfid.begin()) {
        Serial.println(F("\n[FATAL] No se pudo inicializar el RC522."));
        Serial.println(F("Verifica las conexiones y reinicia el dispositivo."));
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println(F("\n¡Sistema listo! Usa el menú para interactuar.\n"));
    showMenu();
}

// ============================================================================
// Loop principal
// ============================================================================
void loop() {
    if (Serial.available()) {
        char option = Serial.read();
        
        // Limpiar buffer serial
        while (Serial.available()) Serial.read();
        
        Serial.println();
        
        switch (option) {
            case '1': menuDetectCard();       break;
            case '2': menuReadBlock();        break;
            case '3': menuWriteText();        break;
            case '4': menuReadText();         break;
            case '5': menuDumpCard();         break;
            case '6': menuReadUltralight();   break;
            case '7': menuWriteUltralight();  break;
            case '8': menuContinuousRead();   break;
            case '9':
                Serial.println(F("[INFO] Verificando conexión..."));
                if (rfid.isConnected()) {
                    Serial.println(F("[OK] Módulo RC522 conectado y funcionando"));
                    Serial.print(F("[INFO] Firmware: "));
                    Serial.println(rfid.getFirmwareVersion());
                } else {
                    Serial.println(F("[ERROR] Módulo RC522 no responde"));
                }
                break;
            case 'm':
            case 'M':
                showMenu();
                break;
            default:
                Serial.println(F("[?] Opción no válida. Presiona 'M' para ver el menú."));
                break;
        }
        
        Serial.println();
    }
    
    delay(50);
}

// ============================================================================
// Menú principal
// ============================================================================
void showMenu() {
    Serial.println(F("╔══════════════════════════════════════════════╗"));
    Serial.println(F("║              MENÚ PRINCIPAL                  ║"));
    Serial.println(F("╠══════════════════════════════════════════════╣"));
    Serial.println(F("║  1. Detectar tarjeta (info completa)        ║"));
    Serial.println(F("║  2. Leer bloque (MIFARE Classic)            ║"));
    Serial.println(F("║  3. Escribir texto (MIFARE Classic)         ║"));
    Serial.println(F("║  4. Leer texto  (MIFARE Classic)            ║"));
    Serial.println(F("║  5. Dump completo de tarjeta                ║"));
    Serial.println(F("║  6. Leer página (Ultralight/NTAG)           ║"));
    Serial.println(F("║  7. Escribir página (Ultralight/NTAG)       ║"));
    Serial.println(F("║  8. Lectura continua de UIDs                ║"));
    Serial.println(F("║  9. Verificar conexión RC522                ║"));
    Serial.println(F("║  M. Mostrar este menú                       ║"));
    Serial.println(F("╚══════════════════════════════════════════════╝"));
    Serial.println(F("Selecciona una opción:"));
}

// ============================================================================
// Opción 1: Detectar tarjeta
// ============================================================================
void menuDetectCard() {
    Serial.println(F("── Detectar Tarjeta ──────────────────────────"));
    
    CardInfo info = rfid.waitForCard(10000);  // Timeout 10 segundos
    
    if (!info.detected) {
        Serial.println(F("[!] No se detectó ninguna tarjeta"));
        return;
    }
    
    Serial.println(F("┌─────────────────────────────────────────┐"));
    Serial.printf( "│ UID:         %s\n", info.uidString.c_str());
    Serial.printf( "│ UID Length:  %d bytes\n", info.uidLength);
    Serial.printf( "│ Tipo:        %s\n", info.typeName.c_str());
    Serial.printf( "│ SAK:         0x%02X\n", info.sak);
    Serial.println(F("└─────────────────────────────────────────┘"));
    
    rfid.haltCard();
}

// ============================================================================
// Opción 2: Leer bloque específico
// ============================================================================
void menuReadBlock() {
    Serial.println(F("── Leer Bloque (MIFARE Classic) ─────────────"));
    Serial.println(F("Acerca la tarjeta al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    Serial.printf("Tarjeta detectada: %s\n", info.uidString.c_str());
    Serial.println(F("¿Qué bloque deseas leer? (0-63 para 1K):"));
    
    int blockNum = readSerialInt();
    if (blockNum < 0 || blockNum > 63) {
        Serial.println(F("[ERROR] Número de bloque inválido"));
        rfid.haltCard();
        return;
    }
    
    RC522Result result = rfid.readBlock((byte)blockNum);
    
    if (result.success) {
        Serial.println(F("\n┌─ Datos del bloque: ────────────────────┐"));
        RC522Module::printBlockData((byte)blockNum, result.data, result.dataLength);
        Serial.println(F("└────────────────────────────────────────┘"));
    } else {
        Serial.println("[ERROR] " + result.message);
    }
    
    rfid.haltCard();
}

// ============================================================================
// Opción 3: Escribir texto
// ============================================================================
void menuWriteText() {
    Serial.println(F("── Escribir Texto (MIFARE Classic) ──────────"));
    Serial.println(F("⚠  Los bloques 0 (fabricante) y Sector Trailers"));
    Serial.println(F("   (3,7,11,15...) están protegidos automáticamente.\n"));
    Serial.println(F("Acerca la tarjeta al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    Serial.printf("Tarjeta: %s (%s)\n", info.uidString.c_str(), info.typeName.c_str());
    
    Serial.println(F("Bloque inicial para escritura (recomendado: 1, 2, 4, 5, 6):"));
    int startBlock = readSerialInt();
    
    if (startBlock < 1 || startBlock > 62) {
        Serial.println(F("[ERROR] Bloque inválido"));
        rfid.haltCard();
        return;
    }
    
    if (RC522Module::isSectorTrailer(startBlock)) {
        Serial.println(F("[ERROR] No se puede escribir en un Sector Trailer"));
        rfid.haltCard();
        return;
    }
    
    Serial.println(F("Escribe el texto a guardar (máx ~48 chars para 3 bloques):"));
    String text = readSerialLine();
    
    if (text.length() == 0) {
        Serial.println(F("[ERROR] Texto vacío"));
        rfid.haltCard();
        return;
    }
    
    Serial.printf("[INFO] Escribiendo '%s' en bloque %d...\n", text.c_str(), startBlock);
    
    RC522Result result = rfid.writeText((byte)startBlock, text);
    
    if (result.success) {
        Serial.println("[OK] " + result.message);
        
        // Verificar escritura leyendo de vuelta
        Serial.println(F("[VERIFY] Verificando escritura..."));
        
        // Necesitamos re-detectar la tarjeta para leer
        rfid.haltCard();
        delay(200);
        
        if (rfid.isNewCardPresent()) {
            byte numBlocks = (text.length() / MIFARE_CLASSIC_BLOCK_SIZE) + 1;
            String readBack = rfid.readText((byte)startBlock, numBlocks);
            Serial.printf("[VERIFY] Leído: '%s'\n", readBack.c_str());
            
            if (readBack == text) {
                Serial.println(F("[OK] ¡Verificación exitosa! Los datos coinciden."));
            } else {
                Serial.println(F("[WARN] Los datos leídos no coinciden exactamente."));
            }
        }
    } else {
        Serial.println("[ERROR] " + result.message);
    }
    
    rfid.haltCard();
}

// ============================================================================
// Opción 4: Leer texto
// ============================================================================
void menuReadText() {
    Serial.println(F("── Leer Texto (MIFARE Classic) ──────────────"));
    Serial.println(F("Acerca la tarjeta al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    Serial.printf("Tarjeta: %s\n", info.uidString.c_str());
    
    Serial.println(F("Bloque inicial de lectura:"));
    int startBlock = readSerialInt();
    
    Serial.println(F("¿Cuántos bloques leer? (1-16):"));
    int numBlocks = readSerialInt();
    
    if (numBlocks < 1 || numBlocks > 16) numBlocks = 1;
    
    String text = rfid.readText((byte)startBlock, (byte)numBlocks);
    
    Serial.println(F("\n┌─ Texto leído: ─────────────────────────┐"));
    Serial.printf( "│ \"%s\"\n", text.c_str());
    Serial.printf( "│ Longitud: %d caracteres\n", text.length());
    Serial.println(F("└────────────────────────────────────────┘"));
    
    rfid.haltCard();
}

// ============================================================================
// Opción 5: Dump completo
// ============================================================================
void menuDumpCard() {
    Serial.println(F("── Dump Completo de Tarjeta ─────────────────"));
    Serial.println(F("Acerca la tarjeta al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    rfid.dumpCardToSerial();
    rfid.haltCard();
}

// ============================================================================
// Opción 6: Leer página Ultralight
// ============================================================================
void menuReadUltralight() {
    Serial.println(F("── Leer Página (MIFARE Ultralight/NTAG) ─────"));
    Serial.println(F("Acerca el tag NFC al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    if (info.type != CARD_MIFARE_ULTRALIGHT) {
        Serial.println(F("[WARN] Esta tarjeta no parece ser Ultralight/NTAG."));
        Serial.println(F("       Continuando de todas formas..."));
    }
    
    Serial.println(F("Número de página a leer (0-44 para NTAG213):"));
    int page = readSerialInt();
    
    RC522Result result = rfid.readUltralightPage((byte)page);
    
    if (result.success) {
        Serial.printf("Página %d: ", page);
        for (byte i = 0; i < result.dataLength; i++) {
            if (result.data[i] < 0x10) Serial.print("0");
            Serial.print(result.data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("[ERROR] " + result.message);
    }
    
    rfid.haltCard();
}

// ============================================================================
// Opción 7: Escribir página Ultralight
// ============================================================================
void menuWriteUltralight() {
    Serial.println(F("── Escribir Página (MIFARE Ultralight/NTAG) ──"));
    Serial.println(F("⚠  Páginas 0-3 están protegidas (solo lectura)\n"));
    Serial.println(F("Acerca el tag NFC al lector..."));
    
    CardInfo info = rfid.waitForCard(10000);
    if (!info.detected) return;
    
    Serial.println(F("Número de página (4+):"));
    int page = readSerialInt();
    
    if (page < 4) {
        Serial.println(F("[ERROR] Las páginas 0-3 no se pueden escribir"));
        rfid.haltCard();
        return;
    }
    
    Serial.println(F("Escribe 4 caracteres (se rellenará con 0x00):"));
    String input = readSerialLine();
    
    byte pageData[4] = {0};
    for (int i = 0; i < 4 && i < (int)input.length(); i++) {
        pageData[i] = (byte)input[i];
    }
    
    RC522Result result = rfid.writeUltralightPage((byte)page, pageData);
    
    if (result.success) {
        Serial.println("[OK] " + result.message);
    } else {
        Serial.println("[ERROR] " + result.message);
    }
    
    rfid.haltCard();
}

// ============================================================================
// Opción 8: Lectura continua de UIDs
// ============================================================================
void menuContinuousRead() {
    Serial.println(F("── Lectura Continua de UIDs ─────────────────"));
    Serial.println(F("Acerca tarjetas al lector. Presiona cualquier"));
    Serial.println(F("tecla para detener.\n"));
    
    int cardCount = 0;
    String lastUID = "";
    unsigned long lastDetection = 0;
    
    while (!Serial.available()) {
        if (rfid.isNewCardPresent()) {
            CardInfo info = rfid.readCardInfo();
            
            if (info.detected) {
                // Evitar reportar la misma tarjeta muy rápido
                if (info.uidString != lastUID || (millis() - lastDetection > 2000)) {
                    cardCount++;
                    Serial.printf("[#%d] UID: %s | Tipo: %s\n", 
                                  cardCount, info.uidString.c_str(), 
                                  info.typeName.c_str());
                    lastUID = info.uidString;
                    lastDetection = millis();
                }
            }
            
            rfid.haltCard();
        }
        
        delay(200);
    }
    
    // Limpiar buffer serial
    while (Serial.available()) Serial.read();
    
    Serial.printf("\n[INFO] Lectura continua finalizada. %d tarjetas detectadas.\n", 
                  cardCount);
}

// ============================================================================
// Helpers para entrada Serial
// ============================================================================

String readSerialLine() {
    String input = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < 30000) {  // Timeout 30 segundos
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    return input;
                }
            } else {
                input += c;
            }
        }
        delay(10);
    }
    
    return input;
}

int readSerialInt() {
    String input = readSerialLine();
    return input.toInt();
}
