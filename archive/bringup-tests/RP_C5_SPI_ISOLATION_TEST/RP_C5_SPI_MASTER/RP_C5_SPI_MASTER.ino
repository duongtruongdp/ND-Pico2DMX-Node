#include <SPI.h>

// V3.3S finite forensic RP2350 SPI master diagnostic.
// Production UART, W5500, Ethernet, DMX, PIO, EEPROM, and DMA are not used.

static constexpr uint32_t SPI_CLOCK_HZ = 1000000UL;
static constexpr uint8_t RP_MISO = 12;
static constexpr uint8_t RP_CS = 13;
static constexpr uint8_t RP_SCK = 14;
static constexpr uint8_t RP_MOSI = 15;
static constexpr uint8_t RP_READY = 6;
static constexpr uint16_t MAGIC = 0x4E44;
static constexpr uint8_t VERSION = 1;
static constexpr uint8_t TYPE_HELLO = 1;
static constexpr uint8_t TYPE_PONG = 0x81;
static constexpr uint8_t TYPE_THROUGHPUT_RESPONSE = 0x82;
static constexpr size_t HEADER_BYTES = 16;
static constexpr size_t MAX_PAYLOAD = 513;
static constexpr size_t FRAME_BYTES = 544;
static constexpr uint32_t READY_TIMEOUT_MS = 250;
static constexpr uint32_t BASIC_PERIOD_MS = 100;

static_assert(HEADER_BYTES + MAX_PAYLOAD + 4 <= FRAME_BYTES, "Frame is too small");

enum class State : uint8_t { BOOT, SEND_REQUEST, WAIT_RESPONSE_READY, READ_RESPONSE, VALIDATE_RESPONSE, PASS, FIRST_FAILURE, LINK_TIMEOUT, HALTED };

SPISettings spiSettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0);
alignas(4) uint8_t txFrame[FRAME_BYTES], rxFrame[FRAME_BYTES];
alignas(4) uint8_t firstTx[32], firstRequestRx[32], firstResponseRx[32], expectedFirstRequest[32];
State state = State::BOOT;
bool halted = false, finalPrinted = false, firstTxCaptured = false, firstRequestRxCaptured = false, firstResponseRxCaptured = false, haveRxSequence = false;
uint32_t nextSequence = 1, transactionNumber = 0, requestTransactions = 0, responseTransactions = 0, txBytes = 0, rxBytes = 0, successfulCycles = 0, validResponses = 0;
uint32_t magicErrors = 0, versionErrors = 0, typeErrors = 0, lengthErrors = 0, crcErrors = 0, payloadErrors = 0, sequenceErrors = 0, readyWaits = 0, readyDetections = 0, readyTimeouts = 0, spiErrors = 0;
uint32_t lastTxSequence = 0, lastRxSequence = 0, phaseStartMs = 0, lastStatusMs = 0, lastCycleMs = 0;
uint8_t readyBeforeRequest = LOW, readyAfterRequest = LOW, readyFirstWaitSample = LOW, readyFinalSample = LOW;
const char *failureReason = "NONE";

uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) { crc ^= data[i]; for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u); }
  return crc ^ 0xFFFFFFFFu;
}
void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8); }
uint32_t get32(const uint8_t *p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint8_t expectedPayload(uint32_t seq, size_t i) { return static_cast<uint8_t>((seq * 17u + i * 29u + 0x5Au) & 0xFFu); }

void buildRequest(uint8_t *frame, uint32_t seq) {
  memset(frame, 0, FRAME_BYTES); put16(frame, MAGIC); frame[2] = VERSION; frame[3] = TYPE_HELLO; put32(frame + 4, seq); put16(frame + 8, 32); put16(frame + 10, 0); put32(frame + 12, 0);
  for (size_t i = 0; i < 32; ++i) frame[HEADER_BYTES + i] = expectedPayload(seq, i);
  put32(frame + HEADER_BYTES + 32, crc32(frame, HEADER_BYTES + 32));
}
void printHex(const char *label, const uint8_t *data, size_t n) { Serial.print(label); for (size_t i = 0; i < n; ++i) { Serial.print(i ? " " : ""); if (data[i] < 16) Serial.print('0'); Serial.print(data[i], HEX); } Serial.println(); }
const char *stateName() { switch (state) { case State::BOOT: return "BOOT"; case State::SEND_REQUEST: return "SEND_REQUEST"; case State::WAIT_RESPONSE_READY: return "WAIT_RESPONSE_READY"; case State::READ_RESPONSE: return "READ_RESPONSE"; case State::VALIDATE_RESPONSE: return "VALIDATE_RESPONSE"; case State::PASS: return "PASS"; case State::FIRST_FAILURE: return "FIRST_FAILURE"; case State::LINK_TIMEOUT: return "LINK_TIMEOUT"; case State::HALTED: return "HALTED"; } return "UNKNOWN"; }

void printCounters() {
  Serial.print("RP requestTransactions="); Serial.print(requestTransactions); Serial.print(" responseTransactions="); Serial.print(responseTransactions); Serial.print(" actualTxBytes="); Serial.print(txBytes); Serial.print(" actualRxBytes="); Serial.print(rxBytes); Serial.print(" successfulCycles="); Serial.print(successfulCycles); Serial.print(" validResponses="); Serial.print(validResponses);
  Serial.print(" magicErrors="); Serial.print(magicErrors); Serial.print(" versionErrors="); Serial.print(versionErrors); Serial.print(" typeErrors="); Serial.print(typeErrors); Serial.print(" lengthErrors="); Serial.print(lengthErrors); Serial.print(" crcErrors="); Serial.print(crcErrors); Serial.print(" payloadErrors="); Serial.print(payloadErrors); Serial.print(" sequenceErrors="); Serial.print(sequenceErrors); Serial.print(" readyWaits="); Serial.print(readyWaits); Serial.print(" readyDetections="); Serial.print(readyDetections); Serial.print(" readyTimeouts="); Serial.print(readyTimeouts); Serial.print(" spiErrors="); Serial.println(spiErrors);
}
void printFinalSummary() {
  Serial.println("=============================="); if (state == State::PASS) Serial.println("[PHASE A PASS]"); else if (state == State::LINK_TIMEOUT) Serial.println("[LINK/READY TIMEOUT]"); else Serial.println("[FIRST FAILURE]"); Serial.println("==============================");
  Serial.print("failureReason="); Serial.println(failureReason); Serial.print("state="); Serial.println(stateName()); Serial.print("sequence="); Serial.println(lastTxSequence); Serial.print("transactionNumber="); Serial.println(transactionNumber); Serial.print("SPI_CLOCK_HZ="); Serial.println(SPI_CLOCK_HZ); Serial.println("SPI_MODE=0 MSB_FIRST");
  printHex("RP INTENDED TX FIRST 32: ", firstTx, 32); printHex("RP EXPECTED FIRST 32: ", expectedFirstRequest, 32); printHex("RP REQUEST-TRANSACTION MISO FIRST 32: ", firstRequestRx, 32); printHex("RP ACTUAL RESPONSE RX FIRST 32: ", firstResponseRx, 32);
  Serial.print("READY before_request="); Serial.println(readyBeforeRequest ? "HIGH" : "LOW"); Serial.print("READY after_request="); Serial.println(readyAfterRequest ? "HIGH" : "LOW"); Serial.print("READY first_wait_sample="); Serial.println(readyFirstWaitSample ? "HIGH" : "LOW"); Serial.print("READY final_wait_sample="); Serial.println(readyFinalSample ? "HIGH" : "LOW");
  if (state == State::LINK_TIMEOUT && readyFinalSample == LOW) Serial.println("READY ELECTRICAL PATH SUSPECTED only if C5 readback reports HIGH");
  printCounters(); Serial.print("lastTxSequence="); Serial.println(lastTxSequence); Serial.print("lastRxSequence="); Serial.println(lastRxSequence); Serial.print("elapsedMs="); Serial.println(millis() - phaseStartMs); Serial.println("[TEST HALTED]"); finalPrinted = true;
}
void haltWith(const char *reason, State terminal) { if (halted) return; failureReason = reason; state = terminal; halted = true; if (!finalPrinted) printFinalSummary(); }
void performTransfer() { SPI1.beginTransaction(spiSettings); digitalWrite(RP_CS, LOW); SPI1.transfer(txFrame, rxFrame, FRAME_BYTES); digitalWrite(RP_CS, HIGH); SPI1.endTransaction(); }

bool waitForReady() {
  state = State::WAIT_RESPONSE_READY; ++readyWaits; readyFirstWaitSample = digitalRead(RP_READY); const uint32_t start = millis();
  while (digitalRead(RP_READY) == LOW) { if (millis() - start >= READY_TIMEOUT_MS) { readyFinalSample = digitalRead(RP_READY); ++readyTimeouts; return false; } yield(); }
  readyFinalSample = HIGH; ++readyDetections; return true;
}
bool validateResponse(uint32_t expectedSeq) {
  state = State::VALIDATE_RESPONSE; const uint16_t magic = get16(rxFrame); const uint8_t version = rxFrame[2]; const uint8_t type = rxFrame[3]; const uint32_t seq = get32(rxFrame + 4); const uint16_t len = get16(rxFrame + 8);
  if (magic != MAGIC) { ++magicErrors; haltWith("BAD_MAGIC", State::FIRST_FAILURE); return false; } if (version != VERSION) { ++versionErrors; haltWith("BAD_VERSION", State::FIRST_FAILURE); return false; } if (type != TYPE_PONG && type != TYPE_THROUGHPUT_RESPONSE) { ++typeErrors; haltWith("BAD_TYPE", State::FIRST_FAILURE); return false; } if (len > MAX_PAYLOAD || HEADER_BYTES + len + 4 > FRAME_BYTES) { ++lengthErrors; haltWith("BAD_LENGTH", State::FIRST_FAILURE); return false; } if (get32(rxFrame + HEADER_BYTES + len) != crc32(rxFrame, HEADER_BYTES + len)) { ++crcErrors; haltWith("BAD_CRC", State::FIRST_FAILURE); return false; } if (seq != expectedSeq || (haveRxSequence && seq != lastRxSequence + 1u)) { ++sequenceErrors; haltWith("BAD_SEQUENCE", State::FIRST_FAILURE); return false; } if (len < 4 || get32(rxFrame + HEADER_BYTES) != expectedSeq) { ++payloadErrors; haltWith("BAD_PAYLOAD", State::FIRST_FAILURE); return false; }
  for (size_t i = 16; i < len; ++i) if (rxFrame[HEADER_BYTES + i] != expectedPayload(expectedSeq, i - 16)) { ++payloadErrors; haltWith("BAD_PAYLOAD", State::FIRST_FAILURE); return false; }
  ++validResponses; lastRxSequence = seq; haveRxSequence = true; return true;
}
void printStatus() { if (halted || millis() - lastStatusMs < 1000) return; lastStatusMs = millis(); Serial.print("[STATUS] state="); Serial.print(stateName()); Serial.print(" cycles="); Serial.print(successfulCycles); Serial.print(" requestTx="); Serial.print(requestTransactions); Serial.print(" responseTx="); Serial.print(responseTransactions); Serial.print(" readyDetections="); Serial.print(readyDetections); Serial.print(" readyTimeouts="); Serial.println(readyTimeouts); }
void reprintIfRequested() { if (!halted || !Serial.available()) return; const int c = Serial.read(); if (c == 'r' || c == 'R') { finalPrinted = false; printFinalSummary(); } }

void setup() {
  Serial.begin(115200); delay(100); Serial.println("ND Pico2DMX V3.3S RP SPI MASTER"); Serial.println("[MODE] BASIC FINITE FORENSIC"); Serial.print("[SPI] SPI1 mode=0 clock="); Serial.println(SPI_CLOCK_HZ); Serial.println("[PINS] MISO=GP12 CS=GP13 SCK=GP14 MOSI=GP15 READY=GP6");
  pinMode(RP_CS, OUTPUT); digitalWrite(RP_CS, HIGH); pinMode(RP_READY, INPUT_PULLDOWN); SPI1.setRX(RP_MISO); SPI1.setCS(RP_CS); SPI1.setSCK(RP_SCK); SPI1.setTX(RP_MOSI); SPI1.begin(false);
  phaseStartMs = millis(); lastStatusMs = phaseStartMs; lastCycleMs = phaseStartMs - BASIC_PERIOD_MS; state = State::SEND_REQUEST; Serial.println("[STATE] WAITING");
}

void loop() {
  reprintIfRequested(); if (halted) { delay(10); return; } printStatus(); if (millis() - lastCycleMs < BASIC_PERIOD_MS) { delay(1); return; } lastCycleMs = millis();
  state = State::SEND_REQUEST; readyBeforeRequest = digitalRead(RP_READY); buildRequest(txFrame, nextSequence); if (!firstTxCaptured) { memcpy(firstTx, txFrame, 32); uint8_t expected[FRAME_BYTES]; buildRequest(expected, nextSequence); memcpy(expectedFirstRequest, expected, 32); firstTxCaptured = true; } memset(rxFrame, 0, sizeof(rxFrame)); ++transactionNumber; ++requestTransactions; lastTxSequence = nextSequence; performTransfer(); txBytes += FRAME_BYTES; if (!firstRequestRxCaptured) { memcpy(firstRequestRx, rxFrame, 32); firstRequestRxCaptured = true; } readyAfterRequest = digitalRead(RP_READY);
  if (!waitForReady()) { haltWith("READY_TIMEOUT", State::LINK_TIMEOUT); return; }
  state = State::READ_RESPONSE; memset(txFrame, 0, sizeof(txFrame)); memset(rxFrame, 0, sizeof(rxFrame)); performTransfer(); ++transactionNumber; ++responseTransactions; rxBytes += FRAME_BYTES; if (!firstResponseRxCaptured) { memcpy(firstResponseRx, rxFrame, 32); firstResponseRxCaptured = true; } if (digitalRead(RP_READY) == HIGH) { haltWith("READY_REMAINED_HIGH", State::FIRST_FAILURE); return; } if (!validateResponse(nextSequence)) return;
  ++successfulCycles; ++nextSequence; if (successfulCycles >= 1000) { state = State::PASS; failureReason = "NONE"; halted = true; printFinalSummary(); }
}
