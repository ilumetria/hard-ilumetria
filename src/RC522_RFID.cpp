#include "RC522_RFID.h"

// ============================================================
//  Claves por defecto (FFFFFFFFFFFF)
// ============================================================
const MifareKey RC522_RFID::DEFAULT_KEY_A = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const MifareKey RC522_RFID::DEFAULT_KEY_B = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================
//  Constructor
// ============================================================
RC522_RFID::RC522_RFID(uint8_t ssPin, uint8_t resetPin, SPIClass& spiInstance)
    : _ssPin(ssPin), _resetPin(resetPin), _spi(spiInstance) {
    uid = {0, {0}, 0};
}

// ============================================================
//  begin()
// ============================================================
bool RC522_RFID::begin(uint32_t spiFreq) {
    pinMode(_ssPin, OUTPUT);
    digitalWrite(_ssPin, HIGH);

    pinMode(_resetPin, OUTPUT);

    // Secuencia de reset por hardware
    digitalWrite(_resetPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_resetPin, HIGH);
    delay(50);

    _spi.begin();

    reset();

    // Configurar timer: TAuto=1 → timer sólo en errores de transmisión
    writeRegister(RC522_REG_T_MODE,      0x80);
    writeRegister(RC522_REG_T_PRESCALER, 0xA9);
    writeRegister(RC522_REG_T_RELOAD_H,  0x03);
    writeRegister(RC522_REG_T_RELOAD_L,  0xE8);

    // 100% ASK
    writeRegister(RC522_REG_TX_ASK, 0x40);

    // CRC preset 0x6363 (ISO 14443-3 parte 6.2.4)
    writeRegister(RC522_REG_MODE, 0x3D);

    antennaOn();

    uint8_t v = getVersion();
    return (v == 0x91 || v == 0x92);  // Versiones válidas del RC522
}

