#include <Arduino.h>
#include <esp_system.h>

HardwareSerial testUart(1);

static constexpr int UART_RX_PIN = 12;
static constexpr int UART_TX_PIN = 11;
static constexpr uint32_t UART_BAUD = 460800;
static constexpr size_t CAPTURE_SIZE = 64;

uint32_t txFrames = 0;
uint32_t txBytes = 0;
uint32_t lastWrite = 0;
uint32_t availableEvents = 0;
uint32_t rxBytes = 0;
uint32_t negativeReads = 0;
uint32_t newlineCount = 0;
uint32_t sequence = 1;

uint8_t capture[CAPTURE_SIZE];
size_t captureLength = 0;

uint32_t nextTransmit = 0;
uint32_t nextSummary = 0;

static void transmitTestFrame() {
  char frame[40];
  const int length = snprintf(frame, sizeof(frame),
                              "C5TEST|%06lu|55AA1234\n",
                              static_cast<unsigned long>(sequence));
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(frame)) {
    return;
  }

  lastWrite = static_cast<uint32_t>(
      testUart.write(reinterpret_cast<const uint8_t *>(frame),
                     static_cast<size_t>(length)));
  txBytes += lastWrite;
  ++txFrames;
  ++sequence;
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

    ++rxBytes;
    const uint8_t byteValue = static_cast<uint8_t>(value);
    if (byteValue == '\n') {
      ++newlineCount;
    }
    if (captureLength < CAPTURE_SIZE) {
      capture[captureLength++] = byteValue;
    }
  }
}

static void printCapture() {
  if (captureLength == 0) {
    return;
  }

  Serial.print("[RX HEX] ");
  for (size_t i = 0; i < captureLength; ++i) {
    if (i != 0) {
      Serial.print(' ');
    }
    if (capture[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(capture[i], HEX);
  }
  Serial.println();

  Serial.print("[RX ASCII] ");
  for (size_t i = 0; i < captureLength; ++i) {
    const uint8_t value = capture[i];
    Serial.print((value >= 0x20 && value <= 0x7e) ?
                     static_cast<char>(value) : '.');
  }
  Serial.println();

  captureLength = 0;
}

static void printSummary() {
  Serial.printf("[TX] frames=%lu bytes=%lu lastWrite=%lu\n",
                static_cast<unsigned long>(txFrames),
                static_cast<unsigned long>(txBytes),
                static_cast<unsigned long>(lastWrite));
  Serial.printf("[RX] availableEvents=%lu validBytes=%lu negativeReads=%lu newlines=%lu\n",
                static_cast<unsigned long>(availableEvents),
                static_cast<unsigned long>(rxBytes),
                static_cast<unsigned long>(negativeReads),
                static_cast<unsigned long>(newlineCount));
  printCapture();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("[C5 UART TEST]");
  Serial.println("UART1 RX=GPIO12 TX=GPIO11");
  Serial.println("460800 8N1");
  Serial.printf("resetReason=%d\n", static_cast<int>(esp_reset_reason()));

  testUart.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  const uint32_t now = millis();
  nextTransmit = now + 2000;
  nextSummary = now + 2000;
}

void loop() {
  serviceReceive();

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextTransmit) >= 0) {
    transmitTestFrame();
    nextTransmit += 2000;
  }
  if (static_cast<int32_t>(now - nextSummary) >= 0) {
    printSummary();
    nextSummary += 2000;
  }
}
