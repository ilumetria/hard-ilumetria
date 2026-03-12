/**
 * @file RC522Module.h
 * @brief Módulo completo para sensor MFRC522 (RC522) con ESP32-S3 CAM
 * @description Proporciona funciones de alto nivel para lectura y escritura
 *              de tarjetas/tags RFID MIFARE Classic 1K y MIFARE Ultralight.
 * 
 * Conexiones SPI (ESP32-S3 CAM -> RC522):
 *   SDA  (SS)  -> GPIO10
 *   SCK        -> GPIO12
 *   MOSI       -> GPIO11
 *   MISO       -> GPIO13
 *   RST        -> GPIO9
 *   3.3V       -> 3.3V
 *   GND        -> GND
 * 
 * NOTA: Estos pines evitan conflictos con el módulo de cámara.
 *       Ajusta según tu modelo específico de ESP32-S3 CAM.
 * 
 * @author Adrian Nava
 * @date 2026-03-08
 */

#ifndef RC522_MODULE_H
#define RC522_MODULE_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// ============================================================================
// Configuración de pines SPI para ESP32-S3 CAM
// Ajusta estos valores según tu cableado
// ============================================================================
#ifndef RC522_SS_PIN
  #define RC522_SS_PIN    7   // SDA / SS
#endif

#ifndef RC522_RST_PIN
  #define RC522_RST_PIN   10    // RST
#endif

#ifndef RC522_SCK_PIN
  #define RC522_SCK_PIN   4   // SCK
#endif

#ifndef RC522_MOSI_PIN
  #define RC522_MOSI_PIN  6   // MOSI
#endif

#ifndef RC522_MISO_PIN
  #define RC522_MISO_PIN  5   // MISO
#endif

// ============================================================================
// Constantes del módulo
// ============================================================================
#define MIFARE_CLASSIC_BLOCK_SIZE  16   // Bytes por bloque en MIFARE Classic
#define MIFARE_CLASSIC_KEY_SIZE     6   // Bytes por clave
#define MIFARE_UL_PAGE_SIZE         4   // Bytes por página en Ultralight

// Clave por defecto de fábrica MIFARE Classic
static const byte DEFAULT_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
// Enumeración de tipos de tarjeta
// ============================================================================
enum CardType {
    CARD_UNKNOWN = 0,
    CARD_MIFARE_MINI,        // 320 bytes
    CARD_MIFARE_1K,          // 1024 bytes (más común)
    CARD_MIFARE_4K,          // 4096 bytes
    CARD_MIFARE_ULTRALIGHT,  // 64 bytes (NFC tags)
    CARD_MIFARE_PLUS,
    CARD_NTAG_213,           // NFC Forum Type 2
    CARD_NTAG_215,
    CARD_NTAG_216
};

// ============================================================================
// Estructura para resultados de operaciones
// ============================================================================
struct RC522Result {
    bool success;
    String message;
    byte data[MIFARE_CLASSIC_BLOCK_SIZE];
    byte dataLength;
    
    RC522Result() : success(false), message(""), dataLength(0) {
        memset(data, 0, sizeof(data));
    }
};

// ============================================================================
// Estructura para información de tarjeta
// ============================================================================
struct CardInfo {
    bool detected;
    CardType type;
    String typeName;
    byte uid[10];          // UID puede ser 4, 7 o 10 bytes
    byte uidLength;
    String uidString;      // UID en formato hexadecimal legible
    byte sak;              // Select Acknowledge
    
    CardInfo() : detected(false), type(CARD_UNKNOWN), typeName("Desconocida"),
                 uidLength(0), uidString(""), sak(0) {
        memset(uid, 0, sizeof(uid));
    }
};

// ============================================================================
// Clase principal RC522Module
// ============================================================================
class RC522Module {
public:
    /**
     * @brief Constructor
     * @param ssPin Pin SDA/SS (default: RC522_SS_PIN)
     * @param rstPin Pin RST (default: RC522_RST_PIN)
     */
    RC522Module(uint8_t ssPin = RC522_SS_PIN, uint8_t rstPin = RC522_RST_PIN);
    
    /**
     * @brief Destructor
     */
    ~RC522Module();

    // ========================================================================
    // Inicialización y estado
    // ========================================================================
    
    /**
     * @brief Inicializa el módulo RC522 con los pines SPI personalizados
     * @return true si la inicialización fue exitosa
     */
    bool begin();
    
    /**
     * @brief Verifica si el módulo RC522 está conectado y funcionando
     * @return true si el módulo responde correctamente
     */
    bool isConnected();
    
    /**
     * @brief Obtiene la versión de firmware del RC522
     * @return String con la versión del firmware
     */
    String getFirmwareVersion();
    
    /**
     * @brief Configura la ganancia de la antena
     * @param gain Valor de ganancia (MFRC522::RxGain)
     */
    void setAntennaGain(MFRC522::PCD_RxGain gain);
    
    /**
     * @brief Realiza un auto-test del módulo
     * @return true si el auto-test pasó correctamente
     */
    bool selfTest();

    // ========================================================================
    // Detección de tarjetas
    // ========================================================================
    
    /**
     * @brief Detecta si hay una nueva tarjeta presente
     * @return true si se detectó una nueva tarjeta
     */
    bool isNewCardPresent();
    
    /**
     * @brief Lee la información completa de la tarjeta detectada
     * @return CardInfo con la información de la tarjeta
     */
    CardInfo readCardInfo();
    
    /**
     * @brief Espera hasta que se presente una tarjeta (bloqueante)
     * @param timeoutMs Tiempo máximo de espera en ms (0 = sin timeout)
     * @return CardInfo con la información de la tarjeta detectada
     */
    CardInfo waitForCard(unsigned long timeoutMs = 0);
    