// ============================================================
//  reset()
// ============================================================
void RC522_RFID::reset() {
    writeRegister(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    uint8_t count = 0;
    do {
        delay(50);
    } while ((readRegister(RC522_REG_COMMAND) & (1 << 4)) && (++count) < 3);
}

// ============================================================
//  Antena
// ============================================================
void RC522_RFID::antennaOn() {
    uint8_t val = readRegister(RC522_REG_TX_CONTROL);
    if ((val & 0x03) != 0x03) {
        writeRegister(RC522_REG_TX_CONTROL, val | 0x03);
    }
}

void RC522_RFID::antennaOff() {
    clearRegisterBitMask(RC522_REG_TX_CONTROL, 0x03);
}

// ============================================================
//  Versión del chip
// ============================================================
uint8_t RC522_RFID::getVersion() {
    return readRegister(RC522_REG_VERSION);
}

// ============================================================
//  Detección de nueva tarjeta
// ============================================================
bool RC522_RFID::isNewCardPresent() {
    uint8_t bufferATQA[2];
    uint8_t bufferSize = sizeof(bufferATQA);
    StatusCode result = requestA(bufferATQA, &bufferSize);
    return (result == STATUS_OK || result == STATUS_COLLISION);
}

bool RC522_RFID::readCardSerial() {
    StatusCode result = select(&uid);
    return (result == STATUS_OK);
}

// ============================================================
//  requestA / wakeupA / reqa_or_wupa
// ============================================================
StatusCode RC522_RFID::requestA(uint8_t* bufferATQA, uint8_t* bufferSize) {
    return reqa_or_wupa(PICC_CMD_REQA, bufferATQA, bufferSize);
}

StatusCode RC522_RFID::wakeupA(uint8_t* bufferATQA, uint8_t* bufferSize) {
    return reqa_or_wupa(PICC_CMD_WUPA, bufferATQA, bufferSize);
}

StatusCode RC522_RFID::reqa_or_wupa(uint8_t command, uint8_t* bufferATQA, uint8_t* bufferSize) {
    if (!bufferATQA || *bufferSize < 2) return STATUS_NO_ROOM;

    clearRegisterBitMask(RC522_REG_COLL, 0x80);  // ValuesAfterColl=0 → todos los bits en caso de colisión

    uint8_t validBits = 7;
    StatusCode status = transceiveData(&command, 1, bufferATQA, bufferSize, &validBits);
    if (status != STATUS_OK) return status;
    if (*bufferSize != 2 || validBits != 0) return STATUS_ERROR;
    return STATUS_OK;
}

// ============================================================
//  select() — Anti-colisión + selección
// ============================================================
StatusCode RC522_RFID::select(Uid* uid, uint8_t validBits) {
    bool uidComplete;
    bool selectDone;
    bool useCascadeTag;
    uint8_t cascadeLevel = 1;
    StatusCode result;
    uint8_t count;
    uint8_t checkBit;
    uint8_t index;
    uint8_t uidIndex;
    int8_t  currentLevelKnownBits;
    uint8_t buffer[9];
    uint8_t bufferUsed;
    uint8_t rxAlign;
    uint8_t txLastBits;
    uint8_t* responseBuffer;
    uint8_t responseLength;

    if (validBits > 80) return STATUS_INVALID;

    clearRegisterBitMask(RC522_REG_COLL, 0x80);

    uidComplete = false;
    while (!uidComplete) {
        switch (cascadeLevel) {
            case 1:
                buffer[0] = PICC_CMD_SEL_CL1;
                uidIndex   = 0;
                useCascadeTag = (validBits && uid->size > 4);
                break;
            case 2:
                buffer[0] = PICC_CMD_SEL_CL2;
                uidIndex   = 3;
                useCascadeTag = (validBits && uid->size > 7);
                break;
            case 3:
                buffer[0] = PICC_CMD_SEL_CL3;
                uidIndex   = 6;
                useCascadeTag = false;
                break;
            default:
                return STATUS_INTERNAL_ERROR;
        }

        currentLevelKnownBits = (int8_t)(validBits - (8 * uidIndex));
        if (currentLevelKnownBits < 0) currentLevelKnownBits = 0;

        index = 2;
        if (useCascadeTag) {
            buffer[index++] = PICC_CMD_CT;
        }
        uint8_t bytesToCopy = currentLevelKnownBits / 8 + (currentLevelKnownBits % 8 ? 1 : 0);
        if (bytesToCopy) {
            uint8_t maxBytes = useCascadeTag ? 3 : 4;
            if (bytesToCopy > maxBytes) bytesToCopy = maxBytes;
            for (count = 0; count < bytesToCopy; count++) {
                buffer[index++] = uid->uidByte[uidIndex + count];
            }
        }

        if (useCascadeTag) currentLevelKnownBits += 8;

        selectDone = false;
        while (!selectDone) {
            if (currentLevelKnownBits >= 32) {
                buffer[1]   = 0x70;
                buffer[6]   = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
                result = calculateCRC(buffer, 7, &buffer[7]);
                if (result != STATUS_OK) return result;
                txLastBits  = 0;
                bufferUsed  = 9;
                responseBuffer = &buffer[6];
                responseLength = 3;
            } else {
                txLastBits  = currentLevelKnownBits % 8;
                count        = currentLevelKnownBits / 8;
                index        = 2 + count;
                buffer[1]   = (index << 4) + txLastBits;
                bufferUsed  = index + (txLastBits ? 1 : 0);
                responseBuffer = &buffer[index];
                responseLength = sizeof(buffer) - index;
            }

            rxAlign = txLastBits;
            writeRegister(RC522_REG_BIT_FRAMING, (rxAlign << 4) + txLastBits);

            result = transceiveData(buffer, bufferUsed, responseBuffer, &responseLength, &txLastBits, rxAlign);
            if (result == STATUS_COLLISION) {
                uint8_t valueOfCollReg = readRegister(RC522_REG_COLL);
                if (valueOfCollReg & 0x20) return STATUS_COLLISION;
                uint8_t collisionPos = valueOfCollReg & 0x1F;
                if (collisionPos == 0) collisionPos = 32;
                if (collisionPos <= (uint8_t)currentLevelKnownBits) return STATUS_INTERNAL_ERROR;
                currentLevelKnownBits = (int8_t)collisionPos;
                count = currentLevelKnownBits % 8;
                checkBit = (currentLevelKnownBits - 1) % 8;
                index = 1 + (currentLevelKnownBits / 8) + (count ? 1 : 0);
                buffer[index] |= (1 << checkBit);
            } else if (result != STATUS_OK) {
                return result;
            } else {
                if (currentLevelKnownBits >= 32) {
                    selectDone = true;
                } else {
                    currentLevelKnownBits = 32;
                    index = 2;
                    if (useCascadeTag) buffer[index++] = PICC_CMD_CT;
                    uint8_t maxBytes = useCascadeTag ? 3 : 4;
                    for (count = 0; count < maxBytes; count++) {
                        buffer[index++] = uid->uidByte[uidIndex + count];
                    }
                }
            }
        }

        index = (buffer[2] == PICC_CMD_CT) ? 3 : 2;
        bytesToCopy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;
        for (count = 0; count < bytesToCopy; count++) {
            uid->uidByte[uidIndex + count] = buffer[index++];
        }

        if (responseLength != 3 || txLastBits != 0) return STATUS_ERROR;
        result = calculateCRC(responseBuffer, 1, &buffer[2]);
        if (result != STATUS_OK) return result;
        if ((buffer[2] != responseBuffer[1]) || (buffer[3] != responseBuffer[2])) return STATUS_CRC_WRONG;

        if (responseBuffer[0] & 0x04) {
            cascadeLevel++;
        } else {
            uidComplete  = true;
            uid->sak     = responseBuffer[0];
        }
    }

    uid->size = 3 * cascadeLevel + 1;
    return STATUS_OK;
}

// ============================================================
//  Autenticación MIFARE Classic
// ============================================================
StatusCode RC522_RFID::authenticate(uint8_t command, uint8_t blockAddr,
                                    MifareKey* key, Uid* uid) {
    uint8_t waitIRq = 0x10;  // IdleIRq

    uint8_t sendData[12];
    sendData[0] = command;
    sendData[1] = blockAddr;
    for (uint8_t i = 0; i < 6; i++) sendData[2 + i] = key->keyByte[i];
    for (uint8_t i = 0; i < 4; i++) sendData[8 + i] = uid->uidByte[i];

    return communicateWithPICC(RC522_CMD_MF_AUTHENT, waitIRq, sendData, sizeof(sendData));
}

void RC522_RFID::stopCrypto1() {
    clearRegisterBitMask(RC522_REG_STATUS2, 0x08);  // Crypto1On = 0
}

// ============================================================
//  Lectura de bloque MIFARE Classic
// ============================================================
StatusCode RC522_RFID::mifareRead(uint8_t blockAddr, uint8_t* buffer, uint8_t* bufferSize) {
    if (!buffer || *bufferSize < 18) return STATUS_NO_ROOM;

    buffer[0] = PICC_CMD_MF_READ;
    buffer[1] = blockAddr;
    StatusCode result = calculateCRC(buffer, 2, &buffer[2]);
    if (result != STATUS_OK) return result;

    return transceiveData(buffer, 4, buffer, bufferSize, nullptr, 0, true);
}

// ============================================================
//  Escritura de bloque MIFARE Classic
// ============================================================
StatusCode RC522_RFID::mifareWrite(uint8_t blockAddr, uint8_t* buffer, uint8_t bufferSize) {
    if (bufferSize < 16) return STATUS_INVALID;

    uint8_t cmdBuffer[4];
    cmdBuffer[0] = PICC_CMD_MF_WRITE;
    cmdBuffer[1] = blockAddr;
    StatusCode result = calculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (result != STATUS_OK) return result;

    uint8_t waitIRq   = 0x10;
    uint8_t cmdBufferSize = 4;
    result = transceiveData(cmdBuffer, 4, cmdBuffer, &cmdBufferSize);
    if (result != STATUS_OK) return result;
    if (cmdBufferSize != 1 || (cmdBuffer[0] & 0x0F) != 0x0A) return STATUS_MIFARE_NACK;

    uint8_t dataBuffer[18];
    memcpy(dataBuffer, buffer, 16);
    result = calculateCRC(dataBuffer, 16, &dataBuffer[16]);
    if (result != STATUS_OK) return result;

    cmdBufferSize = 1;
    return transceiveData(dataBuffer, 18, cmdBuffer, &cmdBufferSize);
}

// ============================================================
//  writeUserId() — Escribe un String como ID de usuario
// ============================================================
StatusCode RC522_RFID::writeUserId(uint8_t blockAddr, const String& userId) {
    uint8_t dataBlock[16];
    memset(dataBlock, 0x00, 16);
    uint8_t len = min((size_t)userId.length(), (size_t)16);
    memcpy(dataBlock, userId.c_str(), len);
    return mifareWrite(blockAddr, dataBlock, 16);
}

// ============================================================
//  readUserId() — Lee un String como ID de usuario
// ============================================================
StatusCode RC522_RFID::readUserId(uint8_t blockAddr, String& userId) {
    uint8_t buffer[18];
    uint8_t size = sizeof(buffer);
    StatusCode status = mifareRead(blockAddr, buffer, &size);
    if (status != STATUS_OK) return status;

    // Convertir bytes a String, ignorando bytes nulos al final
    userId = "";
    for (uint8_t i = 0; i < 16; i++) {
        if (buffer[i] == 0x00) break;
        userId += (char)buffer[i];
    }
    return STATUS_OK;
}

// ============================================================
//  haltA()
// ============================================================
StatusCode RC522_RFID::haltA() {
    uint8_t buffer[4];
    buffer[0] = PICC_CMD_HLTA;
    buffer[1] = 0;
    StatusCode result = calculateCRC(buffer, 2, &buffer[2]);
    if (result != STATUS_OK) return result;
    result = transceiveData(buffer, sizeof(buffer), nullptr, nullptr);
    return (result == STATUS_TIMEOUT) ? STATUS_OK : STATUS_ERROR;
}

// ============================================================
//  Tipo de tarjeta PICC
// ============================================================
PICC_Type RC522_RFID::getPiccType(uint8_t sak) {
    if (sak & 0x04) return PICC_TYPE_NOT_COMPLETE;
    switch (sak) {
        case 0x09: return PICC_TYPE_MIFARE_MINI;
        case 0x08: return PICC_TYPE_MIFARE_1K;
        case 0x18: return PICC_TYPE_MIFARE_4K;
        case 0x00: return PICC_TYPE_MIFARE_UL;
        case 0x10: return PICC_TYPE_MIFARE_PLUS;
        case 0x11: return PICC_TYPE_MIFARE_PLUS;
        case 0x01: return PICC_TYPE_TNP3XXX;
        case 0x20: return PICC_TYPE_ISO_14443_4;
        case 0x40: return PICC_TYPE_ISO_18092;
        default:   return PICC_TYPE_UNKNOWN;
    }
}

const char* RC522_RFID::getPiccTypeName(PICC_Type type) {
    switch (type) {
        case PICC_TYPE_ISO_14443_4:  return "PICC compliant with ISO/IEC 14443-4";
        case PICC_TYPE_ISO_18092:    return "PICC compliant with ISO/IEC 18092 (NFC)";
        case PICC_TYPE_MIFARE_MINI:  return "MIFARE Mini, 320 bytes";
        case PICC_TYPE_MIFARE_1K:    return "MIFARE 1KB";
        case PICC_TYPE_MIFARE_4K:    return "MIFARE 4KB";
        case PICC_TYPE_MIFARE_UL:    return "MIFARE Ultralight or Ultralight C";
        case PICC_TYPE_MIFARE_PLUS:  return "MIFARE Plus";
        case PICC_TYPE_TNP3XXX:      return "MIFARE TNP3XXX";
        case PICC_TYPE_NOT_COMPLETE: return "SAK indicates UID is not complete";
        default:                     return "Unknown type";
    }
}

// ============================================================
//  SPI — Escritura de registro
// ============================================================
void RC522_RFID::writeRegister(uint8_t reg, uint8_t value) {
    _spi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi.transfer((reg << 1) & 0x7E);
    _spi.transfer(value);
    digitalWrite(_ssPin, HIGH);
    _spi.endTransaction();
}

void RC522_RFID::writeRegister(uint8_t reg, uint8_t count, uint8_t* values) {
    _spi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi.transfer((reg << 1) & 0x7E);
    for (uint8_t i = 0; i < count; i++) _spi.transfer(values[i]);
    digitalWrite(_ssPin, HIGH);
    _spi.endTransaction();
}

// ============================================================
//  SPI — Lectura de registro
// ============================================================
uint8_t RC522_RFID::readRegister(uint8_t reg) {
    _spi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi.transfer(((reg << 1) & 0x7E) | 0x80);
    uint8_t val = _spi.transfer(0x00);
    digitalWrite(_ssPin, HIGH);
    _spi.endTransaction();
    return val;
}

void RC522_RFID::readRegister(uint8_t reg, uint8_t count, uint8_t* values, uint8_t rxAlign) {
    if (!count) return;
    uint8_t address = ((reg << 1) & 0x7E) | 0x80;
    _spi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_ssPin, LOW);
    _spi.transfer(address);
    uint8_t index = 0;
    if (rxAlign) {
        uint8_t mask = (0xFF << rxAlign) & 0xFF;
        uint8_t value = _spi.transfer(address);
        values[0] = (values[0] & ~mask) | (value & mask);
        index = 1;
    }
    while (index < count - 1) {
        values[index++] = _spi.transfer(address);
    }
    values[index] = _spi.transfer(0x00);
    digitalWrite(_ssPin, HIGH);
    _spi.endTransaction();
}

