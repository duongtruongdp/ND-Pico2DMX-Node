/*
  V3.1 corrected DMX PIO hardware isolation test.

  Target: WIZnet W5500-EVB-Pico2 / RP2350
  Arduino-Pico 6.1.0, FQBN rp2040:rp2040:rpipico2

  This sketch is standalone: no Ethernet, Wi-Fi, UART, EEPROM, dashboard,
  or production-firmware dependencies.

  Default: logical Port 1 only (GP28 / PIO1 SM1).
  Build with -DDMX_TEST_ALL_PORTS=1 for all four connectors.
*/

#include <Arduino.h>
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"

#ifndef DMX_TEST_ALL_PORTS
#define DMX_TEST_ALL_PORTS 0
#endif

static constexpr uint8_t STATUS_LED_PIN = 25;
static constexpr uint32_t PIO_INSTRUCTION_RATE = 1000000;
static constexpr uint32_t DMX_BIT_US = 4;
static constexpr uint32_t DMX_SLOT_US = 44;
static constexpr uint16_t DMX_BYTES = 513;
static constexpr uint32_t DMX_PAYLOAD_US = static_cast<uint32_t>(DMX_BYTES) * DMX_SLOT_US;
static constexpr uint32_t DMX_FRAME_PERIOD_US = 25000;
static constexpr uint32_t DMX_BREAK_US = 110;
static constexpr uint32_t DMX_MAB_US = 12;

// Explicit logical-port mapping. PIO/SM ownership is not derived from the
// logical index:
//   Port 1 -> GP28 -> PIO1 SM1
//   Port 2 -> GP27 -> PIO1 SM0
//   Port 3 -> GP3  -> PIO0 SM1
//   Port 4 -> GP2  -> PIO0 SM0
//
// Program origin is fixed at zero on each PIO. The final pull is both STOP 2
// and the preload for the next byte, so a non-starved FIFO gives 44 cycles per
// slot at the 1 MHz instruction rate.
static uint16_t dmx_tx_program_instructions[5];

static const struct pio_program dmx_tx_program = {
  dmx_tx_program_instructions, 5, 0
};

struct DmxPort {
  PIO pio;
  uint8_t sm;
  uint8_t pin;
  uint offset;
  bool enabled;
  uint32_t txWords[DMX_BYTES];
  uint16_t nextWord;
  uint32_t frameStartUs;
  uint32_t dataStartUs;
  uint32_t nextFrameUs;
  uint32_t frameCount;
  uint32_t starvationCount;
  bool transmitting;
  bool starvationLatched;
};

static DmxPort ports[4] = {
  {pio1, 1, 28, 0, true},
  {pio1, 0, 27, 0, false},
  {pio0, 1,  3, 0, false},
  {pio0, 0,  2, 0, false}
};

static bool pioProgramLoaded[2] = {false, false};
static uint32_t lastDiagnosticMs = 0;
static bool pioReady = false;

static void buildPioProgram() {
  dmx_tx_program_instructions[0] = static_cast<uint16_t>(
      pio_encode_set(pio_x, 7) |
      pio_encode_sideset_opt(2, 0) |
      pio_encode_delay(3));
  dmx_tx_program_instructions[1] = static_cast<uint16_t>(
      pio_encode_out(pio_pins, 1) |
      pio_encode_delay(2));
  dmx_tx_program_instructions[2] = static_cast<uint16_t>(
      pio_encode_jmp_x_dec(1));
  dmx_tx_program_instructions[3] = static_cast<uint16_t>(
      pio_encode_nop() |
      pio_encode_sideset_opt(2, 1) |
      pio_encode_delay(3));
  dmx_tx_program_instructions[4] = static_cast<uint16_t>(
      pio_encode_pull(false, true) |
      pio_encode_sideset_opt(2, 1) |
      pio_encode_delay(3));
}

static uint32_t pioIndex(PIO pio) {
  return pio == pio0 ? 0u : 1u;
}