    /**
     * @brief Identifica el tipo de tarjeta basándose en SAK y ATQA
     * @return CardType enum del tipo de tarjeta
     */
    CardType identifyCardType();

    // ========================================================================
    // Lectura - MIFARE Classic
    // ========================================================================
    
    /**
     * @brief Lee un bloque completo de una tarjeta MIFARE Classic
     * @param blockAddr Dirección del bloque a leer (0-63 para 1K)
     * @param key Clave de autenticación (default: FFFFFFFFFFFFh)
     * @param keyType Tipo de clave: KEYA o KEYB
     * @return RC522Result con los datos leídos
     */
    RC522Result readBlock(byte blockAddr, 
                          const byte* key = DEFAULT_KEY, 
                          MFRC522::MIFARE_Key* mifareKey = nullptr);
    
    /**
     * @brief Lee un sector completo (4 bloques) de MIFARE Classic
     * @param sectorNum Número de sector (0-15 para 1K)
     * @param key Clave de autenticación
     * @return Vector de RC522Result (4 bloques)
     */
    bool readSector(byte sectorNum, byte sectorData[][MIFARE_CLASSIC_BLOCK_SIZE], 
                    const byte* key = DEFAULT_KEY);
    
    /**
     * @brief Lee texto almacenado en uno o más bloques consecutivos
     * @param startBlock Bloque inicial
     * @param numBlocks Número de bloques a leer
     * @param key Clave de autenticación
     * @return String con el texto leído
     */
    String readText(byte startBlock, byte numBlocks = 1, 
                    const byte* key = DEFAULT_KEY);

    // ========================================================================
    // Escritura - MIFARE Classic
    // ========================================================================
    
    /**
     * @brief Escribe datos en un bloque de MIFARE Classic
     * @param blockAddr Dirección del bloque (evitar bloques trailer: 3,7,11...)
     * @param data Datos a escribir (16 bytes)
     * @param dataLen Longitud de los datos
     * @param key Clave de autenticación
     * @return RC522Result con el resultado de la operación
     */
    RC522Result writeBlock(byte blockAddr, const byte* data, byte dataLen,
                           const byte* key = DEFAULT_KEY);
    
    /**
     * @brief Escribe un string de texto en uno o más bloques
     * @param startBlock Bloque inicial para escritura
     * @param text Texto a escribir
     * @param key Clave de autenticación
     * @return RC522Result con el resultado
     */
    RC522Result writeText(byte startBlock, const String& text, 
                          const byte* key = DEFAULT_KEY);

    // ========================================================================
    // Lectura/Escritura - MIFARE Ultralight / NTAG
    // ========================================================================
    
    /**
     * @brief Lee una página (4 bytes) de MIFARE Ultralight
     * @param pageAddr Dirección de la página (0-15)
     * @return RC522Result con los datos
     */
    RC522Result readUltralightPage(byte pageAddr);
    
    /**
     * @brief Escribe una página (4 bytes) en MIFARE Ultralight
     * @param pageAddr Dirección de la página (4-15 para datos de usuario)
     * @param data Datos a escribir (4 bytes)
     * @return RC522Result con el resultado
     */
    RC522Result writeUltralightPage(byte pageAddr, const byte* data);

    // ========================================================================
    // Utilidades
    // ========================================================================
    
    /**
     * @brief Convierte un arreglo de bytes a string hexadecimal
     * @param buffer Arreglo de bytes
     * @param bufferSize Tamaño del arreglo
     * @return String en formato "XX:XX:XX:XX"
     */
    static String bytesToHexString(const byte* buffer, byte bufferSize);
    
    /**
     * @brief Imprime el contenido de un bloque en formato legible
     * @param blockAddr Dirección del bloque
     * @param data Datos del bloque
     * @param dataLen Longitud de los datos
     */
    static void printBlockData(byte blockAddr, const byte* data, byte dataLen);
    
    /**
     * @brief Realiza un dump completo de la tarjeta por Serial
     */
    void dumpCardToSerial();
    
    /**
     * @brief Verifica si un bloque es un Sector Trailer (contiene claves)
     * @param blockAddr Dirección del bloque
     * @return true si es un sector trailer
     */
    static bool isSectorTrailer(byte blockAddr);
    
    /**
     * @brief Obtiene el primer bloque de datos usable en un sector
     * @param sectorNum Número de sector
     * @return Número del primer bloque de datos
     */
    static byte getFirstDataBlock(byte sectorNum);
    
    /**
     * @brief Detiene la comunicación con la tarjeta actual
     */
    void haltCard();
    
    /**
     * @brief Obtiene acceso directo al objeto MFRC522 para operaciones avanzadas
     * @return Referencia al objeto MFRC522
     */
    MFRC522& getRawReader();

private:
    MFRC522* _mfrc522;
    SPIClass* _spi;
    uint8_t _ssPin;
    uint8_t _rstPin;
    bool _initialized;
    
    /**
     * @brief Autentica un bloque con la clave proporcionada
     * @param blockAddr Dirección del bloque
     * @param key Clave de autenticación (6 bytes)
     * @return true si la autenticación fue exitosa
     */
    bool authenticateBlock(byte blockAddr, const byte* key);
    
    /**
     * @brief Convierte byte array de clave a MFRC522::MIFARE_Key
     */
    MFRC522::MIFARE_Key toMifareKey(const byte* key);
    
    /**
     * @brief Determina el nombre del tipo de tarjeta
     */
    String getCardTypeName(MFRC522::PICC_Type piccType);
};

#endif // RC522_MODULE_H
