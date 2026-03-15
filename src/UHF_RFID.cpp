/**
 * ============================================================================
 * UHF RFID Reader Module - Implementación
 * ============================================================================
 * 
 * Implementación del driver para lector UHF RFID con ESP32-C3
 * Protocolo serial: Header 0xBB | Type | Command | PL(2) | Params | Checksum | End 0x7E
 * 
 * ============================================================================
 */

#include "UHF_RFID.h"

// ============================================================================
// Constructor
// ============================================================================

UHF_RFID::UHF_RFID(HardwareSerial &serial) 
    : _serial(serial), 
      _debug(false), 
      _reading(false),
      _tagCallback(nullptr),
      _readStartTime(0),
      _readDuration(0),
      _rxIndex(0) {
    memset(_txBuffer, 0, UHF_MAX_BUFFER_SIZE);
    memset(_rxBuffer, 0, UHF_MAX_BUFFER_SIZE);
}

// ============================================================================
// Inicialización
// ============================================================================

UHF_Status UHF_RFID::begin(int rxPin, int txPin, uint32_t baudRate) {
    // Inicializar el puerto serial con los pines especificados
    _serial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    
    // Esperar a que el módulo se estabilice
    delay(500);
    
    // Limpiar buffer serial
    flushSerial();
    
    // Verificar comunicación obteniendo versión de hardware
    String version;
    UHF_Status status = getHardwareVersion(version);
    
    if (status == UHF_OK) {
        if (_debug) {
            Serial.println("╔══════════════════════════════════════╗");
            Serial.println("║   UHF RFID Module Inicializado OK   ║");
            Serial.println("╠══════════════════════════════════════╣");
            Serial.printf( "║  HW Version: %-22s ║\n", version.c_str());
            Serial.printf( "║  Baud Rate:  %-22lu ║\n", baudRate);
            Serial.printf( "║  RX Pin:     %-22d ║\n", rxPin);
            Serial.printf( "║  TX Pin:     %-22d ║\n", txPin);
            Serial.println("╚══════════════════════════════════════╝");
        }
    } else {
        if (_debug) {
            Serial.println("╔══════════════════════════════════════╗");
            Serial.println("║  ⚠ ERROR: Módulo UHF no detectado   ║");
            Serial.println("╠══════════════════════════════════════╣");
            Serial.println("║  Verificar:                          ║");
            Serial.println("║  1. Conexiones TX/RX                 ║");
            Serial.println("║  2. Alimentación 12V del módulo      ║");
            Serial.println("║  3. Convertidor de nivel RS232/TTL   ║");
            Serial.println("║  4. Velocidad de comunicación        ║");
            Serial.println("╚══════════════════════════════════════╝");
        }
    }
    
    return status;
}

// ============================================================================
// Información del Módulo
// ============================================================================

UHF_Status UHF_RFID::getHardwareVersion(String &version) {
    // Comando: BB 00 03 00 01 00 04 7E
    uint8_t params[] = {0x00};
    sendCommand(CMD_GET_HW_VERSION, params, 1);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    // Validar que la respuesta es para el comando correcto
    if (_rxBuffer[2] != CMD_GET_HW_VERSION) return UHF_ERR_INVALID_RESPONSE;
    
    // Extraer longitud de parámetros
    uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
    
    // Construir string de versión
    version = "";
    for (uint16_t i = 0; i < paramLen; i++) {
        if (_rxBuffer[5 + i] < 0x10) version += "0";
        version += String(_rxBuffer[5 + i], HEX);
        if (i < paramLen - 1) version += ".";
    }
    version.toUpperCase();
    
    return UHF_OK;
}

UHF_Status UHF_RFID::getSoftwareVersion(String &version) {
    uint8_t params[] = {0x00};
    sendCommand(CMD_GET_SW_VERSION, params, 1);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_rxBuffer[2] != CMD_GET_SW_VERSION) return UHF_ERR_INVALID_RESPONSE;
    
    uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
    
    version = "";
    for (uint16_t i = 0; i < paramLen; i++) {
        if (_rxBuffer[5 + i] < 0x10) version += "0";
        version += String(_rxBuffer[5 + i], HEX);
        if (i < paramLen - 1) version += ".";
    }
    version.toUpperCase();
    
    return UHF_OK;
}

