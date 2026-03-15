/**
 * ============================================================================
 * UHF RFID Reader Module - ESP32-C3
 * ============================================================================
 * 
 * Driver para lector de etiquetas RFID UHF pasivo (modelo R16 / serie similar)
 * Protocolo: ISO 18000-6C (EPC Class 1 Gen 2)
 * Frecuencia: 902-928 MHz
 * Comunicación: UART Serial (9600-115200 bps)
 * Frame: Header 0xBB ... Checksum ... End 0x7E
 * 
 * Funcionalidades:
 *   - Lectura individual y múltiple de etiquetas (inventory)
 *   - Lectura de bancos de memoria (EPC, TID, User, Reserved)
 *   - Escritura en bancos de memoria (EPC, User)
 *   - Configuración de potencia de salida
 *   - Configuración de región de frecuencia
 *   - Consulta de versión de hardware/firmware
 * 
 * Autor: Módulo generado para proyecto ESP32-C3 con PlatformIO
 * ============================================================================
 */

#ifndef UHF_RFID_H
#define UHF_RFID_H

#include <Arduino.h>
#include <HardwareSerial.h>

// ============================================================================
// Constantes del Protocolo
// ============================================================================

// Frame delimiters
#define UHF_HEADER              0xBB
#define UHF_END                 0x7E

// Frame types
#define UHF_TYPE_COMMAND        0x00    // Host -> Reader
#define UHF_TYPE_RESPONSE       0x01    // Reader -> Host (respuesta)
#define UHF_TYPE_NOTICE         0x02    // Reader -> Host (notificación)

// ============================================================================
// Códigos de Comando
// ============================================================================

// Comandos de información del módulo
#define CMD_GET_HW_VERSION      0x03    // Obtener versión de hardware
#define CMD_GET_SW_VERSION      0x00    // Obtener versión de software/firmware
#define CMD_GET_MANUFACTURER    0x04    // Obtener fabricante

// Comandos de configuración
#define CMD_SET_POWER           0xB6    // Configurar potencia de transmisión
#define CMD_GET_POWER           0xB7    // Obtener potencia de transmisión
#define CMD_SET_FREQUENCY       0xAB    // Configurar región de frecuencia
#define CMD_GET_FREQUENCY       0xAA    // Obtener región de frecuencia
#define CMD_SET_BAUDRATE        0x11    // Configurar velocidad serial
#define CMD_SET_BEEP            0xB0    // Habilitar/deshabilitar beep

// Comandos de operación con etiquetas
#define CMD_SINGLE_POLL         0x22    // Lectura individual (single inventory)
#define CMD_MULTI_POLL          0x27    // Lectura múltiple (multi inventory)
#define CMD_STOP_POLL           0x28    // Detener lectura múltiple
#define CMD_READ_DATA           0x39    // Leer datos de memoria de etiqueta
#define CMD_WRITE_DATA          0x49    // Escribir datos en memoria de etiqueta
#define CMD_WRITE_EPC           0x33    // Escribir EPC directamente
#define CMD_LOCK_TAG            0x82    // Bloquear/proteger etiqueta
#define CMD_KILL_TAG            0x65    // Desactivar etiqueta permanentemente
#define CMD_SET_ACCESS_PWD      0x46    // Configurar password de acceso

// ============================================================================
// Bancos de Memoria (Memory Banks)
// ============================================================================

#define MEM_BANK_RESERVED       0x00    // Kill Password + Access Password
#define MEM_BANK_EPC            0x01    // PC + EPC + CRC16
#define MEM_BANK_TID            0x02    // Tag Identifier (solo lectura)
#define MEM_BANK_USER           0x03    // Memoria de usuario (lectura/escritura)

// ============================================================================
// Regiones de Frecuencia
// ============================================================================

