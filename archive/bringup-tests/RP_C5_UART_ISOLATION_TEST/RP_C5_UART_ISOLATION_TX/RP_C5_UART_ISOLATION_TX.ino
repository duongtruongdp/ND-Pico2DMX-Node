#include <Arduino.h>

static constexpr uint32_t UART_BAUD = 9600;
static constexpr uint8_t UART_TX_PIN = 4;
static constexpr uint32_t TX_INTERVAL_MS = 2000;
static constexpr uint32_t TOTAL_INTERVAL_MS = 5000;

uint32_t txFrames = 0;
uint32_t txRequestedBytes = 0;
uint32_t txWrittenBytes = 0;
uint32_t sequence = 1;
uint32_t nextTransmitAt = 0;
uint32_t nextTotalsAt = 0;

static void transmitFrame() {
  char frame[40];
  const int length = snprintf(frame, sizeof(frame),
                              "RPTEST|%06lu|55AA1234\n",
                              static_cast<unsigned long>(sequence));
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(frame)) {
    return;
  }

  const size_t requested = static_cast<size_t>(length);
  const size_t written = Serial2.write(
      reinterpret_cast<const uint8_t *>(frame), requested);

  ++txFrames;
  txRequestedBytes += static_cast<uint32_t>(requested);
  txWrittenBytes += static_cast<uint32_t>(written);

  Serial.printf("[TX] seq=%lu requested=%lu written=%lu\n",
                static_cast<unsigned long>(sequence),
                static_cast<unsigned long>(requested),
                static_cast<unsigned long>(written));
  ++sequence;
}

static void printTotals() {
  Serial.printf("[TX TOTAL] frames=%lu requestedBytes=%lu writtenBytes=%lu\n",
                static_cast<unsigned long>(txFrames),
                static_cast<unsigned long>(txRequestedBytes),
                static_cast<unsigned long>(txWrittenBytes));
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("[RP UART1 ISOLATION TX]");
  Serial.println("Serial2 UART1 TX=GP4 9600 8N1");

  Serial2.setTX(UART_TX_PIN);
  Serial2.begin(UART_BAUD);

  const uint32_t now = millis();
  nextTransmitAt = now + 2000;
  nextTotalsAt = now + 5000;
}

void loop() {
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextTransmitAt) >= 0) {
    transmitFrame();
    nextTransmitAt += TX_INTERVAL_MS;
  }
  if (static_cast<int32_t>(now - nextTotalsAt) >= 0) {
    printTotals();
    nextTotalsAt += TOTAL_INTERVAL_MS;
  }
}