static void buildPattern(DmxPort& port, uint8_t logicalPort) {
  for (uint16_t i = 0; i < DMX_BYTES; ++i) port.txWords[i] = 0;

  // Word 0 is the start code; channel 1 is word 1.
  switch (logicalPort) {
    case 1: port.txWords[1] = 25; break;
    case 2: port.txWords[2] = 50; break;
    case 3: port.txWords[3] = 75; break;
    case 4:
      port.txWords[1] = 100;
      port.txWords[2] = 100;
      port.txWords[3] = 100;
      break;
    default: break;
  }
}

static bool configurePort(DmxPort& port) {
  const uint index = pioIndex(port.pio);
  if (!pioProgramLoaded[index]) {
    if (!pio_can_add_program_at_offset(port.pio, &dmx_tx_program, 0)) {
      Serial.print(F("PIO program cannot load at offset 0 on PIO"));
      Serial.println(index);
      return false;
    }
    const int loaded = pio_add_program_at_offset(port.pio, &dmx_tx_program, 0);
    if (loaded != 0) {
      Serial.print(F("PIO program load failed on PIO"));
      Serial.println(index);
      return false;
    }
    pioProgramLoaded[index] = true;
  }
  port.offset = 0;

  pio_sm_config config = pio_get_default_sm_config();
  sm_config_set_wrap(&config, port.offset, port.offset + 4);
  sm_config_set_sideset(&config, 2, true, false);
  sm_config_set_out_pins(&config, port.pin, 1);
  sm_config_set_sideset_pins(&config, port.pin);
  sm_config_set_out_shift(&config, true, false, 32); // LSB first, no autopull
  sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX); // 8-word TX FIFO
  sm_config_set_clkdiv(&config,
                       static_cast<float>(clock_get_hz(clk_sys)) /
                       static_cast<float>(PIO_INSTRUCTION_RATE));

  pio_sm_set_enabled(port.pio, port.sm, false);
  pio_gpio_init(port.pio, port.pin);
  pio_sm_set_consecutive_pindirs(port.pio, port.sm, port.pin, 1, true);
  pio_sm_init(port.pio, port.sm, port.offset, &config);
  pio_sm_clear_fifos(port.pio, port.sm);
  pio_sm_set_pins_with_mask(port.pio, port.sm, 1u << port.pin, 1u << port.pin);
  pio_sm_set_enabled(port.pio, port.sm, false);
  return true;
}

static void beginBreak(DmxPort& port, uint32_t frameStartUs) {
  pio_sm_set_enabled(port.pio, port.sm, false);
  gpio_set_function(port.pin, GPIO_FUNC_SIO);
  gpio_set_dir(port.pin, GPIO_OUT);
  gpio_put(port.pin, 0);
  port.frameStartUs = frameStartUs;
  port.nextWord = 0;
  port.transmitting = false;
  port.starvationLatched = false;
}

static void startData(DmxPort& port) {
  pio_gpio_init(port.pio, port.pin);
  pio_sm_set_consecutive_pindirs(port.pio, port.sm, port.pin, 1, true);
  pio_sm_restart(port.pio, port.sm);
  pio_sm_clear_fifos(port.pio, port.sm);

  // Explicit first-byte startup: put byte 0 in the FIFO, execute PULL while
  // the SM is disabled, then jump to SET. Byte 0 is already in OSR before
  // the first start bit; no initial pull stall is possible.
  pio_sm_put(port.pio, port.sm, port.txWords[0]);
  pio_sm_exec(port.pio, port.sm, pio_encode_pull(false, true));
  pio_sm_exec(port.pio, port.sm, pio_encode_jmp(port.offset));

  port.nextWord = 1;
  port.dataStartUs = micros();
  port.transmitting = true;
  pio_sm_set_enabled(port.pio, port.sm, true);
}

static void feedFifo(DmxPort& port) {
  while (port.nextWord < DMX_BYTES &&
         !pio_sm_is_tx_fifo_full(port.pio, port.sm)) {
    pio_sm_put(port.pio, port.sm, port.txWords[port.nextWord++]);
  }
}

