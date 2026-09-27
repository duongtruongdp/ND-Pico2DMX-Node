#include <Arduino.h>
#include <driver/spi_slave.h>

// V3.3S finite forensic ESP32-C5 SPI slave diagnostic.
// No Wi-Fi, HTTP, NVS, UART, ND1, Base64, or production subsystem is used.

static constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
static constexpr int C5_SCK = 25, C5_MOSI = 26, C5_MISO = 8, C5_CS = 9, C5_READY = 3;
static constexpr uint16_t MAGIC = 0x4E44;
static constexpr uint8_t VERSION = 1, TYPE_HELLO = 1, TYPE_PONG = 0x81;
static constexpr size_t HEADER_BYTES = 16, MAX_PAYLOAD = 513, FRAME_BYTES = 544, CAPTURE_BYTES = 32;

enum class State : uint8_t { BOOT, ARM_REQUEST, REQUEST_RECEIVED, VALIDATE_REQUEST, PREPARE_RESPONSE, RESPONSE_READY, WAIT_RESPONSE_CLOCK, RESPONSE_CLOCKED, PASS, FIRST_FAILURE, HALTED };
alignas(4) uint8_t rxFrame[FRAME_BYTES], txFrame[FRAME_BYTES], firstActualRx[CAPTURE_BYTES], firstExpectedRx[CAPTURE_BYTES], firstPreparedTx[CAPTURE_BYTES];
spi_slave_transaction_t transaction;
State state = State::BOOT;
bool halted = false, finalPrinted = false, firstCaptured = false, haveRxSequence = false;
uint32_t expectedSequence = 1, transactionNumber = 0, requestTransactions = 0, responseTransactions = 0, actualRxBytes = 0, actualTxBytes = 0, validRequests = 0, successfulCycles = 0, phaseStartMs = 0;
uint32_t magicErrors = 0, versionErrors = 0, typeErrors = 0, lengthErrors = 0, crcErrors = 0, payloadErrors = 0, sequenceErrors = 0, transactionLengthErrors = 0, spiErrors = 0, queueErrors = 0, readyAssertCount = 0, readyDeassertCount = 0;
uint32_t lastRxSequence = 0, lastTxSequence = 0, firstRxTransLenBits = 0, firstRxTransLenBytes = 0, firstResponseTransLenBits = 0, firstResponseTransLenBytes = 0;
uint8_t readyAfterAssert = LOW;
const char *failureReason = "NONE";

