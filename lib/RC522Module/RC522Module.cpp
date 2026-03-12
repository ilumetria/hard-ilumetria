/**
 * @file RC522Module.cpp
 * @brief Implementación del módulo RC522 para ESP32-S3 CAM
 * @author Adrian Nava
 * @date 2026-03-08
 */

#include "RC522Module.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

RC522Module::RC522Module(uint8_t ssPin, uint8_t rstPin)
    : _ssPin(ssPin), _rstPin(rstPin), _initialized(false), 
      _mfrc522(nullptr), _spi(nullptr) {
}

RC522Module::~RC522Module() {
    if (_mfrc522) {
        delete _mfrc522;
        _mfrc522 = nullptr;
    }
    if (_spi) {
        _spi->end();
        delete _spi;
        _spi = nullptr;
    }
}

// ============================================================================
// Inicialización y estado
// ============================================================================

bool RC522Module::begin() {
    Serial.println(F("╔══════════════════════════════════════════╗"));
    Serial.println(F("║    RC522 RFID/NFC Module - ESP32-S3     ║"));
    Serial.println(F("║           Inicializando...               ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));
    
    // Inicializar SPI (global) con pines personalizados para ESP32-S3
    SPI.begin(RC522_SCK_PIN, RC522_MISO_PIN, RC522_MOSI_PIN, _ssPin);
    
    // Crear instancia MFRC522
    _mfrc522 = new MFRC522(_ssPin, _rstPin);
    
    // Inicializar MFRC522
    _mfrc522->PCD_Init(_ssPin, _rstPin);
    delay(100);  // Esperar estabilización
    
    // Verificar conexión
    if (!isConnected()) {
        Serial.println(F("[ERROR] No se detectó el módulo RC522."));
        Serial.println(F("        Verifica las conexiones SPI:"));
        Serial.printf( "        SDA(SS)=%d SCK=%d MOSI=%d MISO=%d RST=%d\n",
                        _ssPin, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN, _rstPin);
        _initialized = false;
        return false;
    }
    
    // Configurar ganancia máxima de antena para mejor lectura
    _mfrc522->PCD_SetAntennaGain(MFRC522::RxGain_max);
    
    _initialized = true;
    
    Serial.println(F("[OK] Módulo RC522 inicializado correctamente"));
    Serial.print(F("[INFO] Firmware: "));
    Serial.println(getFirmwareVersion());
    Serial.printf("[INFO] Pines -> SDA:%d SCK:%d MOSI:%d MISO:%d RST:%d\n",
                  _ssPin, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN, _rstPin);
    Serial.println(F("─────────────────────────────────────────────"));
    
    return true;
}

bool RC522Module::isConnected() {
    if (!_mfrc522) return false;
    byte version = _mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    // Versiones conocidas: 0x91 (v1.0), 0x92 (v2.0), 0x88 (clone)
    return (version == 0x91 || version == 0x92 || version == 0x88 || 
            version == 0x12 || version == 0x82);
}

String RC522Module::getFirmwareVersion() {
    if (!_mfrc522) return "N/A";
    byte version = _mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    switch (version) {
        case 0x88: return "Clone v0.0";
        case 0x90: return "MFRC522 v0.0";
        case 0x91: return "MFRC522 v1.0";
        case 0x92: return "MFRC522 v2.0";
        case 0x12: return "Counterfeit chip";
        case 0x82: return "FM17522 (compatible)";
        default:   return "Desconocido (0x" + String(version, HEX) + ")";
    }
}

void RC522Module::setAntennaGain(MFRC522::PCD_RxGain gain) {
    if (_mfrc522) {
        _mfrc522->PCD_SetAntennaGain(gain);
    }
}

bool RC522Module::selfTest() {
    if (!_mfrc522) return false;
    return _mfrc522->PCD_PerformSelfTest();
}

// ============================================================================
// Detección de tarjetas
// ============================================================================

bool RC522Module::isNewCardPresent() {
    if (!_initialized || !_mfrc522) return false;
    
    // Reset del estado del loop de comunicación
    _mfrc522->PCD_Init();
    delay(4);
    
    return _mfrc522->PICC_IsNewCardPresent() && _mfrc522->PICC_ReadCardSerial();
}

CardInfo RC522Module::readCardInfo() {
    CardInfo info;
    
    if (!_mfrc522 || !_initialized) {
        info.detected = false;
        return info;
    }
    
    info.detected = true;
    
    // Copiar UID
    info.uidLength = _mfrc522->uid.size;
    memcpy(info.uid, _mfrc522->uid.uidByte, info.uidLength);
    info.uidString = bytesToHexString(info.uid, info.uidLength);
    
    // SAK
    info.sak = _mfrc522->uid.sak;
    
    // Tipo de tarjeta
    MFRC522::PICC_Type piccType = _mfrc522->PICC_GetType(info.sak);
    info.typeName = getCardTypeName(piccType);
    
    // Clasificar tipo
    switch (piccType) {
        case MFRC522::PICC_TYPE_MIFARE_MINI:
            info.type = CARD_MIFARE_MINI;
            break;
        case MFRC522::PICC_TYPE_MIFARE_1K:
            info.type = CARD_MIFARE_1K;
            break;
        case MFRC522::PICC_TYPE_MIFARE_4K:
            info.type = CARD_MIFARE_4K;
            break;
        case MFRC522::PICC_TYPE_MIFARE_UL:
            info.type = CARD_MIFARE_ULTRALIGHT;
            break;
        default:
            info.type = CARD_UNKNOWN;
            break;
    }
    
    return info;
}

CardInfo RC522Module::waitForCard(unsigned long timeoutMs) {
    CardInfo info;
    unsigned long startTime = millis();
    
    Serial.println(F("[ESPERA] Acerque una tarjeta/tag al lector..."));
    
    while (true) {
        if (isNewCardPresent()) {
            info = readCardInfo();
            if (info.detected) {
                Serial.println(F("[OK] ¡Tarjeta detectada!"));
                return info;
            }
        }
        
        // Verificar timeout
        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs) {
            Serial.println(F("[TIMEOUT] No se detectó ninguna tarjeta"));
            info.detected = false;
            return info;
        }
        
        delay(100);  // Polling cada 100ms
    }
}

CardType RC522Module::identifyCardType() {
    if (!_mfrc522) return CARD_UNKNOWN;
    
    MFRC522::PICC_Type piccType = _mfrc522->PICC_GetType(_mfrc522->uid.sak);
    
    switch (piccType) {
        case MFRC522::PICC_TYPE_MIFARE_MINI: return CARD_MIFARE_MINI;
        case MFRC522::PICC_TYPE_MIFARE_1K:   return CARD_MIFARE_1K;
        case MFRC522::PICC_TYPE_MIFARE_4K:   return CARD_MIFARE_4K;
        case MFRC522::PICC_TYPE_MIFARE_UL:   return CARD_MIFARE_ULTRALIGHT;
        default:                              return CARD_UNKNOWN;
    }
}

// ============================================================================
// Lectura - MIFARE Classic
// ============================================================================

RC522Result RC522Module::readBlock(byte blockAddr, const byte* key, 
                                    MFRC522::MIFARE_Key* mifareKey) {
    RC522Result result;
    
    if (!_initialized) {
        result.message = "Módulo no inicializado";
        return result;
    }
    
    // Autenticar bloque
    if (!authenticateBlock(blockAddr, key)) {
        result.message = "Error de autenticación en bloque " + String(blockAddr);
        return result;
    }
    
    // Leer bloque
    byte buffer[18];  // 16 bytes datos + 2 bytes CRC
    byte bufferSize = sizeof(buffer);
    
    MFRC522::StatusCode status = _mfrc522->MIFARE_Read(blockAddr, buffer, &bufferSize);
    
    if (status != MFRC522::STATUS_OK) {
        result.message = "Error al leer bloque " + String(blockAddr) + 
                         ": " + String(_mfrc522->GetStatusCodeName(status));
        return result;
    }
    
    // Copiar datos al resultado
    memcpy(result.data, buffer, MIFARE_CLASSIC_BLOCK_SIZE);
    result.dataLength = MIFARE_CLASSIC_BLOCK_SIZE;
    result.success = true;
    result.message = "Bloque " + String(blockAddr) + " leído correctamente";
    
    return result;
}

bool RC522Module::readSector(byte sectorNum, byte sectorData[][MIFARE_CLASSIC_BLOCK_SIZE], 
                              const byte* key) {
    byte firstBlock = sectorNum * 4;
    
    for (byte i = 0; i < 4; i++) {
        RC522Result blockResult = readBlock(firstBlock + i, key);
        if (!blockResult.success) {
            Serial.printf("[ERROR] No se pudo leer bloque %d: %s\n", 
                          firstBlock + i, blockResult.message.c_str());
            return false;
        }
        memcpy(sectorData[i], blockResult.data, MIFARE_CLASSIC_BLOCK_SIZE);
    }
    
    return true;
}

String RC522Module::readText(byte startBlock, byte numBlocks, const byte* key) {
    String text = "";
    
    for (byte i = 0; i < numBlocks; i++) {
        byte blockAddr = startBlock + i;
        
        // Saltar sector trailers
        if (isSectorTrailer(blockAddr)) {
            Serial.printf("[AVISO] Saltando sector trailer en bloque %d\n", blockAddr);
            continue;
        }
        
        RC522Result result = readBlock(blockAddr, key);
        if (!result.success) {
            Serial.printf("[ERROR] Lectura fallida en bloque %d\n", blockAddr);
            break;
        }
        
        // Convertir bytes a caracteres, deteniéndose en null o padding
        for (byte j = 0; j < MIFARE_CLASSIC_BLOCK_SIZE; j++) {
            if (result.data[j] == 0x00) {
                return text;  // Fin del texto
            }
            if (result.data[j] >= 0x20 && result.data[j] <= 0x7E) {
                text += (char)result.data[j];
            }
        }
    }
    
    return text;
}

// ============================================================================
// Escritura - MIFARE Classic
// ============================================================================

RC522Result RC522Module::writeBlock(byte blockAddr, const byte* data, byte dataLen,
                                     const byte* key) {
    RC522Result result;
    
    if (!_initialized) {
        result.message = "Módulo no inicializado";
        return result;
    }
    
    // Protección: no permitir escritura en sector trailers accidentalmente
    if (isSectorTrailer(blockAddr)) {
        result.message = "¡ADVERTENCIA! El bloque " + String(blockAddr) + 
                         " es un Sector Trailer. Escritura bloqueada por seguridad.";
        Serial.println("[SEGURIDAD] " + result.message);
        return result;
    }
    
    // Protección: no escribir en bloque 0 (manufacturer block)
    if (blockAddr == 0) {
        result.message = "¡ADVERTENCIA! El bloque 0 contiene datos del fabricante. Escritura bloqueada.";
        Serial.println("[SEGURIDAD] " + result.message);
        return result;
    }
    
    // Autenticar
    if (!authenticateBlock(blockAddr, key)) {
        result.message = "Error de autenticación en bloque " + String(blockAddr);
        return result;
    }
    
    // Preparar buffer de 16 bytes (rellenar con ceros si es necesario)
    byte writeBuffer[MIFARE_CLASSIC_BLOCK_SIZE];
    memset(writeBuffer, 0x00, sizeof(writeBuffer));
    
    byte copyLen = (dataLen > MIFARE_CLASSIC_BLOCK_SIZE) ? MIFARE_CLASSIC_BLOCK_SIZE : dataLen;
    memcpy(writeBuffer, data, copyLen);
    
    // Escribir
    MFRC522::StatusCode status = _mfrc522->MIFARE_Write(blockAddr, writeBuffer, 
                                                         MIFARE_CLASSIC_BLOCK_SIZE);
    
    if (status != MFRC522::STATUS_OK) {
        result.message = "Error de escritura en bloque " + String(blockAddr) + 
                         ": " + String(_mfrc522->GetStatusCodeName(status));
        return result;
    }
    
    result.success = true;
    result.message = "Bloque " + String(blockAddr) + " escrito correctamente (" + 
                     String(copyLen) + " bytes)";
    memcpy(result.data, writeBuffer, MIFARE_CLASSIC_BLOCK_SIZE);
    result.dataLength = MIFARE_CLASSIC_BLOCK_SIZE;
    
    return result;
}

RC522Result RC522Module::writeText(byte startBlock, const String& text, const byte* key) {
    RC522Result result;
    
    if (text.length() == 0) {
        result.message = "Texto vacío";
        return result;
    }
    
    const char* textData = text.c_str();
    int textLen = text.length();
    int bytesWritten = 0;
    byte currentBlock = startBlock;
    
    Serial.printf("[WRITE] Escribiendo '%s' (%d bytes) desde bloque %d\n", 
                  textData, textLen, startBlock);
    
    while (bytesWritten < textLen) {
        // Saltar sector trailers
        if (isSectorTrailer(currentBlock)) {
            currentBlock++;
            continue;
        }
        
        // Saltar bloque 0
        if (currentBlock == 0) {
            currentBlock++;
            continue;
        }
        
        // Preparar datos del bloque actual
        byte blockData[MIFARE_CLASSIC_BLOCK_SIZE];
        memset(blockData, 0x00, sizeof(blockData));
        
        int remainingBytes = textLen - bytesWritten;
        int bytesToCopy = (remainingBytes > MIFARE_CLASSIC_BLOCK_SIZE) ? 
                          MIFARE_CLASSIC_BLOCK_SIZE : remainingBytes;
        
        memcpy(blockData, textData + bytesWritten, bytesToCopy);
        
        // Necesitamos re-detectar la tarjeta si cambiamos de sector
        // y re-autenticar
        result = writeBlock(currentBlock, blockData, MIFARE_CLASSIC_BLOCK_SIZE, key);
        
        if (!result.success) {
            result.message = "Error al escribir texto en bloque " + String(currentBlock) + 
                             ": " + result.message;
            return result;
        }
        
        bytesWritten += bytesToCopy;
        currentBlock++;
    }
    
    result.success = true;
    result.message = "Texto escrito correctamente (" + String(bytesWritten) + 
                     " bytes en " + String(currentBlock - startBlock) + " bloques)";
    
    return result;
}

// ============================================================================
// Lectura/Escritura - MIFARE Ultralight / NTAG
// ============================================================================

RC522Result RC522Module::readUltralightPage(byte pageAddr) {
    RC522Result result;
    
    if (!_initialized) {
        result.message = "Módulo no inicializado";
        return result;
    }
    
    byte buffer[18];
    byte bufferSize = sizeof(buffer);
    
    MFRC522::StatusCode status = _mfrc522->MIFARE_Read(pageAddr, buffer, &bufferSize);
    
    if (status != MFRC522::STATUS_OK) {
        result.message = "Error al leer página " + String(pageAddr) + 
                         ": " + String(_mfrc522->GetStatusCodeName(status));
        return result;
    }
    
    // Solo copiar los 4 bytes de la página
    memcpy(result.data, buffer, MIFARE_UL_PAGE_SIZE);
    result.dataLength = MIFARE_UL_PAGE_SIZE;
    result.success = true;
    result.message = "Página " + String(pageAddr) + " leída correctamente";
    
    return result;
}

RC522Result RC522Module::writeUltralightPage(byte pageAddr, const byte* data) {
    RC522Result result;
    
    if (!_initialized) {
        result.message = "Módulo no inicializado";
        return result;
    }
    
    // Protección: páginas 0-3 son de solo lectura en Ultralight
    if (pageAddr < 4) {
        result.message = "Las páginas 0-3 son de solo lectura en MIFARE Ultralight";
        return result;
    }
    
    MFRC522::StatusCode status = _mfrc522->MIFARE_Ultralight_Write(pageAddr, 
                                  const_cast<byte*>(data), MIFARE_UL_PAGE_SIZE);
    
    if (status != MFRC522::STATUS_OK) {
        result.message = "Error al escribir página " + String(pageAddr) + 
                         ": " + String(_mfrc522->GetStatusCodeName(status));
        return result;
    }
    
    result.success = true;
    result.message = "Página " + String(pageAddr) + " escrita correctamente";
    memcpy(result.data, data, MIFARE_UL_PAGE_SIZE);
    result.dataLength = MIFARE_UL_PAGE_SIZE;
    
    return result;
}

// ============================================================================
// Utilidades
// ============================================================================

String RC522Module::bytesToHexString(const byte* buffer, byte bufferSize) {
    String hexStr = "";
    for (byte i = 0; i < bufferSize; i++) {
        if (buffer[i] < 0x10) hexStr += "0";
        hexStr += String(buffer[i], HEX);
        if (i < bufferSize - 1) hexStr += ":";
    }
    hexStr.toUpperCase();
    return hexStr;
}

void RC522Module::printBlockData(byte blockAddr, const byte* data, byte dataLen) {
    Serial.printf("  Bloque %02d: ", blockAddr);
    
    // Hex
    for (byte i = 0; i < dataLen; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    
    // ASCII
    Serial.print(" | ");
    for (byte i = 0; i < dataLen; i++) {
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            Serial.print((char)data[i]);
        } else {
            Serial.print(".");
        }
    }
    Serial.println();
}

void RC522Module::dumpCardToSerial() {
    if (!_mfrc522 || !_initialized) {
        Serial.println(F("[ERROR] Módulo no inicializado"));
        return;
    }
    
    CardInfo info = readCardInfo();
    
    Serial.println(F("\n╔══════════════════════════════════════════════╗"));
    Serial.println(F("║          DUMP COMPLETO DE TARJETA            ║"));
    Serial.println(F("╠══════════════════════════════════════════════╣"));
    Serial.printf( "║ UID:    %s\n", info.uidString.c_str());
    Serial.printf( "║ Tipo:   %s\n", info.typeName.c_str());
    Serial.printf( "║ SAK:    0x%02X\n", info.sak);
    Serial.println(F("╚══════════════════════════════════════════════╝\n"));
    
    if (info.type == CARD_MIFARE_1K || info.type == CARD_MIFARE_4K) {
        byte numSectors = (info.type == CARD_MIFARE_1K) ? 16 : 40;
        
        for (byte sector = 0; sector < numSectors; sector++) {
            byte firstBlock = sector * 4;
            Serial.printf("─── Sector %02d ───────────────────────────────\n", sector);
            
            for (byte block = 0; block < 4; block++) {
                RC522Result result = readBlock(firstBlock + block);
                if (result.success) {
                    printBlockData(firstBlock + block, result.data, result.dataLength);
                } else {
                    Serial.printf("  Bloque %02d: [Error: %s]\n", 
                                  firstBlock + block, result.message.c_str());
                }
            }
        }
    } else if (info.type == CARD_MIFARE_ULTRALIGHT) {
        Serial.println(F("─── Páginas MIFARE Ultralight ──────────────"));
        for (byte page = 0; page < 16; page++) {
            RC522Result result = readUltralightPage(page);
            if (result.success) {
                Serial.printf("  Página %02d: ", page);
                for (byte i = 0; i < result.dataLength; i++) {
                    if (result.data[i] < 0x10) Serial.print("0");
                    Serial.print(result.data[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            }
        }
    }
    
    Serial.println(F("\n════════════════════════════════════════════════"));
}

bool RC522Module::isSectorTrailer(byte blockAddr) {
    // En MIFARE Classic 1K: bloques 3, 7, 11, 15, ..., 63
    return ((blockAddr + 1) % 4 == 0);
}

byte RC522Module::getFirstDataBlock(byte sectorNum) {
    // El primer bloque de datos de un sector
    // Sector 0: bloque 1 (bloque 0 es manufacturer)
    // Sector N: bloque N*4
    if (sectorNum == 0) return 1;
    return sectorNum * 4;
}

void RC522Module::haltCard() {
    if (_mfrc522) {
        _mfrc522->PICC_HaltA();
        _mfrc522->PCD_StopCrypto1();
    }
}

MFRC522& RC522Module::getRawReader() {
    return *_mfrc522;
}

// ============================================================================
// Métodos privados
// ============================================================================

bool RC522Module::authenticateBlock(byte blockAddr, const byte* key) {
    MFRC522::MIFARE_Key mifareKey = toMifareKey(key);
    
    MFRC522::StatusCode status = _mfrc522->PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A,
        blockAddr,
        &mifareKey,
        &(_mfrc522->uid)
    );
    
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[AUTH] Falló autenticación bloque %d: %s\n", 
                      blockAddr, _mfrc522->GetStatusCodeName(status));
        return false;
    }
    
    return true;
}

MFRC522::MIFARE_Key RC522Module::toMifareKey(const byte* key) {
    MFRC522::MIFARE_Key mifareKey;
    memcpy(mifareKey.keyByte, key, MIFARE_CLASSIC_KEY_SIZE);
    return mifareKey;
}

String RC522Module::getCardTypeName(MFRC522::PICC_Type piccType) {
    switch (piccType) {
        case MFRC522::PICC_TYPE_MIFARE_MINI:  return "MIFARE Mini (320 bytes)";
        case MFRC522::PICC_TYPE_MIFARE_1K:    return "MIFARE Classic 1K";
        case MFRC522::PICC_TYPE_MIFARE_4K:    return "MIFARE Classic 4K";
        case MFRC522::PICC_TYPE_MIFARE_UL:    return "MIFARE Ultralight";
        case MFRC522::PICC_TYPE_MIFARE_PLUS:  return "MIFARE Plus";
        case MFRC522::PICC_TYPE_TNP3XXX:      return "MIFARE TNP3XXX";
        case MFRC522::PICC_TYPE_ISO_14443_4:  return "ISO/IEC 14443-4";
        case MFRC522::PICC_TYPE_ISO_18092:    return "ISO/IEC 18092 (NFC)";
        default:                               return "Tipo desconocido";
    }
}
