#pragma once
#include <stdint.h>

inline uint8_t risingButtonEdges(uint8_t current, uint8_t &previous) {
  uint8_t rising = current & ~previous;
  previous = current;
  return rising;
}