// ============================================================
//  Máscaras de bits
// ============================================================
void RC522_RFID::setRegisterBitMask(uint8_t reg, uint8_t mask) {
    writeRegister(reg, readRegister(reg) | mask);
}

void RC522_RFID::clearRegisterBitMask(uint8_t reg, uint8_t mask) {
    writeRegister(reg, readRegister(reg) & (~mask));
}

// ============================================================
//  Cálculo CRC
// ============================================================
StatusCode RC522_RFID::calculateCRC(uint8_t* data, uint8_t length, uint8_t* result) {
    writeRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    clearRegisterBitMask(RC522_REG_DIV_IRQ, 0x04);
    setRegisterBitMask(RC522_REG_FIFO_LEVEL, 0x80);
    writeRegister(RC522_REG_FIFO_DATA, length, data);
    writeRegister(RC522_REG_COMMAND, RC522_CMD_CALC_CRC);

    uint16_t i = 5000;
    uint8_t n;
    do {
        n = readRegister(RC522_REG_DIV_IRQ);
        if (--i == 0) return STATUS_TIMEOUT;
    } while (!(n & 0x04));

    writeRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    result[0] = readRegister(RC522_REG_CRC_RESULT_L);
    result[1] = readRegister(RC522_REG_CRC_RESULT_H);
    return STATUS_OK;
}

