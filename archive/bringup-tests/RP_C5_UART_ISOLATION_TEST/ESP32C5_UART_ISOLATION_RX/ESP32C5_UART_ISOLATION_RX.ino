#include <Arduino.h>

HardwareSerial testUart(1);

static constexpr uint32_t UART_BAUD = 9600;
static constexpr uint8_t UART_RX_PIN = 12;
static constexpr uint8_t UART_TX_PIN = 11;
static constexpr size_t CAPTURE_SIZE = 128;
static constexpr uint32_t PARTIAL_INTERVAL_MS = 2000;
static constexpr uint32_t TOTAL_INTERVAL_MS = 2000;

uint32_t availableEvents = 0;
uint32_t validBytes = 0;
uint32_t negativeReads = 0;
uint32_t newlines = 0;
uint32_t lineCount = 0;
uint32_t overflowCount = 0;

uint8_t capture[CAPTURE_SIZE];
size_t captureLength = 0;
uint32_t lastPartialAt = 0;
uint32_t lastTotalsAt = 0;

static void printHex(const uint8_t *data, size_t length) {
  Serial.print("[RX HEX] ");
  for (size_t i = 0; i < length; ++i) {
    if (i != 0) {
      Serial.print(' ');
    }
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

static void printAscii(const uint8_t *data, size_t length,
                       const char *prefix) {
  Serial.print(prefix);
  for (size_t i = 0; i < length; ++i) {
    const uint8_t value = data[i];
    Serial.print((value >= 0x20 && value <= 0x7e) ?
                     static_cast<char>(value) : '.');
  }
  Serial.println();
}

static void printCompleteLine() {
  Serial.printf("[RX LINE] len=%lu\n",
                static_cast<unsigned long>(captureLength));
  printAscii(capture, captureLength, "[RX ASCII] ");
  printHex(capture, captureLength);
  captureLength = 0;
}

static void printPartial() {
  if (captureLength == 0) {
    return;
  }

  Serial.printf("[RX PARTIAL] len=%lu\n",
                static_cast<unsigned long>(captureLength));
  printHex(capture, captureLength);
  printAscii(capture, captureLength, "[RX ASCII] ");
}

static void serviceReceive() {
  const int pending = testUart.available();
  if (pending <= 0) {
    return;
  }

  ++availableEvents;
  while (testUart.available() > 0) {
    const int value = testUart.read();
    if (value < 0) {
      ++negativeReads;
      continue;
    }

    ++validBytes;
    const uint8_t byteValue = static_cast<uint8_t>(value);
    if (captureLength >= CAPTURE_SIZE) {
      ++overflowCount;
      captureLength = 0;
    }
    capture[captureLength++] = byteValue;

    if (byteValue == '\n') {
      ++newlines;
      ++lineCount;
      printCompleteLine();
    }
  }
}

static void printTotals() {
  Serial.printf("[RX TOTAL]\navailableEvents=%lu\nvalidBytes=%lu\n"
                "negativeReads=%lu\nnewlines=%lu\nlines=%lu\n"
                "overflows=%lu\n",
                static_cast<unsigned long>(availableEvents),
                static_cast<unsigned long>(validBytes),
                static_cast<unsigned long>(negativeReads),
                static_cast<unsigned long>(newlines),
                static_cast<unsigned long>(lineCount),
                static_cast<unsigned long>(overflowCount));
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("[C5 UART1 ISOLATION RX]");
  Serial.println("UART1 RX=GPIO12 TX=GPIO11 9600 8N1");

  testUart.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  const uint32_t now = millis();
  lastPartialAt = now;
  lastTotalsAt = now;
}

void loop() {
  serviceReceive();

  const uint32_t now = millis();
  if (now - lastPartialAt >= PARTIAL_INTERVAL_MS) {
    printPartial();
    lastPartialAt = now;
  }
  if (now - lastTotalsAt >= TOTAL_INTERVAL_MS) {
    printTotals();
    lastTotalsAt = now;
  }
}