// ============================================================================
// Configuración
// ============================================================================

UHF_Status UHF_RFID::setTransmitPower(uint16_t power) {
    // El power se envía en 2 bytes (MSB first)
    // Ejemplo: 2600 = 0x0A28
    uint8_t params[2];
    params[0] = (power >> 8) & 0xFF;  // MSB
    params[1] = power & 0xFF;          // LSB
    
    sendCommand(CMD_SET_POWER, params, 2);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    // Verificar respuesta exitosa (Type 0x01, Command echo)
    if (_rxBuffer[1] != UHF_TYPE_RESPONSE || _rxBuffer[2] != CMD_SET_POWER) {
        return UHF_ERR_INVALID_RESPONSE;
    }
    
    if (_debug) {
        Serial.printf("[UHF] Potencia configurada: %d.%02d dBm\n", power / 100, power % 100);
    }
    
    return UHF_OK;
}

UHF_Status UHF_RFID::getTransmitPower(uint16_t &power) {
    sendCommand(CMD_GET_POWER, nullptr, 0);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_rxBuffer[2] != CMD_GET_POWER) return UHF_ERR_INVALID_RESPONSE;
    
    power = ((uint16_t)_rxBuffer[5] << 8) | _rxBuffer[6];
    
    if (_debug) {
        Serial.printf("[UHF] Potencia actual: %d.%02d dBm\n", power / 100, power % 100);
    }
    
    return UHF_OK;
}

UHF_Status UHF_RFID::setFrequencyRegion(uint8_t region) {
    uint8_t params[3];
    params[0] = region;
    params[1] = 0x00;   // Start channel offset
    params[2] = 0x00;   // End channel offset (0 = auto)
    
    sendCommand(CMD_SET_FREQUENCY, params, 3);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_rxBuffer[2] != CMD_SET_FREQUENCY) return UHF_ERR_INVALID_RESPONSE;
    
    if (_debug) {
        const char* regionNames[] = {"", "China 900", "US (FCC)", "EU (ETSI)", "China 800", "", "Korea"};
        if (region <= 6) {
            Serial.printf("[UHF] Región de frecuencia: %s\n", regionNames[region]);
        }
    }
    
    return UHF_OK;
}

UHF_Status UHF_RFID::getFrequencyRegion(uint8_t &region) {
    sendCommand(CMD_GET_FREQUENCY, nullptr, 0);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_rxBuffer[2] != CMD_GET_FREQUENCY) return UHF_ERR_INVALID_RESPONSE;
    
    region = _rxBuffer[5];
    
    return UHF_OK;
}

UHF_Status UHF_RFID::setBaudRate(uint8_t baudRate) {
    uint8_t params[] = {baudRate};
    sendCommand(CMD_SET_BAUDRATE, params, 1);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    return UHF_OK;
}

UHF_Status UHF_RFID::setBeep(bool enable) {
    uint8_t params[] = {enable ? (uint8_t)0x01 : (uint8_t)0x00};
    sendCommand(CMD_SET_BEEP, params, 1);
    
    UHF_Status status = receiveResponse(UHF_DEFAULT_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_debug) {
        Serial.printf("[UHF] Buzzer: %s\n", enable ? "Activado" : "Desactivado");
    }
    
    return UHF_OK;
}

// ============================================================================
// Operaciones de Inventory (Lectura de etiquetas)
// ============================================================================

UHF_Status UHF_RFID::singleRead(UHF_TagInfo &tag) {
    // Comando Single Poll: BB 00 22 00 00 22 7E
    sendCommand(CMD_SINGLE_POLL, nullptr, 0);
    
    UHF_Status status = receiveResponse(UHF_INVENTORY_TIMEOUT);
    if (status != UHF_OK) return status;
    
    // Verificar si es respuesta de inventory
    if (_rxBuffer[2] == CMD_SINGLE_POLL) {
        // Verificar si hay error (parámetro con código de error)
        uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
        if (paramLen == 1 && _rxBuffer[1] == UHF_TYPE_RESPONSE) {
            // Respuesta de error o no tag
            return UHF_ERR_NO_TAG;
        }
        return parseInventoryResponse(tag);
    }
    
    return UHF_ERR_INVALID_RESPONSE;
}

