#pragma once

#include <stddef.h>
#include <stdint.h>

// V3.4A shared RP2350 <-> ESP32-C5 transport definition.
// Physical exchanges are always exactly 544 bytes.
namespace ND_SPI {
static constexpr uint16_t MAGIC = 0x4E44;       // "DN" on the wire, little-endian
static constexpr uint8_t VERSION = 1;
static constexpr size_t HEADER_BYTES = 16;
static constexpr size_t MAX_PAYLOAD = 513;
static constexpr size_t CRC_BYTES = 4;
static constexpr size_t FRAME_BYTES = 544;
static constexpr uint32_t CLOCK_HZ = 1000000UL;

enum MessageType : uint8_t {
  SPI_MSG_NOP = 0,
  SPI_MSG_HELLO = 1,
  SPI_MSG_HELLO_ACK = 2,
  SPI_MSG_API_REQUEST = 3,
  SPI_MSG_API_RESPONSE = 4,
  SPI_MSG_STATUS_REQUEST = 5,
  SPI_MSG_STATUS_RESPONSE = 6,
  SPI_MSG_ARTNET_UNIVERSE = 7,
  SPI_MSG_SACN_UNIVERSE = 8,
  SPI_MSG_ACK = 9,
  SPI_MSG_ERROR = 10
};

static constexpr uint16_t FLAG_FIRST = 0x0001;
static constexpr uint16_t FLAG_MORE = 0x0002;
static constexpr uint16_t FLAG_LAST = 0x0004;
static constexpr uint16_t FLAG_HIGH_PRIORITY = 0x0008;

inline void put16(uint8_t *p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
inline void put32(uint8_t *p, uint32_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24); }
inline uint16_t get16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t get32(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

inline uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
  }
  return crc ^ 0xFFFFFFFFu;
}

inline void clear(uint8_t *frame) { for (size_t i = 0; i < FRAME_BYTES; ++i) frame[i] = 0; }
inline bool validLength(uint16_t length) { return length <= MAX_PAYLOAD && HEADER_BYTES + length + CRC_BYTES <= FRAME_BYTES; }

inline void build(uint8_t *frame, uint8_t type, uint32_t sequence, uint16_t flags,
                  uint32_t channel, const uint8_t *payload, uint16_t length) {
  clear(frame);
  if (!validLength(length)) length = 0;
  put16(frame, MAGIC); frame[2] = VERSION; frame[3] = type;
  put32(frame + 4, sequence); put16(frame + 8, length); put16(frame + 10, flags); put32(frame + 12, channel);
  if (payload && length) for (uint16_t i = 0; i < length; ++i) frame[HEADER_BYTES + i] = payload[i];
  put32(frame + HEADER_BYTES + length, crc32(frame, HEADER_BYTES + length));
}

inline bool validate(const uint8_t *frame, uint8_t *error = nullptr) {
  if (get16(frame) != MAGIC) { if (error) *error = 1; return false; }
  if (frame[2] != VERSION) { if (error) *error = 2; return false; }
  const uint16_t length = get16(frame + 8);
  if (!validLength(length)) { if (error) *error = 3; return false; }
  if (get32(frame + HEADER_BYTES + length) != crc32(frame, HEADER_BYTES + length)) { if (error) *error = 4; return false; }
  if (error) *error = 0;
  return true;
}
}

