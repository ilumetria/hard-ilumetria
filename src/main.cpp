/**
 * ============================================================
 *  main.cpp
 *  Script de prueba — RC522_RFID Library
 *  Entorno: PlatformIO + Arduino Framework — ESP32-S3
 *
 *  CONEXIONES:
 *  ┌──────────┬──────────────┬────────────────────────────────┐
 *  │  RC522   │   ESP32-S3   │           Nota                 │
 *  ├──────────┼──────────────┼────────────────────────────────┤
 *  │  SDA/SS  │   GPIO 5     │  Configurable con SS_PIN       │
 *  │  SCK     │   GPIO 36    │  SPI CLK por defecto ESP32-S3  │
 *  │  MOSI    │   GPIO 35    │  SPI MOSI por defecto ESP32-S3 │
 *  │  MISO    │   GPIO 37    │  SPI MISO por defecto ESP32-S3 │
 *  │  RST     │   GPIO 4     │  Configurable con RST_PIN      │
 *  │  GND     │   GND        │                                │
 *  │  3.3V    │   3.3V       │  No usar 5V                    │
 *  └──────────┴──────────────┴────────────────────────────────┘
 *
 *  ESTRUCTURA DEL PROYECTO PlatformIO:
 *  ├── src/
 *  │   ├── main.cpp
 *  │   ├── RC522_RFID.h
 *  │   └── RC522_RFID.cpp
 *  └── platformio.ini
 *
 *  platformio.ini mínimo:
 *  [env:esp32-s3-devkitc-1]
 *  platform  = espressif32
 *  board     = esp32-s3-devkitc-1
 *  framework = arduino
 *  monitor_speed = 115200
 * ============================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include "RC522_RFID.h"

// ── Pines ────────────────────────────────────────────────────
#define SS_PIN   5
#define RST_PIN  4

// ── Bloque de datos para el User ID ─────────────────────────
// Bloques seguros MIFARE 1K: 1, 2, 5, 6, 9, 10...
// Evitar bloque 0 (UID de fábrica) y trailers (3, 7, 11, 15...)
#define USER_ID_BLOCK  1

// ── Instancia de la librería ─────────────────────────────────
RC522_RFID rfid(SS_PIN, RST_PIN, SPI);

// ── Clave de acceso (por defecto FFFFFFFFFFFF) ───────────────
MifareKey key = RC522_RFID::DEFAULT_KEY_A;

// ── Estado del menú ──────────────────────────────────────────
enum AppState {
    STATE_MENU,
    STATE_WAITING_CARD,
    STATE_WAITING_USER_ID_INPUT
};

AppState    appState     = STATE_MENU;
char        pendingAction = 0;
String      pendingUserId = "";

// ── Prototipos ───────────────────────────────────────────────
void printMenu();
void handleMenuInput(char option);
void processCard();

void doReadUID();
void doWriteUserId();
void doReadUserId();
void doClearBlock();
void doDumpSector();
void doHardwareTest();

bool        authBlock(uint8_t blockAddr);
void        printStatus(StatusCode s);
void        printUid(const Uid& u);
void        printBlock(uint8_t* buf, uint8_t len);
void        printSeparator();


// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("============================================");
    Serial.println("  RC522_RFID — PlatformIO + Arduino");
    Serial.println("  ESP32-S3");
    Serial.println("============================================");

    // Inicializar SPI con los pines correctos del ESP32-S3
    // SCK=36, MISO=37, MOSI=35, SS=5
    SPI.begin(36, 37, 35, SS_PIN);

    if (!rfid.begin()) {
        Serial.println();
        Serial.println("[ERROR] No se pudo inicializar el RC522.");
        Serial.println("        Revisa el cableado y que alimentes con 3.3V.");
        Serial.printf ("        Version leida: 0x%02X\n", rfid.getVersion());
        while (true) delay(1000);
    }

    Serial.printf("\n[OK] RC522 listo. Chip version: 0x%02X\n", rfid.getVersion());
    printMenu();
}


// ============================================================
//  loop()
// ============================================================
void loop() {

    // ── Leer entrada del Serial ──────────────────────────────
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.isEmpty()) return;

        if (appState == STATE_WAITING_USER_ID_INPUT) {
            // El usuario ingresó el User ID a escribir
            pendingUserId = input;
            if (pendingUserId.length() > 16) {
                pendingUserId = pendingUserId.substring(0, 16);
                Serial.println("[WARN] ID truncado a 16 caracteres: " + pendingUserId);
            }
            appState = STATE_WAITING_CARD;
            Serial.println("[>>] Ahora acerca la tarjeta al lector...");
            return;
        }

        if (appState == STATE_MENU) {
            handleMenuInput(input[0]);
        }
    }

    // ── Detectar tarjeta ────────────────────────────────────
    if (appState == STATE_WAITING_CARD) {
        if (!rfid.isNewCardPresent()) return;
        if (!rfid.readCardSerial())   return;

        Serial.println();
        printSeparator();
        Serial.print("[TARJETA] UID : ");
        printUid(rfid.uid);
        Serial.println();
        Serial.printf  ("[TARJETA] Tipo: %s\n",
            rfid.getPiccTypeName(rfid.getPiccType(rfid.uid.sak)));
        printSeparator();

        processCard();

        rfid.haltA();
        rfid.stopCrypto1();
        appState = STATE_MENU;

        delay(1500);
        printMenu();
    }
}


// ============================================================
//  Menú
// ============================================================
void printMenu() {
    Serial.println();
    printSeparator();
    Serial.println("  MENU — Elige una opcion:");
    printSeparator();
    Serial.println("  [1] Leer UID de tarjeta");
    Serial.println("  [2] Escribir User ID en tarjeta");
    Serial.println("  [3] Leer User ID de tarjeta");
    Serial.println("  [4] Borrar bloque de datos");
    Serial.println("  [5] Dump sector 0");
    Serial.println("  [6] Test de hardware");
    printSeparator();
    Serial.print("> ");
}

void handleMenuInput(char option) {
    Serial.println(option);
    pendingAction = option;

    switch (option) {
        case '1':
        case '3':
        case '4':
        case '5':
            appState = STATE_WAITING_CARD;
            Serial.println("[>>] Acerca la tarjeta al lector...");
            break;

        case '2':
            Serial.print("[>>] Ingresa el User ID (max 16 chars): ");
            appState = STATE_WAITING_USER_ID_INPUT;
            break;

        case '6':
            doHardwareTest();
            printMenu();
            break;

        default:
            Serial.println("[WARN] Opcion no valida.");
            printMenu();
            break;
    }
}

void processCard() {
    switch (pendingAction) {
        case '1': doReadUID();    break;
        case '2': doWriteUserId(); break;
        case '3': doReadUserId(); break;
        case '4': doClearBlock(); break;
        case '5': doDumpSector(); break;
    }
}


// ============================================================
//  [1] Leer UID
// ============================================================
void doReadUID() {
    Serial.println("\n[1] LECTURA DE UID");

    Serial.println("  UID (legible)  : " + rfid.uid.toHexString());
    Serial.println("  UID (compacto) : " + rfid.uid.toHexStringCompact());
    Serial.printf ("  Tamano UID     : %d bytes\n", rfid.uid.size);

    Serial.println();
    Serial.println("  -- Query SQL de ejemplo:");
    Serial.println("  SELECT * FROM usuarios");
    Serial.println("    WHERE rfid_uid = '" + rfid.uid.toHexStringCompact() + "';");
    Serial.println();
    Serial.println("  -- JSON de ejemplo:");
    Serial.println("  {\"rfid_uid\": \"" + rfid.uid.toHexStringCompact() + "\"}");
}


// ============================================================
//  [2] Escribir User ID
// ============================================================
void doWriteUserId() {
    Serial.println("\n[2] ESCRITURA DE USER ID");
    Serial.println("  User ID        : " + pendingUserId);
    Serial.printf ("  Bloque destino : %d\n", USER_ID_BLOCK);

    if (!authBlock(USER_ID_BLOCK)) return;

    StatusCode status = rfid.writeUserId(USER_ID_BLOCK, pendingUserId);

    if (status == STATUS_OK) {
        Serial.println("  [OK] User ID escrito correctamente.");

        // Verificación automática
        String verificado;
        StatusCode stVerify = rfid.readUserId(USER_ID_BLOCK, verificado);
        if (stVerify == STATUS_OK) {
            Serial.print("  [OK] Verificacion: '" + verificado + "' ");
            Serial.println(verificado == pendingUserId ? "-> COINCIDE" : "-> [WARN] DISCREPANCIA");
        } else {
            Serial.print("  [WARN] No se pudo verificar: ");
            printStatus(stVerify);
        }
    } else {
        Serial.print("  [ERROR] No se pudo escribir: ");
        printStatus(status);
    }
}


// ============================================================
//  [3] Leer User ID
// ============================================================
void doReadUserId() {
    Serial.println("\n[3] LECTURA DE USER ID");
    Serial.printf ("  Bloque fuente  : %d\n", USER_ID_BLOCK);

    if (!authBlock(USER_ID_BLOCK)) return;

    String userId;
    StatusCode status = rfid.readUserId(USER_ID_BLOCK, userId);

    if (status == STATUS_OK) {
        String uid = rfid.uid.toHexStringCompact();
        Serial.println("  User ID leido  : '" + userId + "'");
        Serial.println();
        Serial.println("  -- Queries SQL de ejemplo:");
        Serial.println("  SELECT * FROM usuarios WHERE user_id = '" + userId + "';");
        Serial.println("  SELECT * FROM usuarios WHERE rfid_uid = '" + uid + "';");
        Serial.println();
        Serial.println("  -- Registro de acceso:");
        Serial.println("  INSERT INTO accesos (rfid_uid, user_id, timestamp)");
        Serial.println("    VALUES ('" + uid + "', '" + userId + "', NOW());");
        Serial.println();
        Serial.println("  -- JSON:");
        Serial.println("  {\"rfid_uid\":\"" + uid + "\",\"user_id\":\"" + userId + "\"}");
    } else {
        Serial.print("  [ERROR] No se pudo leer: ");
        printStatus(status);
    }
}


// ============================================================
//  [4] Borrar bloque
// ============================================================
void doClearBlock() {
    Serial.println("\n[4] BORRAR BLOQUE");
    Serial.printf ("  Bloque         : %d\n", USER_ID_BLOCK);

    if (!authBlock(USER_ID_BLOCK)) return;

    uint8_t emptyBlock[16] = {0x00};
    StatusCode status = rfid.mifareWrite(USER_ID_BLOCK, emptyBlock, 16);

    if (status == STATUS_OK) {
        Serial.println("  [OK] Bloque borrado correctamente.");
    } else {
        Serial.print("  [ERROR] ");
        printStatus(status);
    }
}


// ============================================================
//  [5] Dump sector 0
// ============================================================
void doDumpSector() {
    Serial.println("\n[5] DUMP SECTOR 0 (bloques 0-3)");

    StatusCode status = rfid.authenticate(
        PICC_CMD_MF_AUTH_KEY_A, 3, &key, &rfid.uid);

    if (status != STATUS_OK) {
        Serial.print("  [ERROR] Autenticacion fallida: ");
        printStatus(status);
        return;
    }

    Serial.println("  Bloque  Hex                                       ASCII");
    Serial.println("  " + String('-', 68));

    uint8_t buffer[18];
    for (uint8_t block = 0; block < 4; block++) {
        uint8_t size = sizeof(buffer);
        status = rfid.mifareRead(block, buffer, &size);

        Serial.printf("  %2d      ", block);
        if (status == STATUS_OK) {
            printBlock(buffer, 16);
            if (block == 0) Serial.print(" <- UID fab.");
            if (block == 3) Serial.print(" <- trailer");
        } else {
            Serial.print("[ERROR] ");
            printStatus(status);
        }
        Serial.println();
    }
}


// ============================================================
//  [6] Test de hardware
// ============================================================
void doHardwareTest() {
    Serial.println("\n[6] TEST DE HARDWARE");
    printSeparator();

    uint8_t ver = rfid.getVersion();
    Serial.printf("  Version chip   : 0x%02X", ver);
    if      (ver == 0x91) Serial.println(" -> RC522 v1.0  [OK]");
    else if (ver == 0x92) Serial.println(" -> RC522 v2.0  [OK]");
    else if (ver == 0x88) Serial.println(" -> Clone RC522 [OK - compatible]");
    else                  Serial.println(" -> [WARN] Version desconocida");

    Serial.println();
    Serial.println("  Pines SPI configurados:");
    Serial.printf ("    SS   : GPIO %d\n", SS_PIN);
    Serial.printf ("    RST  : GPIO %d\n", RST_PIN);
    Serial.println("    SCK  : GPIO 36");
    Serial.println("    MISO : GPIO 37");
    Serial.println("    MOSI : GPIO 35");
    Serial.println();
    Serial.printf ("  Bloque User ID : %d\n", USER_ID_BLOCK);
}


// ============================================================
//  Helpers
// ============================================================
bool authBlock(uint8_t blockAddr) {
    uint8_t trailer = (blockAddr / 4) * 4 + 3;
    StatusCode status = rfid.authenticate(
        PICC_CMD_MF_AUTH_KEY_A, trailer, &key, &rfid.uid);
    if (status != STATUS_OK) {
        Serial.printf("  [ERROR] Autenticacion fallida (trailer bloque %d): ", trailer);
        printStatus(status);
        return false;
    }
    return true;
}

void printStatus(StatusCode s) {
    switch (s) {
        case STATUS_OK:             Serial.println("OK");                           break;
        case STATUS_ERROR:          Serial.println("Error generico");               break;
        case STATUS_COLLISION:      Serial.println("Colision detectada");           break;
        case STATUS_TIMEOUT:        Serial.println("Timeout - tarjeta fuera de rango"); break;
        case STATUS_NO_ROOM:        Serial.println("Buffer insuficiente");          break;
        case STATUS_INTERNAL_ERROR: Serial.println("Error interno");                break;
        case STATUS_INVALID:        Serial.println("Argumento invalido");           break;
        case STATUS_CRC_WRONG:      Serial.println("CRC incorrecto");               break;
        case STATUS_MIFARE_NACK:    Serial.println("MIFARE NAK - clave incorrecta"); break;
        default:                    Serial.println("Desconocido");                  break;
    }
}

void printUid(const Uid& u) {
    for (uint8_t i = 0; i < u.size; i++) {
        if (u.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(u.uidByte[i], HEX);
        if (i < u.size - 1) Serial.print(":");
    }
}

void printBlock(uint8_t* buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.print(" ");
    for (uint8_t i = 0; i < len; i++) {
        Serial.print((buf[i] >= 32 && buf[i] < 127) ? (char)buf[i] : '.');
    }
}

void printSeparator() {
    Serial.println("--------------------------------------------");
}
