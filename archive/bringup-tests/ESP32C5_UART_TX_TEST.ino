/* Temporary ESP32-C5 UART TX isolation test.
   No Wi-Fi, ND1 framing, telemetry, or production protocol. */
#include <Arduino.h>

static constexpr int RP_RX_PIN = 12;
static constexpr int RP_TX_PIN = 11;
static constexpr uint32_t UART_BAUD = 460800;

HardwareSerial rpLink(1);
uint32_t sequence = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  rpLink.begin(UART_BAUD, SERIAL_8N1, RP_RX_PIN, RP_TX_PIN);
  Serial.println("ESP32-C5 UART TX TEST: RX GPIO12, TX GPIO11, 460800 8N1");
}

void loop() {
  ++sequence;
  char line[32];
  snprintf(line, sizeof(line), "ESP_TEST_%06lu\n", static_cast<unsigned long>(sequence));
  rpLink.print(line);
  Serial.print("TX: ");
  Serial.print(line);
  delay(500);
}