static void monitorFifo(DmxPort& port) {
  if (!port.transmitting || port.nextWord >= DMX_BYTES) return;

  // At the pull instruction, an empty FIFO means the PIO is waiting for the
  // next byte. An empty FIFO elsewhere is normal while the OSR is shifting.
  const uint8_t pc = pio_sm_get_pc(port.pio, port.sm);
  const bool stalledAtPull = pc == static_cast<uint8_t>(port.offset + 4) &&
                             pio_sm_is_tx_fifo_empty(port.pio, port.sm);
  if (stalledAtPull && !port.starvationLatched) {
    ++port.starvationCount;
    port.starvationLatched = true;
  } else if (!stalledAtPull) {
    port.starvationLatched = false;
  }
}

static bool frameComplete(const DmxPort& port, uint32_t nowUs) {
  if (!port.transmitting || port.nextWord < DMX_BYTES) return false;
  if (static_cast<uint32_t>(nowUs - port.dataStartUs) < DMX_PAYLOAD_US) return false;
  return pio_sm_is_tx_fifo_empty(port.pio, port.sm);
}

static void printDiagnostics() {
  Serial.println(F("--- V3.1 DMX PIO diagnostics ---"));
  for (uint8_t i = 0; i < 4; ++i) {
    const DmxPort& port = ports[i];
    if (!port.enabled) continue;
    Serial.print(F("Port ")); Serial.print(i + 1);
    Serial.print(F(" GP")); Serial.print(port.pin);
    Serial.print(F(" PIO")); Serial.print(pioIndex(port.pio));
    Serial.print(F(" SM")); Serial.print(port.sm);
    Serial.print(F(" frames=")); Serial.print(port.frameCount);
    Serial.print(F(" fifo_starvation=")); Serial.print(port.starvationCount);
    Serial.println();
  }
}

static void startFrame(DmxPort& port, uint32_t frameStartUs) {
  beginBreak(port, frameStartUs);
  delayMicroseconds(DMX_BREAK_US);
  gpio_put(port.pin, 1);
  delayMicroseconds(DMX_MAB_US);
  startData(port);
  port.nextFrameUs = frameStartUs + DMX_FRAME_PERIOD_US;
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

#if DMX_TEST_ALL_PORTS
  for (uint8_t i = 0; i < 4; ++i) ports[i].enabled = true;
#endif

  Serial.println(F("V3.1 corrected PIO DMX hardware test"));
  Serial.println(F("PIO clock target: 1 MHz; DMX: 250000 8N2"));
  buildPioProgram();

  for (uint8_t i = 0; i < 4; ++i) {
    buildPattern(ports[i], i + 1);
    if (ports[i].enabled && !configurePort(ports[i])) {
      Serial.print(F("Port ")); Serial.print(i + 1);
      Serial.println(F(" configuration FAILED"));
      return;
    }
  }

  pioReady = true;
  const uint32_t frameStartUs = micros();
  for (DmxPort& port : ports) {
    if (port.enabled) startFrame(port, frameStartUs);
  }
}

void loop() {
  if (!pioReady) {
    digitalWrite(STATUS_LED_PIN, (millis() / 150) & 1u);
    return;
  }

  const uint32_t nowUs = micros();
  bool allComplete = true;
  bool anyEnabled = false;

  for (DmxPort& port : ports) {
    if (!port.enabled) continue;
    anyEnabled = true;
    feedFifo(port);
    monitorFifo(port);
    if (!frameComplete(port, nowUs)) allComplete = false;
  }

  static bool waitingForNextFrame = false;
  if (anyEnabled && allComplete) {
    waitingForNextFrame = true;
    for (DmxPort& port : ports) {
      if (port.enabled) {
        port.transmitting = false;
        ++port.frameCount;
      }
    }
  }

  if (waitingForNextFrame) {
    uint32_t nextFrameUs = 0;
    bool initialized = false;
    for (const DmxPort& port : ports) {
      if (!port.enabled) continue;
      if (!initialized || static_cast<int32_t>(port.nextFrameUs - nextFrameUs) < 0) {
        nextFrameUs = port.nextFrameUs;
        initialized = true;
      }
    }
    if (initialized && static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
      const uint32_t frameStartUs = nowUs;
      for (DmxPort& port : ports) {
        if (port.enabled) startFrame(port, frameStartUs);
      }
      waitingForNextFrame = false;
    }
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastDiagnosticMs >= 1000u) {
    printDiagnostics();
    lastDiagnosticMs = nowMs;
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }
}
