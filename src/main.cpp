/**
 * ============================================================================
 * UHF RFID Reader - Programa Principal para ESP32-C3
 * ============================================================================
 * 
 * Ejemplo completo con menú interactivo por serial para:
 *   1. Lectura individual de etiquetas
 *   2. Lectura múltiple (inventory)
 *   3. Lectura continua con callback
 *   4. Leer memoria EPC
 *   5. Leer memoria TID
 *   6. Leer memoria de Usuario
 *   7. Escribir nuevo EPC
 *   8. Escribir memoria de Usuario
 *   9. Configurar potencia
 *   10. Configurar región de frecuencia
 *   11. Obtener versión del módulo
 *   12. Toggle buzzer
 * 
 * Conexiones ESP32-C3 -> Módulo UHF RFID (R16):
 * ┌─────────────────────────────────────────────────┐
 * │  ESP32-C3          Módulo UHF R16               │
 * │  ─────────         ──────────────               │
 * │  GPIO 20 (RX) <──  TX (pin serial / RS232 TX)   │
 * │  GPIO 21 (TX) ──>  RX (pin serial / RS232 RX)   │
 * │  GND          ───  GND                          │
 * │                                                 │
 * │  ⚠ IMPORTANTE:                                  │
 * │  - El módulo R16 se alimenta a 12V DC           │
 * │  - Si usa RS232 real (±12V), necesitas un       │
 * │    convertidor MAX3232 entre el ESP32 y módulo   │
 * │  - Si la interfaz es TTL (3.3V/5V), puedes      │
 * │    conectar directamente con divisor de voltaje  │
 * │    si es 5V                                      │
 * └─────────────────────────────────────────────────┘
 * 
 * Velocidad por defecto del módulo R16: 9600 bps
 * (Ajustar UHF_BAUD_RATE si tu módulo usa otra velocidad)
 * 
 * ============================================================================
 */

#include <Arduino.h>
#include "UHF_RFID.h"

// ============================================================================
// Configuración de Pines
// ============================================================================

// Pines UART para comunicación con el módulo UHF
// ESP32-C3 tiene UART0 (USB/debug) y UART1 (disponible para periféricos)
#define UHF_RX_PIN     20    // GPIO20 - RX del ESP32 (conectar a TX del módulo)
#define UHF_TX_PIN     21    // GPIO21 - TX del ESP32 (conectar a RX del módulo)

// Velocidad de comunicación serial con el módulo UHF
// El R16 viene configurado a 9600 bps por defecto
// Muchos módulos UHF usan 115200 bps
// ⚡ AJUSTAR según tu módulo ⚡
#define UHF_BAUD_RATE  115200

// ============================================================================
// Instancias Globales
// ============================================================================

// Usar Serial1 para comunicación con el módulo UHF
// (Serial/Serial0 se usa para debug por USB)
HardwareSerial uhfSerial(1);
UHF_RFID uhfReader(uhfSerial);

// Buffer para almacenar etiquetas en lectura múltiple
UHF_TagInfo tags[UHF_MAX_TAGS];
uint8_t tagsFound = 0;

// Flag para lectura continua
bool continuousMode = false;

// ============================================================================
// Callback para lectura continua
// ============================================================================

void onTagFound(const UHF_TagInfo &tag) {
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║           🏷️  ETIQUETA DETECTADA             ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf( "║  EPC:  %s\n", tag.getEPCString().c_str());
    Serial.printf( "║  RSSI: -%d dBm\n", tag.rssi);
    Serial.printf( "║  PC:   0x%04X\n", tag.pc);
    Serial.println("╚══════════════════════════════════════════════╝");
}

// ============================================================================
// Funciones de Menú
// ============================================================================