#define FREQ_CHINA_900          0x01    // 920.125 - 924.875 MHz
#define FREQ_US                 0x02    // 902.25 - 927.75 MHz (FCC)
#define FREQ_EU                 0x03    // 865.1 - 867.9 MHz (ETSI)
#define FREQ_CHINA_800          0x04    // 840.125 - 844.875 MHz
#define FREQ_KOREA              0x06    // 917.1 - 923.5 MHz

// ============================================================================
// Velocidades de UART
// ============================================================================

#define BAUD_9600               0x03
#define BAUD_19200              0x02
#define BAUD_38400              0x01
#define BAUD_57600              0x04
#define BAUD_115200             0x00

// ============================================================================
// Niveles de Potencia (dBm)
// ============================================================================

#define POWER_18_DBM            0x0708  // 18 dBm
#define POWER_20_DBM            0x07D0  // 20 dBm
#define POWER_22_DBM            0x0898  // 22 dBm
#define POWER_24_DBM            0x0960  // 24 dBm
#define POWER_26_DBM            0x0A28  // 26 dBm (máximo recomendado)

// ============================================================================
// Constantes de configuración
// ============================================================================

#define UHF_MAX_BUFFER_SIZE     256     // Tamaño máximo del buffer
#define UHF_MAX_EPC_LENGTH      32      // Máxima longitud de EPC en bytes
#define UHF_MAX_DATA_LENGTH     64      // Máxima longitud de datos
#define UHF_DEFAULT_TIMEOUT     2000    // Timeout por defecto en ms
#define UHF_INVENTORY_TIMEOUT   5000    // Timeout para inventory en ms
#define UHF_MAX_TAGS            50      // Máximo de etiquetas almacenables

// ============================================================================
// Estructura para almacenar información de una etiqueta
// ============================================================================

struct UHF_TagInfo {
    uint8_t epc[UHF_MAX_EPC_LENGTH];    // Datos del EPC
    uint8_t epcLen;                      // Longitud del EPC en bytes
    uint8_t rssi;                        // Intensidad de señal (RSSI)
    uint16_t pc;                         // Protocol Control word
    uint8_t antenna;                     // Antena que detectó la etiqueta
    
    // Datos leídos de memoria
    uint8_t data[UHF_MAX_DATA_LENGTH];   // Buffer de datos leídos
    uint8_t dataLen;                     // Longitud de datos leídos
    
    // Para imprimir el EPC como string hexadecimal
    String getEPCString() const {
        String result = "";
        for (int i = 0; i < epcLen; i++) {
            if (epc[i] < 0x10) result += "0";
            result += String(epc[i], HEX);
            if (i < epcLen - 1) result += " ";
        }
        result.toUpperCase();
        return result;
    }
    
    // Para obtener datos leídos como string hexadecimal
    String getDataString() const {
        String result = "";
        for (int i = 0; i < dataLen; i++) {
            if (data[i] < 0x10) result += "0";
            result += String(data[i], HEX);
            if (i < dataLen - 1) result += " ";
        }
        result.toUpperCase();
        return result;
    }
};

// ============================================================================
// Códigos de Error / Estado
// ============================================================================

enum UHF_Status {
    UHF_OK                      = 0x00,  // Operación exitosa
    UHF_ERR_TIMEOUT             = 0x01,  // Timeout esperando respuesta
    UHF_ERR_INVALID_RESPONSE    = 0x02,  // Respuesta inválida
    UHF_ERR_CRC_ERROR           = 0x03,  // Error de checksum
    UHF_ERR_NO_TAG              = 0x04,  // No se encontró etiqueta
    UHF_ERR_READ_FAIL           = 0x05,  // Error de lectura
    UHF_ERR_WRITE_FAIL          = 0x06,  // Error de escritura
    UHF_ERR_LOCK_FAIL           = 0x07,  // Error al bloquear
    UHF_ERR_INSUFFICIENT_POWER  = 0x08,  // Potencia insuficiente
    UHF_ERR_MEMORY_OVERFLOW     = 0x09,  // Desbordamiento de memoria
    UHF_ERR_ACCESS_DENIED       = 0x0A,  // Acceso denegado (password)
    UHF_ERR_TAG_NOT_FOUND       = 0x0B,  // Etiqueta específica no encontrada
    UHF_ERR_SERIAL_ERROR        = 0x0C,  // Error de comunicación serial
    UHF_ERR_UNKNOWN             = 0xFF   // Error desconocido
};

