#ifndef RC522_RFID_H
#define RC522_RFID_H

#include <Arduino.h>
#include <SPI.h>

// ============================================================
//  Registros del RC522
// ============================================================
#define RC522_REG_COMMAND        0x01
#define RC522_REG_COM_IEN        0x02
#define RC522_REG_DIV_IEN        0x03
#define RC522_REG_COM_IRQ        0x04
#define RC522_REG_DIV_IRQ        0x05
#define RC522_REG_ERROR          0x06
#define RC522_REG_STATUS1        0x07
#define RC522_REG_STATUS2        0x08
#define RC522_REG_FIFO_DATA      0x09
#define RC522_REG_FIFO_LEVEL     0x0A
#define RC522_REG_WATER_LEVEL    0x0B
#define RC522_REG_CONTROL        0x0C
#define RC522_REG_BIT_FRAMING    0x0D
#define RC522_REG_COLL           0x0E
#define RC522_REG_MODE           0x11
#define RC522_REG_TX_MODE        0x12
#define RC522_REG_RX_MODE        0x13
#define RC522_REG_TX_CONTROL     0x14
#define RC522_REG_TX_ASK         0x15
#define RC522_REG_TX_SEL         0x16
#define RC522_REG_RX_SEL         0x17
#define RC522_REG_RX_THRESHOLD   0x18
#define RC522_REG_DEMOD          0x19
#define RC522_REG_MF_TX          0x1C
#define RC522_REG_MF_RX          0x1D
#define RC522_REG_SERIAL_SPEED   0x1F
#define RC522_REG_CRC_RESULT_H   0x21
#define RC522_REG_CRC_RESULT_L   0x22
#define RC522_REG_MOD_WIDTH      0x24
#define RC522_REG_RF_CFG         0x26
#define RC522_REG_GS_N           0x27
#define RC522_REG_CW_GS_P        0x28
#define RC522_REG_MOD_GS_P       0x29
#define RC522_REG_T_MODE         0x2A
#define RC522_REG_T_PRESCALER    0x2B
#define RC522_REG_T_RELOAD_H     0x2C
#define RC522_REG_T_RELOAD_L     0x2D
#define RC522_REG_T_COUNTER_H    0x2E
#define RC522_REG_T_COUNTER_L    0x2F
#define RC522_REG_TEST_SEL1      0x31
#define RC522_REG_TEST_SEL2      0x32
#define RC522_REG_TEST_PIN_EN    0x33
#define RC522_REG_TEST_PIN_VAL   0x34
#define RC522_REG_TEST_BUS       0x35
#define RC522_REG_AUTO_TEST      0x36
#define RC522_REG_VERSION        0x37

// ============================================================
//  Comandos RC522
// ============================================================
#define RC522_CMD_IDLE           0x00
#define RC522_CMD_MEM            0x01
#define RC522_CMD_GEN_RANDOM_ID  0x02
#define RC522_CMD_CALC_CRC       0x03
#define RC522_CMD_TRANSMIT       0x04
#define RC522_CMD_NO_CMD_CHANGE  0x07
#define RC522_CMD_RECEIVE        0x08
#define RC522_CMD_TRANSCEIVE     0x0C
#define RC522_CMD_MF_AUTHENT     0x0E
#define RC522_CMD_SOFT_RESET     0x0F

// ============================================================
//  Comandos PICC (ISO 14443A)
// ============================================================
#define PICC_CMD_REQA            0x26
#define PICC_CMD_WUPA            0x52
#define PICC_CMD_CT              0x88
#define PICC_CMD_SEL_CL1         0x93
#define PICC_CMD_SEL_CL2         0x95
#define PICC_CMD_SEL_CL3         0x97
#define PICC_CMD_HLTA            0x50
#define PICC_CMD_MF_AUTH_KEY_A   0x60
#define PICC_CMD_MF_AUTH_KEY_B   0x61
#define PICC_CMD_MF_READ         0x30
#define PICC_CMD_MF_WRITE        0xA0
#define PICC_CMD_MF_DECREMENT    0xC0
#define PICC_CMD_MF_INCREMENT    0xC1
#define PICC_CMD_MF_RESTORE      0xC2
#define PICC_CMD_MF_TRANSFER     0xB0
#define PICC_CMD_UL_WRITE        0xA2

// ============================================================
//  Tipos de tarjeta PICC
// ============================================================
enum PICC_Type {
    PICC_TYPE_UNKNOWN,
    PICC_TYPE_ISO_14443_4,
    PICC_TYPE_ISO_18092,
    PICC_TYPE_MIFARE_MINI,
    PICC_TYPE_MIFARE_1K,
    PICC_TYPE_MIFARE_4K,
    PICC_TYPE_MIFARE_UL,
    PICC_TYPE_MIFARE_PLUS,
    PICC_TYPE_TNP3XXX,
    PICC_TYPE_NOT_COMPLETE
};

// ============================================================
//  Códigos de estado
// ============================================================
enum StatusCode {
    STATUS_OK,
    STATUS_ERROR,
    STATUS_COLLISION,
    STATUS_TIMEOUT,
    STATUS_NO_ROOM,
    STATUS_INTERNAL_ERROR,
    STATUS_INVALID,
    STATUS_CRC_WRONG,
    STATUS_MIFARE_NACK
};