UHF_Status UHF_RFID::multiRead(UHF_TagInfo *tags, uint8_t maxTags, uint8_t &tagsFound, uint16_t durationMs) {
    tagsFound = 0;
    
    // Comando Multi Poll: BB 00 27 00 03 22 27 10 checksum 7E
    // Parámetros: 0x22 = inventory type, 0x00 0x0A = repeticiones (10 veces)
    uint8_t repeat_hi = (durationMs / 100) >> 8;
    uint8_t repeat_lo = (durationMs / 100) & 0xFF;
    uint8_t params[] = {0x22, repeat_hi, repeat_lo};
    
    sendCommand(CMD_MULTI_POLL, params, 3);
    
    unsigned long startTime = millis();
    unsigned long timeout = durationMs + 2000; // Margen extra
    
    while (millis() - startTime < timeout && tagsFound < maxTags) {
        UHF_Status status = receiveResponse(timeout - (millis() - startTime));
        
        if (status == UHF_ERR_TIMEOUT) break;
        if (status != UHF_OK) continue;
        
        // Verificar tipo de respuesta
        if (_rxBuffer[1] == UHF_TYPE_RESPONSE && _rxBuffer[2] == CMD_MULTI_POLL) {
            // Respuesta de inventory con datos de tag
            uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
            if (paramLen > 1) {
                UHF_Status parseStatus = parseInventoryResponse(tags[tagsFound]);
                if (parseStatus == UHF_OK) {
                    // Verificar si es un tag duplicado
                    bool duplicate = false;
                    for (uint8_t i = 0; i < tagsFound; i++) {
                        if (tags[i].epcLen == tags[tagsFound].epcLen &&
                            memcmp(tags[i].epc, tags[tagsFound].epc, tags[i].epcLen) == 0) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        tagsFound++;
                        if (_debug) {
                            Serial.printf("[UHF] Tag #%d: %s (RSSI: -%d dBm)\n", 
                                          tagsFound, 
                                          tags[tagsFound - 1].getEPCString().c_str(),
                                          tags[tagsFound - 1].rssi);
                        }
                    }
                }
            } else {
                // Fin del inventory (respuesta corta indica finalización)
                break;
            }
        } else if (_rxBuffer[1] == UHF_TYPE_RESPONSE && _rxBuffer[2] == CMD_STOP_POLL) {
            // Confirmación de stop
            break;
        }
    }
    
    // Enviar stop por si acaso
    stopRead();
    
    if (_debug) {
        Serial.printf("[UHF] Inventory completado: %d etiquetas encontradas\n", tagsFound);
    }
    
    return (tagsFound > 0) ? UHF_OK : UHF_ERR_NO_TAG;
}

UHF_Status UHF_RFID::stopRead() {
    _reading = false;
    
    // Comando Stop: BB 00 28 00 00 28 7E
    sendCommand(CMD_STOP_POLL, nullptr, 0);
    
    // Esperar confirmación con timeout corto
    UHF_Status status = receiveResponse(1000);
    
    // Limpiar cualquier dato residual
    delay(100);
    flushSerial();
    
    return UHF_OK; // Siempre retornar OK para stop
}

void UHF_RFID::startContinuousRead(TagFoundCallback callback, uint16_t durationMs) {
    _tagCallback = callback;
    _reading = true;
    _readStartTime = millis();
    _readDuration = durationMs;
    
    // Iniciar multi-read con repeticiones altas
    uint8_t repeat_hi = 0xFF;
    uint8_t repeat_lo = 0xFF;
    if (durationMs > 0) {
        uint16_t repeats = durationMs / 100;
        repeat_hi = (repeats >> 8) & 0xFF;
        repeat_lo = repeats & 0xFF;
    }
    
    uint8_t params[] = {0x22, repeat_hi, repeat_lo};
    sendCommand(CMD_MULTI_POLL, params, 3);
}