// ============================================================================
// Callbacks
// ============================================================================

typedef void (*TagFoundCallback)(const UHF_TagInfo &tag);

// ============================================================================
// Clase Principal: UHF_RFID
// ============================================================================

class UHF_RFID {
public:
    /**
     * Constructor
     * @param serial Referencia al puerto serial hardware
     */
    UHF_RFID(HardwareSerial &serial);
    
    /**
     * Inicializar el módulo UHF RFID
     * @param rxPin Pin RX del ESP32 (conectar a TX del módulo)
     * @param txPin Pin TX del ESP32 (conectar a RX del módulo)
     * @param baudRate Velocidad de comunicación (default: 115200)
     * @return UHF_OK si se inicializó correctamente
     */
    UHF_Status begin(int rxPin, int txPin, uint32_t baudRate = 115200);
    
    // ========================================================================
    // Información del Módulo
    // ========================================================================
    
    /**
     * Obtener versión de hardware del módulo
     * @param version Buffer donde se almacenará la versión
     * @return UHF_OK si se obtuvo correctamente
     */
    UHF_Status getHardwareVersion(String &version);
    
    /**
     * Obtener versión de firmware/software del módulo
     * @param version Buffer donde se almacenará la versión
     * @return UHF_OK si se obtuvo correctamente
     */
    UHF_Status getSoftwareVersion(String &version);
    
    // ========================================================================
    // Configuración
    // ========================================================================
    
    /**
     * Configurar la potencia de transmisión
     * @param power Potencia en centésimas de dBm (ej: 2600 = 26.00 dBm)
     * @return UHF_OK si se configuró correctamente
     */
    UHF_Status setTransmitPower(uint16_t power);
    
    /**
     * Obtener la potencia de transmisión actual
     * @param power Referencia donde se almacenará la potencia
     * @return UHF_OK si se obtuvo correctamente
     */
    UHF_Status getTransmitPower(uint16_t &power);
    
    /**
     * Configurar la región de frecuencia
     * @param region Código de región (FREQ_US, FREQ_EU, etc.)
     * @return UHF_OK si se configuró correctamente
     */
    UHF_Status setFrequencyRegion(uint8_t region);
    
    /**
     * Obtener la región de frecuencia actual
     * @param region Referencia donde se almacenará la región
     * @return UHF_OK si se obtuvo correctamente
     */
    UHF_Status getFrequencyRegion(uint8_t &region);
    
    /**
     * Configurar velocidad de comunicación UART
     * @param baudRate Código de velocidad (BAUD_9600, BAUD_115200, etc.)
     * @return UHF_OK si se configuró correctamente
     */
    UHF_Status setBaudRate(uint8_t baudRate);
    
    /**
     * Habilitar o deshabilitar el buzzer/beep del módulo
     * @param enable true para habilitar, false para deshabilitar
     * @return UHF_OK si se configuró correctamente
     */
    UHF_Status setBeep(bool enable);
    
    // ========================================================================
    // Operaciones de Inventory (Lectura de etiquetas)
    // ========================================================================
    
    /**
     * Lectura individual - leer UNA etiqueta
     * @param tag Referencia donde se almacenará la info de la etiqueta
     * @return UHF_OK si se leyó una etiqueta
     */
    UHF_Status singleRead(UHF_TagInfo &tag);
    
    /**
     * Lectura múltiple - leer varias etiquetas
     * @param tags Array donde se almacenarán las etiquetas detectadas
     * @param maxTags Máximo de etiquetas a leer
     * @param tagsFound Referencia donde se almacenará el número de etiquetas encontradas
     * @param durationMs Duración de la lectura en milisegundos
     * @return UHF_OK si la operación fue exitosa
     */
    UHF_Status multiRead(UHF_TagInfo *tags, uint8_t maxTags, uint8_t &tagsFound, uint16_t durationMs = 3000);
    