// ============================================================
//  Estructura UID
// ============================================================
struct Uid {
    uint8_t size;           // Número de bytes en el UID
    uint8_t uidByte[10];    // UID bytes
    uint8_t sak;            // SAK (Select Acknowledge)

    // Retorna el UID como String hexadecimal
    String toHexString() const {
        String result = "";
        for (uint8_t i = 0; i < size; i++) {
            if (uidByte[i] < 0x10) result += "0";
            result += String(uidByte[i], HEX);
            if (i < size - 1) result += ":";
        }
        result.toUpperCase();
        return result;
    }

    // Retorna el UID como String sin separadores (ideal para DB lookup)
    String toHexStringCompact() const {
        String result = "";
        for (uint8_t i = 0; i < size; i++) {
            if (uidByte[i] < 0x10) result += "0";
            result += String(uidByte[i], HEX);
        }
        result.toUpperCase();
        return result;
    }

    // Compara dos UIDs
    bool equals(const Uid& other) const {
        if (size != other.size) return false;
        for (uint8_t i = 0; i < size; i++) {
            if (uidByte[i] != other.uidByte[i]) return false;
        }
        return true;
    }
};

// ============================================================
//  Estructura de clave MIFARE
// ============================================================
struct MifareKey {
    uint8_t keyByte[6];
};

// ============================================================
//  Clase principal RC522_RFID
// ============================================================
class RC522_RFID {
public:
    // ---- Constructor / Destructor ----
    RC522_RFID(uint8_t ssPin, uint8_t resetPin, SPIClass& spiInstance = SPI);
    ~RC522_RFID() = default;

    // ---- Inicialización ----
    bool begin(uint32_t spiFreq = 10000000);
    void reset();
    void antennaOn();
    void antennaOff();

    // ---- Detección de tarjeta ----
    bool isNewCardPresent();
    bool readCardSerial();

    // ---- Información de tarjeta ----
    Uid         uid;
    PICC_Type   getPiccType(uint8_t sak);
    const char* getPiccTypeName(PICC_Type type);
    uint8_t     getVersion();
    bool        selfTest();

    // ---- Autenticación MIFARE Classic ----
    StatusCode authenticate(uint8_t command, uint8_t blockAddr,
                            MifareKey* key, Uid* uid);
    void stopCrypto1();

    // ---- Lectura / Escritura MIFARE Classic ----
    StatusCode mifareRead(uint8_t blockAddr, uint8_t* buffer, uint8_t* bufferSize);
    StatusCode mifareWrite(uint8_t blockAddr, uint8_t* buffer, uint8_t bufferSize);

    // ---- Utilidades para ID de usuario ----
    /**
     * Escribe un userId (string) en un bloque MIFARE Classic.
     * El bloque debe estar previamente autenticado.
     * @param blockAddr  Dirección del bloque (recomendado: 1, 2, 5, 6...)
     * @param userId     String de hasta 16 caracteres
     */
    StatusCode writeUserId(uint8_t blockAddr, const String& userId);

    /**
     * Lee un userId (string) de un bloque MIFARE Classic.
     * El bloque debe estar previamente autenticado.
     * @param blockAddr  Dirección del bloque
     * @param userId     String donde se almacenará el ID leído
     */
    StatusCode readUserId(uint8_t blockAddr, String& userId);

    // ---- Halt ----
    StatusCode haltA();

    // ---- Claves por defecto ----
    static const MifareKey DEFAULT_KEY_A;
    static const MifareKey DEFAULT_KEY_B;

private:
    uint8_t   _ssPin;
    uint8_t   _resetPin;
    SPIClass& _spi;

    // Registro SPI
    void        writeRegister(uint8_t reg, uint8_t value);
    void        writeRegister(uint8_t reg, uint8_t count, uint8_t* values);
    uint8_t     readRegister(uint8_t reg);
    void        readRegister(uint8_t reg, uint8_t count, uint8_t* values, uint8_t rxAlign = 0);
    void        setRegisterBitMask(uint8_t reg, uint8_t mask);
    void        clearRegisterBitMask(uint8_t reg, uint8_t mask);

    // CRC
    StatusCode  calculateCRC(uint8_t* data, uint8_t length, uint8_t* result);

    // Comunicación con la tarjeta
    StatusCode  communicateWithPICC(uint8_t command, uint8_t waitIRq,
                                    uint8_t* sendData, uint8_t sendLen,
                                    uint8_t* backData = nullptr,
                                    uint8_t* backLen  = nullptr,
                                    uint8_t* validBits = nullptr,
                                    uint8_t rxAlign   = 0,
                                    bool checkCRC     = false);

    StatusCode  transceiveData(uint8_t* sendData, uint8_t sendLen,
                               uint8_t* backData, uint8_t* backLen,
                               uint8_t* validBits = nullptr,
                               uint8_t rxAlign    = 0,
                               bool checkCRC      = false);

    // PICC
    StatusCode  requestA(uint8_t* bufferATQA, uint8_t* bufferSize);
    StatusCode  wakeupA(uint8_t* bufferATQA, uint8_t* bufferSize);
    StatusCode  reqa_or_wupa(uint8_t command, uint8_t* bufferATQA, uint8_t* bufferSize);
    StatusCode  select(Uid* uid, uint8_t validBits = 0);
};

#endif // RC522_RFID_H