// ============================================================
//  communicateWithPICC()
// ============================================================
StatusCode RC522_RFID::communicateWithPICC(uint8_t command, uint8_t waitIRq,
                                            uint8_t* sendData, uint8_t sendLen,
                                            uint8_t* backData, uint8_t* backLen,
                                            uint8_t* validBits, uint8_t rxAlign,
                                            bool checkCRC) {
    uint8_t txLastBits  = validBits ? *validBits : 0;
    uint8_t bitFraming  = (rxAlign << 4) + txLastBits;

    writeRegister(RC522_REG_COMMAND,     RC522_CMD_IDLE);
    clearRegisterBitMask(RC522_REG_COM_IRQ, 0x80);
    setRegisterBitMask(RC522_REG_FIFO_LEVEL, 0x80);
    writeRegister(RC522_REG_FIFO_DATA,  sendLen, sendData);
    writeRegister(RC522_REG_BIT_FRAMING, bitFraming);
    writeRegister(RC522_REG_COMMAND,    command);

    if (command == RC522_CMD_TRANSCEIVE) {
        setRegisterBitMask(RC522_REG_BIT_FRAMING, 0x80);
    }

    uint16_t i = 2000;
    uint8_t n;
    do {
        n = readRegister(RC522_REG_COM_IRQ);
        if (--i == 0) return STATUS_TIMEOUT;
    } while (!(n & 0x01) && !(n & waitIRq));

    clearRegisterBitMask(RC522_REG_BIT_FRAMING, 0x80);

    if (!i) return STATUS_TIMEOUT;
    if (readRegister(RC522_REG_ERROR) & 0x13) return STATUS_ERROR;

    StatusCode status = STATUS_OK;
    if (n & waitIRq & 0x01) status = STATUS_TIMEOUT;

    if (command == RC522_CMD_TRANSCEIVE && backData && backLen) {
        uint8_t fifoLevel = readRegister(RC522_REG_FIFO_LEVEL);
        uint8_t lastBits  = readRegister(RC522_REG_CONTROL) & 0x07;
        if (validBits) *validBits = lastBits;
        if (fifoLevel > *backLen) return STATUS_NO_ROOM;
        *backLen = fifoLevel;
        readRegister(RC522_REG_FIFO_DATA, fifoLevel, backData, rxAlign);
    }

    if (readRegister(RC522_REG_ERROR) & 0x08) return STATUS_COLLISION;

    if (backData && backLen && checkCRC) {
        if (*backLen == 1 && (readRegister(RC522_REG_CONTROL) & 0x07) == 4) {
            return STATUS_MIFARE_NACK;
        }
        if (*backLen < 2 || (readRegister(RC522_REG_CONTROL) & 0x07) != 0) {
            return STATUS_CRC_WRONG;
        }
        uint8_t controlBuffer[2];
        StatusCode crcStatus = calculateCRC(backData, *backLen - 2, controlBuffer);
        if (crcStatus != STATUS_OK) return crcStatus;
        if ((backData[*backLen - 2] != controlBuffer[0]) ||
            (backData[*backLen - 1] != controlBuffer[1])) {
            return STATUS_CRC_WRONG;
        }
    }

    return status;
}

// ============================================================
//  transceiveData()
// ============================================================
StatusCode RC522_RFID::transceiveData(uint8_t* sendData, uint8_t sendLen,
                                       uint8_t* backData, uint8_t* backLen,
                                       uint8_t* validBits, uint8_t rxAlign,
                                       bool checkCRC) {
    uint8_t waitIRq = 0x30;  // RxIRq + IdleIRq
    return communicateWithPICC(RC522_CMD_TRANSCEIVE, waitIRq,
                               sendData, sendLen, backData, backLen,
                               validBits, rxAlign, checkCRC);
}