    /**
     * Detener lectura múltiple en progreso
     * @return UHF_OK si se detuvo correctamente
     */
    UHF_Status stopRead();
    
    /**
     * Iniciar lectura continua con callback
     * @param callback Función a llamar cada vez que se detecte una etiqueta
     * @param durationMs Duración en ms (0 = indefinido, usar stopRead())
     */
    void startContinuousRead(TagFoundCallback callback, uint16_t durationMs = 0);
    
    /**
     * Procesar datos entrantes (llamar en loop() durante lectura continua)
     */
    void process();
    
    /**
     * Verificar si hay lectura continua activa
     * @return true si hay lectura activa
     */
    bool isReading() const;
    
    // ========================================================================
    // Operaciones de Lectura de Memoria
    // ========================================================================
    
    /**
     * Leer datos de un banco de memoria de una etiqueta
     * @param memBank Banco de memoria (MEM_BANK_EPC, MEM_BANK_TID, etc.)
     * @param startAddr Dirección de inicio (en words de 16 bits)
     * @param wordCount Número de words a leer
     * @param data Buffer donde se almacenarán los datos leídos
     * @param dataLen Longitud de datos leídos
     * @param accessPwd Password de acceso (4 bytes, 0x00000000 por defecto)
     * @return UHF_OK si se leyó correctamente
     */
    UHF_Status readTagMemory(uint8_t memBank, uint8_t startAddr, uint8_t wordCount,
                             uint8_t *data, uint8_t &dataLen, 
                             uint32_t accessPwd = 0x00000000);
    
    /**
     * Leer el TID (Tag Identifier) de una etiqueta
     * @param tag Referencia donde se almacenarán los datos del TID
     * @return UHF_OK si se leyó correctamente
     */
    UHF_Status readTID(UHF_TagInfo &tag);
    
    /**
     * Leer la memoria de usuario de una etiqueta
     * @param startAddr Dirección de inicio
     * @param wordCount Número de words a leer
     * @param data Buffer de datos
     * @param dataLen Longitud de datos leídos
     * @param accessPwd Password de acceso
     * @return UHF_OK si se leyó correctamente
     */
    UHF_Status readUserMemory(uint8_t startAddr, uint8_t wordCount,
                              uint8_t *data, uint8_t &dataLen,
                              uint32_t accessPwd = 0x00000000);
    
    // ========================================================================
    // Operaciones de Escritura
    // ========================================================================
    
    /**
     * Escribir datos en un banco de memoria de una etiqueta
     * @param memBank Banco de memoria
     * @param startAddr Dirección de inicio (en words de 16 bits)
     * @param data Datos a escribir
     * @param dataLen Longitud de datos (en bytes, debe ser múltiplo de 2)
     * @param accessPwd Password de acceso
     * @return UHF_OK si se escribió correctamente
     */
    UHF_Status writeTagMemory(uint8_t memBank, uint8_t startAddr,
                              const uint8_t *data, uint8_t dataLen,
                              uint32_t accessPwd = 0x00000000);
    
    /**
     * Escribir un nuevo EPC en una etiqueta
     * IMPORTANTE: Solo una etiqueta debe estar en el campo de lectura
     * @param newEPC Datos del nuevo EPC
     * @param epcLen Longitud del EPC en bytes (debe ser múltiplo de 2)
     * @param accessPwd Password de acceso
     * @return UHF_OK si se escribió correctamente
     */
    UHF_Status writeEPC(const uint8_t *newEPC, uint8_t epcLen,
                        uint32_t accessPwd = 0x00000000);
    