void UHF_RFID::process() {
    if (!_reading) return;
    
    // Verificar timeout de duración
    if (_readDuration > 0 && millis() - _readStartTime > _readDuration) {
        stopRead();
        return;
    }
    
    // Intentar leer respuesta sin bloquear mucho
    UHF_Status status = receiveResponse(100);
    
    if (status == UHF_OK) {
        if (_rxBuffer[1] == UHF_TYPE_RESPONSE && _rxBuffer[2] == CMD_MULTI_POLL) {
            uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
            if (paramLen > 1 && _tagCallback != nullptr) {
                UHF_TagInfo tag;
                if (parseInventoryResponse(tag) == UHF_OK) {
                    _tagCallback(tag);
                }
            } else if (paramLen <= 1) {
                // Inventory terminó
                _reading = false;
            }
        }
    }
}

bool UHF_RFID::isReading() const {
    return _reading;
}

// ============================================================================
// Operaciones de Lectura de Memoria
// ============================================================================

UHF_Status UHF_RFID::readTagMemory(uint8_t memBank, uint8_t startAddr, uint8_t wordCount,
                                    uint8_t *data, uint8_t &dataLen, uint32_t accessPwd) {
    // Construir parámetros del comando de lectura
    // Formato: AccessPwd(4) + MemBank(1) + StartAddr(2) + WordCount(2)
    uint8_t params[9];
    
    // Access Password (4 bytes, MSB first)
    params[0] = (accessPwd >> 24) & 0xFF;
    params[1] = (accessPwd >> 16) & 0xFF;
    params[2] = (accessPwd >> 8) & 0xFF;
    params[3] = accessPwd & 0xFF;
    
    // Memory Bank
    params[4] = memBank;
    
    // Start Address (2 bytes, MSB first)
    params[5] = 0x00;
    params[6] = startAddr;
    
    // Word Count (2 bytes, MSB first)
    params[7] = 0x00;
    params[8] = wordCount;
    
    sendCommand(CMD_READ_DATA, params, 9);
    
    UHF_Status status = receiveResponse(UHF_INVENTORY_TIMEOUT);
    if (status != UHF_OK) return status;
    
    // Verificar respuesta
    if (_rxBuffer[2] != CMD_READ_DATA) {
        // Verificar si es un error del módulo
        if (_rxBuffer[1] == UHF_TYPE_RESPONSE) {
            return UHF_ERR_READ_FAIL;
        }
        return UHF_ERR_INVALID_RESPONSE;
    }
    
    return parseReadResponse(data, dataLen);
}