void printMenu() {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║         🔖 UHF RFID READER - MENÚ PRINCIPAL           ║");
    Serial.println("╠══════════════════════════════════════════════════════════╣");
    Serial.println("║                                                        ║");
    Serial.println("║   📖 LECTURA                                          ║");
    Serial.println("║   [1] Lectura Individual (Single Poll)                 ║");
    Serial.println("║   [2] Lectura Múltiple (Multi Poll / Inventory)        ║");
    Serial.println("║   [3] Lectura Continua (Start/Stop)                    ║");
    Serial.println("║                                                        ║");
    Serial.println("║   💾 MEMORIA                                          ║");
    Serial.println("║   [4] Leer EPC de etiqueta                            ║");
    Serial.println("║   [5] Leer TID de etiqueta                            ║");
    Serial.println("║   [6] Leer Memoria de Usuario                         ║");
    Serial.println("║                                                        ║");
    Serial.println("║   ✏️  ESCRITURA                                        ║");
    Serial.println("║   [7] Escribir nuevo EPC                              ║");
    Serial.println("║   [8] Escribir Memoria de Usuario                     ║");
    Serial.println("║                                                        ║");
    Serial.println("║   ⚙️  CONFIGURACIÓN                                    ║");
    Serial.println("║   [9] Configurar Potencia (dBm)                       ║");
    Serial.println("║   [A] Configurar Región de Frecuencia                 ║");
    Serial.println("║   [B] Obtener Info del Módulo                         ║");
    Serial.println("║   [C] Toggle Buzzer ON/OFF                            ║");
    Serial.println("║                                                        ║");
    Serial.println("║   [H] Mostrar este menú                               ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.print(">> Selecciona opción: ");
}

void printSeparator() {
    Serial.println("────────────────────────────────────────────────");
}

// ============================================================================
// Función: Lectura Individual
// ============================================================================

void doSingleRead() {
    printSeparator();
    Serial.println("📖 Lectura Individual - Acerca una etiqueta...");
    printSeparator();
    
    UHF_TagInfo tag;
    UHF_Status status = uhfReader.singleRead(tag);
    
    if (status == UHF_OK) {
        Serial.println("✅ Etiqueta encontrada:");
        Serial.printf("   EPC:  %s\n", tag.getEPCString().c_str());
        Serial.printf("   RSSI: -%d dBm\n", tag.rssi);
        Serial.printf("   PC:   0x%04X\n", tag.pc);
        Serial.printf("   EPC Length: %d bytes\n", tag.epcLen);
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Lectura Múltiple
// ============================================================================

void doMultiRead() {
    printSeparator();
    Serial.println("📖 Lectura Múltiple - Escaneando durante 5 segundos...");
    printSeparator();
    
    tagsFound = 0;
    UHF_Status status = uhfReader.multiRead(tags, UHF_MAX_TAGS, tagsFound, 5000);
    
    if (status == UHF_OK) {
        Serial.printf("✅ Se encontraron %d etiqueta(s):\n\n", tagsFound);
        
        for (uint8_t i = 0; i < tagsFound; i++) {
            Serial.printf("   Tag #%d:\n", i + 1);
            Serial.printf("      EPC:  %s\n", tags[i].getEPCString().c_str());
            Serial.printf("      RSSI: -%d dBm\n", tags[i].rssi);
            Serial.printf("      PC:   0x%04X\n", tags[i].pc);
            Serial.println();
        }
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Lectura Continua
// ============================================================================

void toggleContinuousRead() {
    if (continuousMode) {
        Serial.println("⏹️  Deteniendo lectura continua...");
        uhfReader.stopRead();
        continuousMode = false;
        Serial.println("✅ Lectura continua detenida.");
    } else {
        printSeparator();
        Serial.println("▶️  Iniciando lectura continua...");
        Serial.println("   (Presiona '3' para detener)");
        printSeparator();
        
        uhfReader.startContinuousRead(onTagFound, 0); // 0 = indefinido
        continuousMode = true;
    }
}

// ============================================================================
// Función: Leer EPC
// ============================================================================

void doReadEPC() {
    printSeparator();
    Serial.println("💾 Leyendo banco EPC de la etiqueta...");
    printSeparator();
    
    uint8_t data[UHF_MAX_DATA_LENGTH];
    uint8_t dataLen = 0;
    
    // Leer EPC: banco 1, dirección 1 (PC), 8 words (PC + EPC completo)
    UHF_Status status = uhfReader.readTagMemory(MEM_BANK_EPC, 0x01, 0x08, data, dataLen);
    
    if (status == UHF_OK) {
        Serial.println("✅ Banco EPC leído:");
        Serial.print("   PC + EPC: ");
        for (int i = 0; i < dataLen; i++) {
            if (data[i] < 0x10) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        if (dataLen >= 2) {
            uint16_t pc = ((uint16_t)data[0] << 8) | data[1];
            Serial.printf("   PC Word: 0x%04X\n", pc);
            Serial.print("   EPC:     ");
            for (int i = 2; i < dataLen; i++) {
                if (data[i] < 0x10) Serial.print("0");
                Serial.print(data[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Leer TID
// ============================================================================

void doReadTID() {
    printSeparator();
    Serial.println("💾 Leyendo TID de la etiqueta...");
    printSeparator();
    
    UHF_TagInfo tag;
    UHF_Status status = uhfReader.readTID(tag);
    
    if (status == UHF_OK) {
        Serial.println("✅ TID leído:");
        Serial.printf("   TID: %s\n", tag.getDataString().c_str());
        Serial.printf("   Longitud: %d bytes\n", tag.dataLen);
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Leer Memoria de Usuario
// ============================================================================

void doReadUserMemory() {
    printSeparator();
    Serial.println("💾 Leyendo memoria de usuario...");
    printSeparator();
    
    uint8_t data[UHF_MAX_DATA_LENGTH];
    uint8_t dataLen = 0;
    
    // Leer 8 words (16 bytes) desde la dirección 0
    UHF_Status status = uhfReader.readUserMemory(0x00, 0x08, data, dataLen);
    
    if (status == UHF_OK) {
        Serial.println("✅ Memoria de usuario leída:");
        Serial.print("   Datos (HEX): ");
        for (int i = 0; i < dataLen; i++) {
            if (data[i] < 0x10) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        // Mostrar como texto ASCII si es imprimible
        Serial.print("   Datos (ASCII): ");
        for (int i = 0; i < dataLen; i++) {
            if (data[i] >= 32 && data[i] <= 126) {
                Serial.print((char)data[i]);
            } else {
                Serial.print('.');
            }
        }
        Serial.println();
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Escribir EPC
// ============================================================================

void doWriteEPC() {
    printSeparator();
    Serial.println("✏️  Escribir nuevo EPC en etiqueta");
    Serial.println("   ⚠ IMPORTANTE: Solo UNA etiqueta debe estar en el campo");
    Serial.println();
    Serial.println("   Ingresa el nuevo EPC en hexadecimal (sin espacios)");
    Serial.println("   Ejemplo: E20068226013007026300E4A (12 bytes = 24 caracteres hex)");
    Serial.println("   O usa uno corto para prueba: 112233445566 (6 bytes)");
    Serial.print("   EPC> ");
    
    // Esperar input del usuario
    while (!Serial.available()) {
        delay(10);
    }
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    
    if (input.length() < 4 || input.length() % 2 != 0) {
        Serial.println("❌ Error: EPC debe tener longitud par y mínimo 4 caracteres hex");
        return;
    }
    
    // Convertir hex string a bytes
    uint8_t epcLen = input.length() / 2;
    uint8_t newEPC[UHF_MAX_EPC_LENGTH];
    
    for (uint8_t i = 0; i < epcLen; i++) {
        String byteStr = input.substring(i * 2, i * 2 + 2);
        newEPC[i] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
    }
    
    Serial.println();
    Serial.print("   Nuevo EPC a escribir: ");
    for (int i = 0; i < epcLen; i++) {
        if (newEPC[i] < 0x10) Serial.print("0");
        Serial.print(newEPC[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.println("   ¿Confirmar escritura? (S/N)");
    Serial.print("   > ");
    
    while (!Serial.available()) {
        delay(10);
    }
    
    String confirm = Serial.readStringUntil('\n');
    confirm.trim();
    confirm.toUpperCase();
    
    if (confirm != "S" && confirm != "Y") {
        Serial.println("   Escritura cancelada.");
        return;
    }
    
    Serial.println("   Escribiendo...");
    UHF_Status status = uhfReader.writeEPC(newEPC, epcLen);
    
    if (status == UHF_OK) {
        Serial.println("   ✅ EPC escrito exitosamente!");
        Serial.println("   Realizando lectura de verificación...");
        
        // Verificar leyendo la etiqueta
        delay(500);
        UHF_TagInfo tag;
        if (uhfReader.singleRead(tag) == UHF_OK) {
            Serial.printf("   EPC verificado: %s\n", tag.getEPCString().c_str());
        }
    } else {
        Serial.printf("   ❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Escribir Memoria de Usuario
// ============================================================================

void doWriteUserMemory() {
    printSeparator();
    Serial.println("✏️  Escribir en Memoria de Usuario");
    Serial.println("   ⚠ IMPORTANTE: Solo UNA etiqueta debe estar en el campo");
    Serial.println();
    Serial.println("   Ingresa los datos en hexadecimal (sin espacios)");
    Serial.println("   La longitud debe ser múltiplo de 4 caracteres (2 bytes = 1 word)");
    Serial.println("   Ejemplo: 48656C6C6F21 = 'Hello!' (6 bytes)");
    Serial.print("   DATA> ");
    
    while (!Serial.available()) {
        delay(10);
    }
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    
    if (input.length() < 4 || input.length() % 4 != 0) {
        Serial.println("❌ Error: Datos deben ser múltiplo de 4 caracteres hex (2 bytes = 1 word)");
        return;
    }
    
    uint8_t dataLen = input.length() / 2;
    uint8_t data[UHF_MAX_DATA_LENGTH];
    
    for (uint8_t i = 0; i < dataLen; i++) {
        String byteStr = input.substring(i * 2, i * 2 + 2);
        data[i] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
    }
    
    Serial.println();
    Serial.print("   Datos a escribir: ");
    for (int i = 0; i < dataLen; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.println("   ¿Confirmar escritura? (S/N)");
    Serial.print("   > ");
    
    while (!Serial.available()) {
        delay(10);
    }
    
    String confirm = Serial.readStringUntil('\n');
    confirm.trim();
    confirm.toUpperCase();
    
    if (confirm != "S" && confirm != "Y") {
        Serial.println("   Escritura cancelada.");
        return;
    }
    
    Serial.println("   Escribiendo...");
    UHF_Status status = uhfReader.writeUserMemory(0x00, data, dataLen);
    
    if (status == UHF_OK) {
        Serial.println("   ✅ Datos escritos exitosamente!");
        Serial.println("   Verificando lectura...");
        
        delay(500);
        uint8_t readData[UHF_MAX_DATA_LENGTH];
        uint8_t readLen = 0;
        
        if (uhfReader.readUserMemory(0x00, dataLen / 2, readData, readLen) == UHF_OK) {
            Serial.print("   Datos verificados: ");
            for (int i = 0; i < readLen; i++) {
                if (readData[i] < 0x10) Serial.print("0");
                Serial.print(readData[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    } else {
        Serial.printf("   ❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Configurar Potencia
// ============================================================================

void doSetPower() {
    printSeparator();
    Serial.println("⚙️  Configurar Potencia de Transmisión");
    Serial.println();
    
    // Mostrar potencia actual
    uint16_t currentPower;
    if (uhfReader.getTransmitPower(currentPower) == UHF_OK) {
        Serial.printf("   Potencia actual: %d.%02d dBm\n", currentPower / 100, currentPower % 100);
    }
    
    Serial.println();
    Serial.println("   Opciones:");
    Serial.println("   [1] 18 dBm (bajo consumo, corto alcance)");
    Serial.println("   [2] 20 dBm");
    Serial.println("   [3] 22 dBm");
    Serial.println("   [4] 24 dBm");
    Serial.println("   [5] 26 dBm (máxima potencia, mayor alcance)");
    Serial.print("   > ");
    
    while (!Serial.available()) {
        delay(10);
    }
    
    char choice = Serial.read();
    Serial.println(choice);
    
    uint16_t power;
    switch (choice) {
        case '1': power = 1800; break;
        case '2': power = 2000; break;
        case '3': power = 2200; break;
        case '4': power = 2400; break;
        case '5': power = 2600; break;
        default:
            Serial.println("   ❌ Opción inválida");
            return;
    }
    
    UHF_Status status = uhfReader.setTransmitPower(power);
    if (status == UHF_OK) {
        Serial.printf("   ✅ Potencia configurada a %d.%02d dBm\n", power / 100, power % 100);
    } else {
        Serial.printf("   ❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Configurar Región de Frecuencia
// ============================================================================

void doSetRegion() {
    printSeparator();
    Serial.println("⚙️  Configurar Región de Frecuencia");
    Serial.println();
    Serial.println("   [1] China 920-925 MHz");
    Serial.println("   [2] US/FCC 902-928 MHz  ← México");
    Serial.println("   [3] EU/ETSI 865-868 MHz");
    Serial.println("   [4] China 840-845 MHz");
    Serial.println("   [5] Korea 917-924 MHz");
    Serial.print("   > ");
    
    while (!Serial.available()) {
        delay(10);
    }
    
    char choice = Serial.read();
    Serial.println(choice);
    
    uint8_t region;
    switch (choice) {
        case '1': region = FREQ_CHINA_900; break;
        case '2': region = FREQ_US; break;
        case '3': region = FREQ_EU; break;
        case '4': region = FREQ_CHINA_800; break;
        case '5': region = FREQ_KOREA; break;
        default:
            Serial.println("   ❌ Opción inválida");
            return;
    }
    
    UHF_Status status = uhfReader.setFrequencyRegion(region);
    if (status == UHF_OK) {
        Serial.println("   ✅ Región de frecuencia configurada");
    } else {
        Serial.printf("   ❌ %s\n", UHF_RFID::getStatusString(status).c_str());
    }
}

// ============================================================================
// Función: Información del Módulo
// ============================================================================

void doGetModuleInfo() {
    printSeparator();
    Serial.println("ℹ️  Información del Módulo UHF RFID");
    printSeparator();
    
    String hwVersion, swVersion;
    
    if (uhfReader.getHardwareVersion(hwVersion) == UHF_OK) {
        Serial.printf("   HW Version: %s\n", hwVersion.c_str());
    } else {
        Serial.println("   HW Version: No disponible");
    }
    
    if (uhfReader.getSoftwareVersion(swVersion) == UHF_OK) {
        Serial.printf("   SW Version: %s\n", swVersion.c_str());
    } else {
        Serial.println("   SW Version: No disponible");
    }
    
    uint16_t power;
    if (uhfReader.getTransmitPower(power) == UHF_OK) {
        Serial.printf("   Potencia:   %d.%02d dBm\n", power / 100, power % 100);
    }
    
    uint8_t region;
    if (uhfReader.getFrequencyRegion(region) == UHF_OK) {
        const char* regionNames[] = {"", "China 900", "US (FCC)", "EU (ETSI)", "China 800", "", "Korea"};
        if (region <= 6) {
            Serial.printf("   Región:     %s\n", regionNames[region]);
        } else {
            Serial.printf("   Región:     Código 0x%02X\n", region);
        }
    }
}

// ============================================================================
// Toggle Buzzer
// ============================================================================

bool buzzerEnabled = true;

void doToggleBuzzer() {
    buzzerEnabled = !buzzerEnabled;
    
    UHF_Status status = uhfReader.setBeep(buzzerEnabled);
    if (status == UHF_OK) {
        Serial.printf("🔔 Buzzer: %s\n", buzzerEnabled ? "ACTIVADO" : "DESACTIVADO");
    } else {
        Serial.printf("❌ %s\n", UHF_RFID::getStatusString(status).c_str());
        buzzerEnabled = !buzzerEnabled; // Revertir
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Inicializar serial de debug (USB)
    Serial.begin(115200);
    delay(2000); // Esperar a que se estabilice el USB CDC
    
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║                                                        ║");
    Serial.println("║   🔖  UHF RFID Reader - ESP32-C3                      ║");
    Serial.println("║   Protocolo: ISO 18000-6C (EPC Gen2)                   ║");
    Serial.println("║   Frecuencia: 902-928 MHz                              ║");
    Serial.println("║                                                        ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Activar modo debug para ver las tramas
    uhfReader.setDebug(true);
    
    // Inicializar módulo UHF RFID
    Serial.println("Inicializando módulo UHF RFID...");
    Serial.printf("  RX Pin: GPIO%d\n", UHF_RX_PIN);
    Serial.printf("  TX Pin: GPIO%d\n", UHF_TX_PIN);
    Serial.printf("  Baud Rate: %d\n", UHF_BAUD_RATE);
    Serial.println();
    
    UHF_Status status = uhfReader.begin(UHF_RX_PIN, UHF_TX_PIN, UHF_BAUD_RATE);
    
    if (status == UHF_OK) {
        Serial.println("✅ Módulo UHF RFID inicializado correctamente!");
        
        // Configurar región para México (FCC/US)
        Serial.println("Configurando región US/FCC (902-928 MHz) para México...");
        uhfReader.setFrequencyRegion(FREQ_US);
        
    } else {
        Serial.println("⚠️  No se pudo detectar el módulo UHF RFID.");
        Serial.println("   Verifica las conexiones y alimentación.");
        Serial.println("   El programa continuará intentando comunicarse.");
        Serial.println();
        Serial.println("   Posibles soluciones:");
        Serial.println("   1. Verificar que el módulo está alimentado a 12V");
        Serial.println("   2. Verificar TX/RX están correctamente cruzados");
        Serial.println("   3. ¿Se necesita convertidor MAX3232? (si RS232 real)");
        Serial.printf( "   4. Probar otra velocidad (actual: %d)\n", UHF_BAUD_RATE);
        Serial.println("   5. Verificar GND común entre ESP32 y módulo");
    }
    
    // Mostrar menú
    printMenu();
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Si está en modo lectura continua, procesar datos
    if (continuousMode) {
        uhfReader.process();
        
        // Verificar si el usuario quiere detener
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '3' || c == 's' || c == 'S' || c == 'q' || c == 'Q') {
                toggleContinuousRead();
                printMenu();
            }
        }
        return;
    }
    
    // Procesar comandos del usuario
    if (Serial.available()) {
        char cmd = Serial.read();
        
        // Consumir caracteres extra (newline, etc.)
        delay(50);
        while (Serial.available()) Serial.read();
        
        switch (cmd) {
            case '1':
                doSingleRead();
                break;
            case '2':
                doMultiRead();
                break;
            case '3':
                toggleContinuousRead();
                break;
            case '4':
                doReadEPC();
                break;
            case '5':
                doReadTID();
                break;
            case '6':
                doReadUserMemory();
                break;
            case '7':
                doWriteEPC();
                break;
            case '8':
                doWriteUserMemory();
                break;
            case '9':
                doSetPower();
                break;
            case 'A': case 'a':
                doSetRegion();
                break;
            case 'B': case 'b':
                doGetModuleInfo();
                break;
            case 'C': case 'c':
                doToggleBuzzer();
                break;
            case 'H': case 'h': case '?':
                printMenu();
                break;
            case '\n': case '\r':
                // Ignorar saltos de línea sueltos
                break;
            default:
                Serial.printf("❌ Opción '%c' no reconocida. Presiona 'H' para ver el menú.\n", cmd);
                break;
        }
        
        if (!continuousMode && cmd != '\n' && cmd != '\r') {
            Serial.println();
            Serial.print(">> Selecciona opción: ");
        }
    }
}