    /**
     * Escribir datos en la memoria de usuario
     * @param startAddr Dirección de inicio
     * @param data Datos a escribir
     * @param dataLen Longitud en bytes
     * @param accessPwd Password de acceso
     * @return UHF_OK si se escribió correctamente
     */
    UHF_Status writeUserMemory(uint8_t startAddr, const uint8_t *data,
                               uint8_t dataLen, uint32_t accessPwd = 0x00000000);
    
    // ========================================================================
    // Seguridad
    // ========================================================================
    
    /**
     * Configurar password de acceso de una etiqueta
     * @param newPassword Nuevo password (4 bytes)
     * @param currentPassword Password actual (4 bytes, 0x00000000 por defecto)
     * @return UHF_OK si se configuró correctamente
     */
    UHF_Status setAccessPassword(uint32_t newPassword, uint32_t currentPassword = 0x00000000);
    
    /**
     * Bloquear/proteger bancos de memoria de una etiqueta
     * @param lockPayload Configuración de bloqueo (ver documentación)
     * @param accessPwd Password de acceso
     * @return UHF_OK si se bloqueó correctamente
     */
    UHF_Status lockTag(uint32_t lockPayload, uint32_t accessPwd);
    
    // ========================================================================
    // Utilidades
    // ========================================================================
    
    /**
     * Obtener descripción de un código de error
     * @param status Código de estado
     * @return String con la descripción del error
     */
    static String getStatusString(UHF_Status status);
    
    /**
     * Activar/desactivar modo debug (imprime tramas por Serial)
     * @param enable true para activar debug
     */
    void setDebug(bool enable);

private:
    HardwareSerial &_serial;
    bool _debug;
    bool _reading;                              // Flag de lectura continua
    TagFoundCallback _tagCallback;              // Callback de etiqueta encontrada
    unsigned long _readStartTime;               // Tiempo de inicio de lectura
    uint16_t _readDuration;                     // Duración programada
    
    uint8_t _txBuffer[UHF_MAX_BUFFER_SIZE];     // Buffer de transmisión
    uint8_t _rxBuffer[UHF_MAX_BUFFER_SIZE];     // Buffer de recepción
    uint16_t _rxIndex;                          // Índice del buffer de recepción
    
    /**
     * Construir y enviar un frame de comando
     * @param cmd Código de comando
     * @param params Datos/parámetros del comando
     * @param paramLen Longitud de los parámetros
     */
    void sendCommand(uint8_t cmd, const uint8_t *params = nullptr, uint16_t paramLen = 0);
    
    /**
     * Esperar y recibir una respuesta del módulo
     * @param timeoutMs Timeout en milisegundos
     * @return UHF_OK si se recibió respuesta válida
     */
    UHF_Status receiveResponse(uint16_t timeoutMs = UHF_DEFAULT_TIMEOUT);
    
    /**
     * Calcular checksum del frame
     * @param data Datos del frame (desde Type hasta último parámetro)
     * @param len Longitud de datos
     * @return Byte de checksum
     */
    uint8_t calculateChecksum(const uint8_t *data, uint16_t len);
    
    /**
     * Validar un frame recibido
     * @return true si el frame es válido
     */
    bool validateFrame();
    
    /**
     * Parsear la respuesta de un inventory (single/multi read)
     * @param tag Referencia donde almacenar la información
     * @return UHF_OK si se parseó correctamente
     */
    UHF_Status parseInventoryResponse(UHF_TagInfo &tag);
    
    /**
     * Parsear la respuesta de una lectura de memoria
     * @param data Buffer de datos
     * @param dataLen Longitud de datos parseados
     * @return UHF_OK si se parseó correctamente
     */
    UHF_Status parseReadResponse(uint8_t *data, uint8_t &dataLen);
    
    /**
     * Imprimir buffer en formato hexadecimal (debug)
     * @param label Etiqueta descriptiva
     * @param buffer Datos a imprimir
     * @param len Longitud de datos
     */
    void printHex(const char *label, const uint8_t *buffer, uint16_t len);
    
    /**
     * Limpiar el buffer de recepción serial
     */
    void flushSerial();
};

#endif // UHF_RFID_H