UHF_Status UHF_RFID::readTID(UHF_TagInfo &tag) {
    uint8_t data[UHF_MAX_DATA_LENGTH];
    uint8_t dataLen = 0;
    
    // Leer TID: banco 2, dirección 0, 6 words (12 bytes)
    UHF_Status status = readTagMemory(MEM_BANK_TID, 0x00, 0x06, data, dataLen);
    
    if (status == UHF_OK) {
        memcpy(tag.data, data, dataLen);
        tag.dataLen = dataLen;
        
        if (_debug) {
            Serial.print("[UHF] TID: ");
            for (int i = 0; i < dataLen; i++) {
                if (data[i] < 0x10) Serial.print("0");
                Serial.print(data[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    }
    
    return status;
}

UHF_Status UHF_RFID::readUserMemory(uint8_t startAddr, uint8_t wordCount,
                                     uint8_t *data, uint8_t &dataLen,
                                     uint32_t accessPwd) {
    return readTagMemory(MEM_BANK_USER, startAddr, wordCount, data, dataLen, accessPwd);
}

// ============================================================================
// Operaciones de Escritura
// ============================================================================

UHF_Status UHF_RFID::writeTagMemory(uint8_t memBank, uint8_t startAddr,
                                     const uint8_t *data, uint8_t dataLen,
                                     uint32_t accessPwd) {
    // Verificar que la longitud es múltiplo de 2 (words de 16 bits)
    if (dataLen % 2 != 0) {
        if (_debug) {
            Serial.println("[UHF] Error: La longitud de datos debe ser múltiplo de 2 bytes (words)");
        }
        return UHF_ERR_WRITE_FAIL;
    }
    
    uint8_t wordCount = dataLen / 2;
    
    // Construir parámetros: 
    // AccessPwd(4) + MemBank(1) + StartAddr(2) + WordCount(2) + Data(N)
    uint8_t paramLen = 9 + dataLen;
    uint8_t params[UHF_MAX_BUFFER_SIZE];
    
    // Access Password
    params[0] = (accessPwd >> 24) & 0xFF;
    params[1] = (accessPwd >> 16) & 0xFF;
    params[2] = (accessPwd >> 8) & 0xFF;
    params[3] = accessPwd & 0xFF;
    
    // Memory Bank
    params[4] = memBank;
    
    // Start Address
    params[5] = 0x00;
    params[6] = startAddr;
    
    // Word Count
    params[7] = 0x00;
    params[8] = wordCount;
    
    // Data
    memcpy(&params[9], data, dataLen);
    
    sendCommand(CMD_WRITE_DATA, params, paramLen);
    
    UHF_Status status = receiveResponse(UHF_INVENTORY_TIMEOUT);
    if (status != UHF_OK) return status;
    
    // Verificar respuesta exitosa
    if (_rxBuffer[1] == UHF_TYPE_RESPONSE && _rxBuffer[2] == CMD_WRITE_DATA) {
        // Verificar parámetros de respuesta (código de estado)
        uint16_t respParamLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
        if (respParamLen >= 1) {
            // Algunos módulos devuelven un código de status
            if (_debug) {
                Serial.printf("[UHF] Escritura exitosa en banco %d, dirección %d, %d words\n",
                              memBank, startAddr, wordCount);
            }
            return UHF_OK;
        }
    }
    
    return UHF_ERR_WRITE_FAIL;
}

UHF_Status UHF_RFID::writeEPC(const uint8_t *newEPC, uint8_t epcLen, uint32_t accessPwd) {
    if (epcLen % 2 != 0 || epcLen == 0) {
        if (_debug) {
            Serial.println("[UHF] Error: EPC debe tener longitud par y > 0");
        }
        return UHF_ERR_WRITE_FAIL;
    }
    
    // Método 1: Usar comando específico de escritura EPC (CMD_WRITE_EPC)
    // Parámetros: AccessPwd(4) + EPC data(N)
    uint8_t paramLen = 4 + epcLen;
    uint8_t params[UHF_MAX_BUFFER_SIZE];
    
    // Access Password
    params[0] = (accessPwd >> 24) & 0xFF;
    params[1] = (accessPwd >> 16) & 0xFF;
    params[2] = (accessPwd >> 8) & 0xFF;
    params[3] = accessPwd & 0xFF;
    
    // Nuevo EPC
    memcpy(&params[4], newEPC, epcLen);
    
    sendCommand(CMD_WRITE_EPC, params, paramLen);
    
    UHF_Status status = receiveResponse(UHF_INVENTORY_TIMEOUT);
    if (status != UHF_OK) {
        if (_debug) {
            Serial.println("[UHF] Comando WriteEPC falló, intentando con WriteData...");
        }
        // Método alternativo: usar writeTagMemory directamente
        // EPC empieza en word 2 del banco EPC (words 0-1 son CRC y PC)
        return writeTagMemory(MEM_BANK_EPC, 0x02, newEPC, epcLen, accessPwd);
    }
    
    if (_rxBuffer[2] != CMD_WRITE_EPC) {
        return UHF_ERR_WRITE_FAIL;
    }
    
    if (_debug) {
        Serial.print("[UHF] Nuevo EPC escrito: ");
        for (int i = 0; i < epcLen; i++) {
            if (newEPC[i] < 0x10) Serial.print("0");
            Serial.print(newEPC[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    return UHF_OK;
}

UHF_Status UHF_RFID::writeUserMemory(uint8_t startAddr, const uint8_t *data,
                                      uint8_t dataLen, uint32_t accessPwd) {
    return writeTagMemory(MEM_BANK_USER, startAddr, data, dataLen, accessPwd);
}

// ============================================================================
// Seguridad
// ============================================================================

UHF_Status UHF_RFID::setAccessPassword(uint32_t newPassword, uint32_t currentPassword) {
    // El password de acceso está en el banco Reserved, words 2-3
    uint8_t pwdData[4];
    pwdData[0] = (newPassword >> 24) & 0xFF;
    pwdData[1] = (newPassword >> 16) & 0xFF;
    pwdData[2] = (newPassword >> 8) & 0xFF;
    pwdData[3] = newPassword & 0xFF;
    
    return writeTagMemory(MEM_BANK_RESERVED, 0x02, pwdData, 4, currentPassword);
}

UHF_Status UHF_RFID::lockTag(uint32_t lockPayload, uint32_t accessPwd) {
    // Parámetros: AccessPwd(4) + LockPayload(3)
    uint8_t params[7];
    
    params[0] = (accessPwd >> 24) & 0xFF;
    params[1] = (accessPwd >> 16) & 0xFF;
    params[2] = (accessPwd >> 8) & 0xFF;
    params[3] = accessPwd & 0xFF;
    
    params[4] = (lockPayload >> 16) & 0xFF;
    params[5] = (lockPayload >> 8) & 0xFF;
    params[6] = lockPayload & 0xFF;
    
    sendCommand(CMD_LOCK_TAG, params, 7);
    
    UHF_Status status = receiveResponse(UHF_INVENTORY_TIMEOUT);
    if (status != UHF_OK) return status;
    
    if (_rxBuffer[2] != CMD_LOCK_TAG) return UHF_ERR_LOCK_FAIL;
    
    return UHF_OK;
}

// ============================================================================
// Utilidades
// ============================================================================

String UHF_RFID::getStatusString(UHF_Status status) {
    switch (status) {
        case UHF_OK:                    return "OK - Operación exitosa";
        case UHF_ERR_TIMEOUT:           return "ERROR - Timeout esperando respuesta";
        case UHF_ERR_INVALID_RESPONSE:  return "ERROR - Respuesta inválida del módulo";
        case UHF_ERR_CRC_ERROR:         return "ERROR - Error de checksum (CRC)";
        case UHF_ERR_NO_TAG:            return "ERROR - No se encontró etiqueta";
        case UHF_ERR_READ_FAIL:         return "ERROR - Fallo en lectura";
        case UHF_ERR_WRITE_FAIL:        return "ERROR - Fallo en escritura";
        case UHF_ERR_LOCK_FAIL:         return "ERROR - Fallo al bloquear etiqueta";
        case UHF_ERR_INSUFFICIENT_POWER:return "ERROR - Potencia insuficiente";
        case UHF_ERR_MEMORY_OVERFLOW:   return "ERROR - Desbordamiento de memoria";
        case UHF_ERR_ACCESS_DENIED:     return "ERROR - Acceso denegado (verificar password)";
        case UHF_ERR_TAG_NOT_FOUND:     return "ERROR - Etiqueta específica no encontrada";
        case UHF_ERR_SERIAL_ERROR:      return "ERROR - Error de comunicación serial";
        default:                        return "ERROR - Error desconocido";
    }
}

void UHF_RFID::setDebug(bool enable) {
    _debug = enable;
}

// ============================================================================
// Funciones Privadas: Protocolo Serial
// ============================================================================

void UHF_RFID::sendCommand(uint8_t cmd, const uint8_t *params, uint16_t paramLen) {
    uint16_t idx = 0;
    
    // Header
    _txBuffer[idx++] = UHF_HEADER;
    
    // Type (siempre 0x00 para comandos del host)
    _txBuffer[idx++] = UHF_TYPE_COMMAND;
    
    // Command
    _txBuffer[idx++] = cmd;
    
    // Parameter Length (2 bytes, MSB first)
    _txBuffer[idx++] = (paramLen >> 8) & 0xFF;
    _txBuffer[idx++] = paramLen & 0xFF;
    
    // Parameters
    if (params != nullptr && paramLen > 0) {
        memcpy(&_txBuffer[idx], params, paramLen);
        idx += paramLen;
    }
    
    // Checksum: suma de bytes desde Type hasta último parámetro (LSB)
    uint8_t checksum = calculateChecksum(&_txBuffer[1], idx - 1);
    _txBuffer[idx++] = checksum;
    
    // End
    _txBuffer[idx++] = UHF_END;
    
    if (_debug) {
        printHex("TX", _txBuffer, idx);
    }
    
    // Enviar por serial
    _serial.write(_txBuffer, idx);
    _serial.flush();
}

UHF_Status UHF_RFID::receiveResponse(uint16_t timeoutMs) {
    _rxIndex = 0;
    memset(_rxBuffer, 0, UHF_MAX_BUFFER_SIZE);
    
    unsigned long startTime = millis();
    bool headerFound = false;
    bool complete = false;
    uint16_t expectedLen = 0;
    
    while (millis() - startTime < timeoutMs && !complete) {
        if (_serial.available()) {
            uint8_t byte = _serial.read();
            
            if (!headerFound) {
                // Buscar header 0xBB
                if (byte == UHF_HEADER) {
                    _rxBuffer[0] = byte;
                    _rxIndex = 1;
                    headerFound = true;
                }
            } else {
                if (_rxIndex < UHF_MAX_BUFFER_SIZE) {
                    _rxBuffer[_rxIndex++] = byte;
                    
                    // Una vez tenemos Type + Cmd + PL(2), calculamos longitud esperada
                    if (_rxIndex == 5) {
                        expectedLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
                    }
                    
                    // Frame completo: Header(1) + Type(1) + Cmd(1) + PL(2) + Params(N) + Checksum(1) + End(1)
                    if (_rxIndex >= 5 && _rxIndex == 5 + expectedLen + 2) {
                        // Verificar End byte
                        if (_rxBuffer[_rxIndex - 1] == UHF_END) {
                            complete = true;
                        } else {
                            // Frame corrupto, reiniciar
                            headerFound = false;
                            _rxIndex = 0;
                        }
                    }
                } else {
                    // Buffer overflow, reiniciar
                    headerFound = false;
                    _rxIndex = 0;
                }
            }
        }
    }
    
    if (!complete) {
        if (_debug && _rxIndex > 0) {
            Serial.printf("[UHF] Timeout parcial (%d bytes recibidos)\n", _rxIndex);
            printHex("RX parcial", _rxBuffer, _rxIndex);
        }
        return UHF_ERR_TIMEOUT;
    }
    
    if (_debug) {
        printHex("RX", _rxBuffer, _rxIndex);
    }
    
    // Validar checksum
    if (!validateFrame()) {
        if (_debug) {
            Serial.println("[UHF] Error de checksum en respuesta");
        }
        return UHF_ERR_CRC_ERROR;
    }
    
    // Verificar si la respuesta indica un error del módulo
    if (_rxBuffer[1] == UHF_TYPE_RESPONSE) {
        uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
        if (paramLen == 1) {
            uint8_t errorCode = _rxBuffer[5];
            if (errorCode != 0x00) {
                if (_debug) {
                    Serial.printf("[UHF] Módulo reportó error: 0x%02X\n", errorCode);
                }
                // Mapear errores del módulo a nuestros códigos
                switch (errorCode) {
                    case 0x09: return UHF_ERR_NO_TAG;
                    case 0x0A: return UHF_ERR_ACCESS_DENIED;
                    case 0x05: case 0x06: return UHF_ERR_READ_FAIL;
                    case 0x03: case 0x04: return UHF_ERR_WRITE_FAIL;
                    case 0x0B: return UHF_ERR_LOCK_FAIL;
                    default:   return UHF_ERR_UNKNOWN;
                }
            }
        }
    }
    
    return UHF_OK;
}

uint8_t UHF_RFID::calculateChecksum(const uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

bool UHF_RFID::validateFrame() {
    if (_rxIndex < 7) return false;  // Frame mínimo
    
    // Verificar header y end
    if (_rxBuffer[0] != UHF_HEADER) return false;
    if (_rxBuffer[_rxIndex - 1] != UHF_END) return false;
    
    // Calcular checksum (bytes desde Type hasta último parámetro)
    // Excluir Header, Checksum y End
    uint16_t checksumLen = _rxIndex - 3; // -1 header, -1 checksum, -1 end
    uint8_t calculated = calculateChecksum(&_rxBuffer[1], checksumLen);
    uint8_t received = _rxBuffer[_rxIndex - 2];
    
    return (calculated == received);
}

UHF_Status UHF_RFID::parseInventoryResponse(UHF_TagInfo &tag) {
    memset(&tag, 0, sizeof(UHF_TagInfo));
    
    uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
    
    if (paramLen < 5) return UHF_ERR_INVALID_RESPONSE;
    
    uint16_t idx = 5; // Inicio de los parámetros
    
    // RSSI (1 byte)
    tag.rssi = _rxBuffer[idx++];
    
    // PC (Protocol Control, 2 bytes)
    tag.pc = ((uint16_t)_rxBuffer[idx] << 8) | _rxBuffer[idx + 1];
    idx += 2;
    
    // EPC length: paramLen - 1(RSSI) - 2(PC) - 1(posible CRC o conteo)
    // El PC word contiene la longitud del EPC en los 5 bits más significativos
    uint8_t epcWordCount = (tag.pc >> 11) & 0x1F;
    tag.epcLen = epcWordCount * 2;
    
    // Validar que no exceda el tamaño del buffer
    if (tag.epcLen > UHF_MAX_EPC_LENGTH) {
        tag.epcLen = UHF_MAX_EPC_LENGTH;
    }
    
    // Verificar que hay suficientes bytes en la respuesta
    if (idx + tag.epcLen > 5 + paramLen) {
        // Ajustar: calcular EPC basado en los bytes disponibles
        tag.epcLen = (5 + paramLen) - idx;
        if (tag.epcLen > 1) tag.epcLen -= 1; // Resta CRC si lo hay
    }
    
    // Copiar EPC
    if (tag.epcLen > 0) {
        memcpy(tag.epc, &_rxBuffer[idx], tag.epcLen);
    }
    
    return UHF_OK;
}

UHF_Status UHF_RFID::parseReadResponse(uint8_t *data, uint8_t &dataLen) {
    uint16_t paramLen = ((uint16_t)_rxBuffer[3] << 8) | _rxBuffer[4];
    
    if (paramLen < 1) return UHF_ERR_INVALID_RESPONSE;
    
    // La respuesta de lectura contiene:
    // - Byte count (1 byte) + datos leídos
    // O puede ser: RSSI + PC + EPC + datos (depende del módulo)
    
    // Para simplificar, los datos están en el campo de parámetros
    // Primero puede venir un conteo o directamente los datos
    
    uint16_t idx = 5;
    
    // Verificar si el primer byte es un conteo de bytes leídos
    uint8_t byteCount = _rxBuffer[idx];
    
    if (byteCount <= paramLen && byteCount > 0 && byteCount <= UHF_MAX_DATA_LENGTH) {
        // Formato: count + datos
        idx++;
        dataLen = byteCount;
        memcpy(data, &_rxBuffer[idx], dataLen);
    } else {
        // Formato directo: todos los parámetros son datos
        dataLen = (paramLen > UHF_MAX_DATA_LENGTH) ? UHF_MAX_DATA_LENGTH : paramLen;
        memcpy(data, &_rxBuffer[idx], dataLen);
    }
    
    return UHF_OK;
}

void UHF_RFID::printHex(const char *label, const uint8_t *buffer, uint16_t len) {
    Serial.printf("[UHF] %s (%d bytes): ", label, len);
    for (uint16_t i = 0; i < len; i++) {
        if (buffer[i] < 0x10) Serial.print("0");
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void UHF_RFID::flushSerial() {
    while (_serial.available()) {
        _serial.read();
    }
}
