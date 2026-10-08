#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
using byte = uint8_t;
using boolean = bool;
extern unsigned long mqttClock;
inline unsigned long millis() { return mqttClock++; } // Deterministic advancing clock.
inline void yield() {}
inline uint8_t pgm_read_byte_near(const uint8_t *value) { return *value; }
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;
  virtual size_t write(const uint8_t *values, size_t count) {
    size_t written = 0; for (size_t i = 0; i < count; ++i) written += write(values[i]); return written;
  }
};
