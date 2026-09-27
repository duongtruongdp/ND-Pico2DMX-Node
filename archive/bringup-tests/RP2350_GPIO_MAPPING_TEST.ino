#include <Arduino.h>

static constexpr uint8_t GP12 = 12;
static constexpr uint8_t GP13 = 13;
static constexpr uint32_t STATE_DURATION_MS = 5000;

struct TestState {
  const char *label;
  uint8_t gp12Level;
  uint8_t gp13Level;
};

static constexpr TestState STATES[] = {
  {"A", LOW, LOW},
  {"B", HIGH, LOW},
  {"C", LOW, HIGH},
  {"D", HIGH, HIGH},
};

static constexpr size_t STATE_COUNT = sizeof(STATES) / sizeof(STATES[0]);
size_t stateIndex = 0;
uint32_t stateStartedAt = 0;

static void applyState(size_t index) {
  const TestState &state = STATES[index];
  digitalWrite(GP12, state.gp12Level);
  digitalWrite(GP13, state.gp13Level);

  Serial.print("[STATE ");
  Serial.print(state.label);
  Serial.print("] GP12=");
  Serial.print(state.gp12Level == HIGH ? "HIGH" : "LOW");
  Serial.print(" GP13=");
  Serial.println(state.gp13Level == HIGH ? "HIGH" : "LOW");
}

void setup() {
  Serial.begin(115200);

  pinMode(GP12, OUTPUT);
  pinMode(GP13, OUTPUT);

  Serial.println();
  Serial.println("[RP2350 GPIO MAPPING TEST]");
  Serial.println("GP12 and GP13 only; no UART, Ethernet, DMX, or PIO");

  applyState(stateIndex);
  stateStartedAt = millis();
}

void loop() {
  if (millis() - stateStartedAt < STATE_DURATION_MS) {
    return;
  }

  stateIndex = (stateIndex + 1) % STATE_COUNT;
  applyState(stateIndex);
  stateStartedAt = millis();
}
