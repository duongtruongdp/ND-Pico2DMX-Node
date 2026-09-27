#include <Arduino.h>

static constexpr uint8_t GPIO_TX0_CANDIDATE = 11;
static constexpr uint8_t GPIO_RX0_CANDIDATE = 12;
static constexpr uint32_t STATE_DURATION_MS = 5000;

struct TestState {
  const char *label;
  uint8_t gpio11Level;
  uint8_t gpio12Level;
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
  digitalWrite(GPIO_TX0_CANDIDATE, state.gpio11Level);
  digitalWrite(GPIO_RX0_CANDIDATE, state.gpio12Level);

  Serial.printf("[STATE %s] GPIO11=%s GPIO12=%s\n",
                state.label,
                state.gpio11Level == HIGH ? "HIGH" : "LOW",
                state.gpio12Level == HIGH ? "HIGH" : "LOW");
}

void setup() {
  Serial.begin(115200);

  pinMode(GPIO_TX0_CANDIDATE, OUTPUT);
  pinMode(GPIO_RX0_CANDIDATE, OUTPUT);

  Serial.println("[C5 GPIO MAPPING TEST]");
  Serial.println("GPIO11 and GPIO12 only; no HardwareSerial or Wi-Fi");

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