uint32_t crc32(const uint8_t *data, size_t length) { uint32_t crc = 0xFFFFFFFFu; for (size_t i = 0; i < length; ++i) { crc ^= data[i]; for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u); } return crc ^ 0xFFFFFFFFu; }
void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8); }
uint32_t get32(const uint8_t *p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint8_t expectedPayload(uint32_t seq, size_t i) { return static_cast<uint8_t>((seq * 17u + i * 29u + 0x5Au) & 0xFFu); }
void buildExpectedRequest(uint8_t *frame, uint32_t seq) { memset(frame, 0, FRAME_BYTES); put16(frame, MAGIC); frame[2] = VERSION; frame[3] = TYPE_HELLO; put32(frame + 4, seq); put16(frame + 8, 32); put16(frame + 10, 0); put32(frame + 12, 0); for (size_t i = 0; i < 32; ++i) frame[HEADER_BYTES + i] = expectedPayload(seq, i); put32(frame + HEADER_BYTES + 32, crc32(frame, HEADER_BYTES + 32)); }
void printHex(const char *label, const uint8_t *data, size_t n) { Serial.print(label); for (size_t i = 0; i < n; ++i) { Serial.print(i ? " " : ""); if (data[i] < 16) Serial.print('0'); Serial.print(data[i], HEX); } Serial.println(); }
const char *stateName() { switch (state) { case State::BOOT: return "BOOT"; case State::ARM_REQUEST: return "ARM_REQUEST"; case State::REQUEST_RECEIVED: return "REQUEST_RECEIVED"; case State::VALIDATE_REQUEST: return "VALIDATE_REQUEST"; case State::PREPARE_RESPONSE: return "PREPARE_RESPONSE"; case State::RESPONSE_READY: return "RESPONSE_READY"; case State::WAIT_RESPONSE_CLOCK: return "WAIT_RESPONSE_CLOCK"; case State::RESPONSE_CLOCKED: return "RESPONSE_CLOCKED"; case State::PASS: return "PASS"; case State::FIRST_FAILURE: return "FIRST_FAILURE"; case State::HALTED: return "HALTED"; } return "UNKNOWN"; }
void printCounters() { Serial.print("C5 requestTransactions="); Serial.print(requestTransactions); Serial.print(" responseTransactions="); Serial.print(responseTransactions); Serial.print(" actualRxBytes="); Serial.print(actualRxBytes); Serial.print(" actualTxBytes="); Serial.print(actualTxBytes); Serial.print(" validRequests="); Serial.print(validRequests); Serial.print(" transactionLengthErrors="); Serial.print(transactionLengthErrors); Serial.print(" magicErrors="); Serial.print(magicErrors); Serial.print(" versionErrors="); Serial.print(versionErrors); Serial.print(" typeErrors="); Serial.print(typeErrors); Serial.print(" lengthErrors="); Serial.print(lengthErrors); Serial.print(" crcErrors="); Serial.print(crcErrors); Serial.print(" payloadErrors="); Serial.print(payloadErrors); Serial.print(" sequenceErrors="); Serial.print(sequenceErrors); Serial.print(" spiErrors="); Serial.print(spiErrors); Serial.print(" queueErrors="); Serial.print(queueErrors); Serial.print(" READY+="); Serial.print(readyAssertCount); Serial.print(" READY-="); Serial.println(readyDeassertCount); }
void printFinalSummary() { Serial.println("=============================="); if (state == State::PASS) Serial.println("[PHASE A PASS]"); else Serial.println("[FIRST FAILURE]"); Serial.println("=============================="); Serial.print("failureReason="); Serial.println(failureReason); Serial.print("state="); Serial.println(stateName()); Serial.print("transactionNumber="); Serial.println(transactionNumber); Serial.print("successfulCycles="); Serial.println(successfulCycles); Serial.print("trans_len_bits="); Serial.println(firstRxTransLenBits); Serial.print("trans_len_bytes="); Serial.println(firstRxTransLenBytes); Serial.print("response_trans_len_bits="); Serial.println(firstResponseTransLenBits); Serial.print("response_trans_len_bytes="); Serial.println(firstResponseTransLenBytes); Serial.print("C5_READY_AFTER_ASSERT="); Serial.println(readyAfterAssert ? "HIGH" : "LOW"); printHex("C5 ACTUAL REQUEST RX FIRST 32: ", firstActualRx, 32); printHex("C5 EXPECTED REQUEST FIRST 32: ", firstExpectedRx, 32); printHex("C5 PREPARED RESPONSE FIRST 32: ", firstPreparedTx, 32); printCounters(); Serial.print("lastRxSequence="); Serial.println(lastRxSequence); Serial.print("lastTxSequence="); Serial.println(lastTxSequence); Serial.print("elapsedMs="); Serial.println(millis() - phaseStartMs); Serial.println("[TEST HALTED]"); finalPrinted = true; }
void haltWith(const char *reason) { if (halted) return; failureReason = reason; state = State::FIRST_FAILURE; halted = true; if (!finalPrinted) printFinalSummary(); }

bool validateLength(uint32_t &bits, uint32_t &bytes) { bits = static_cast<uint32_t>(transaction.trans_len); bytes = bits / 8u; if (bits % 8u) { ++transactionLengthErrors; haltWith("BAD_TRANS_LEN_NOT_BYTE_ALIGNED"); return false; } return true; }
const char *validateRequest(uint32_t sequence, uint8_t type, uint16_t length) { if (get16(rxFrame) != MAGIC) { ++magicErrors; return "BAD_MAGIC"; } if (rxFrame[2] != VERSION) { ++versionErrors; return "BAD_VERSION"; } if (type != TYPE_HELLO) { ++typeErrors; return "BAD_TYPE"; } if (length > MAX_PAYLOAD || HEADER_BYTES + length + 4 > FRAME_BYTES) { ++lengthErrors; return "BAD_LENGTH"; } if (get32(rxFrame + HEADER_BYTES + length) != crc32(rxFrame, HEADER_BYTES + length)) { ++crcErrors; return "BAD_CRC"; } for (size_t i = 0; i < length; ++i) if (rxFrame[HEADER_BYTES + i] != expectedPayload(sequence, i)) { ++payloadErrors; return "BAD_PAYLOAD"; } if (sequence != expectedSequence || (haveRxSequence && sequence != lastRxSequence + 1u)) { ++sequenceErrors; return "BAD_SEQUENCE"; } return nullptr; }
void buildResponse(uint8_t *frame, uint32_t seq) { memset(frame, 0, FRAME_BYTES); put16(frame, MAGIC); frame[2] = VERSION; frame[3] = TYPE_PONG; put32(frame + 4, seq); put16(frame + 8, 32); put16(frame + 10, 0); put32(frame + 12, 0); put32(frame + HEADER_BYTES, seq); const char identity[] = "C5-SPI-V3.3S"; memcpy(frame + HEADER_BYTES + 4, identity, sizeof(identity) - 1); for (size_t i = 16; i < 32; ++i) frame[HEADER_BYTES + i] = expectedPayload(seq, i - 16); put32(frame + HEADER_BYTES + 32, crc32(frame, HEADER_BYTES + 32)); }

void spiSlaveTask(void *) {
  uint8_t expectedFrame[FRAME_BYTES]; memset(&transaction, 0, sizeof(transaction)); memset(rxFrame, 0, sizeof(rxFrame)); memset(txFrame, 0, sizeof(txFrame));
  for (;;) {
    state = State::ARM_REQUEST; digitalWrite(C5_READY, LOW); ++readyDeassertCount; transaction.length = FRAME_BYTES * 8; transaction.rx_buffer = rxFrame; transaction.tx_buffer = txFrame;
    const esp_err_t requestResult = spi_slave_transmit(SPI_HOST, &transaction, portMAX_DELAY); if (requestResult != ESP_OK) { ++spiErrors; haltWith("SPI_REQUEST_ERROR"); return; }
    state = State::REQUEST_RECEIVED; ++transactionNumber; ++requestTransactions; uint32_t transBits = static_cast<uint32_t>(transaction.trans_len), transBytes = transBits / 8u; if (transBits % 8u) { ++transactionLengthErrors; haltWith("BAD_TRANS_LEN_NOT_BYTE_ALIGNED"); return; } actualRxBytes += transBytes;
    if (!firstCaptured) { firstRxTransLenBits = transBits; firstRxTransLenBytes = transBytes; memcpy(firstActualRx, rxFrame, 32); buildExpectedRequest(expectedFrame, expectedSequence); memcpy(firstExpectedRx, expectedFrame, 32); firstCaptured = true; }
    if (transBits != FRAME_BYTES * 8u) { ++transactionLengthErrors; haltWith("BAD_TRANS_LEN"); return; }
    state = State::VALIDATE_REQUEST; const uint32_t seq = get32(rxFrame + 4); const uint8_t type = rxFrame[3]; const uint16_t len = get16(rxFrame + 8); const char *bad = validateRequest(seq, type, len); if (bad) { haltWith(bad); return; }
    ++validRequests; lastRxSequence = seq; haveRxSequence = true;
    state = State::PREPARE_RESPONSE; buildResponse(txFrame, seq); if (firstCaptured) memcpy(firstPreparedTx, txFrame, 32); lastTxSequence = seq;
    state = State::RESPONSE_READY; digitalWrite(C5_READY, HIGH); ++readyAssertCount; readyAfterAssert = digitalRead(C5_READY);
    state = State::WAIT_RESPONSE_CLOCK; memset(rxFrame, 0, sizeof(rxFrame)); transaction.length = FRAME_BYTES * 8; transaction.rx_buffer = rxFrame; transaction.tx_buffer = txFrame;
    const esp_err_t responseResult = spi_slave_transmit(SPI_HOST, &transaction, portMAX_DELAY); if (responseResult != ESP_OK) { ++spiErrors; digitalWrite(C5_READY, LOW); ++readyDeassertCount; haltWith("SPI_RESPONSE_ERROR"); return; }
    state = State::RESPONSE_CLOCKED; ++transactionNumber; ++responseTransactions; firstResponseTransLenBits = static_cast<uint32_t>(transaction.trans_len); firstResponseTransLenBytes = firstResponseTransLenBits / 8u; actualTxBytes += firstResponseTransLenBytes; digitalWrite(C5_READY, LOW); ++readyDeassertCount;
    if ((firstResponseTransLenBits % 8u) || firstResponseTransLenBits != FRAME_BYTES * 8u) { ++transactionLengthErrors; haltWith("BAD_RESPONSE_TRANS_LEN"); return; }
    ++successfulCycles;
    ++expectedSequence;
    if (successfulCycles >= 1000) { state = State::PASS; failureReason = "NONE"; halted = true; printFinalSummary(); return; }
  }
}
void reprintIfRequested() { if (!halted || !Serial.available()) return; const int c = Serial.read(); if (c == 'r' || c == 'R') { finalPrinted = false; printFinalSummary(); } }

void setup() {
  Serial.begin(115200); delay(100); phaseStartMs = millis(); Serial.println("ND Pico2DMX V3.3S ESP32-C5 SPI SLAVE"); Serial.println("[MODE] BASIC FINITE FORENSIC"); Serial.println("[SPI] host=SPI2 mode=0 DMA=auto length=4352 bits"); Serial.println("[PINS] SCK=GPIO25 MOSI=GPIO26 MISO=GPIO8 CS=GPIO9 READY=GPIO3"); pinMode(C5_READY, OUTPUT); digitalWrite(C5_READY, LOW);
  spi_bus_config_t busConfig = {}; busConfig.mosi_io_num = C5_MOSI; busConfig.miso_io_num = C5_MISO; busConfig.sclk_io_num = C5_SCK; busConfig.quadwp_io_num = -1; busConfig.quadhd_io_num = -1; busConfig.max_transfer_sz = FRAME_BYTES;
  spi_slave_interface_config_t slaveConfig = {}; slaveConfig.spics_io_num = C5_CS; slaveConfig.flags = 0; slaveConfig.queue_size = 1; slaveConfig.mode = 0;
  const esp_err_t initResult = spi_slave_initialize(SPI_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO); if (initResult != ESP_OK) { failureReason = "SPI_INIT_ERROR"; state = State::FIRST_FAILURE; halted = true; printFinalSummary(); return; }
  Serial.println("[SPI] initialize PASS"); Serial.println("[STATE] WAITING"); xTaskCreate(spiSlaveTask, "spi_slave_task", 8192, nullptr, 3, nullptr);
}
void loop() { reprintIfRequested(); delay(halted ? 10 : 1); }
