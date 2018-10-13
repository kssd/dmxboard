#ifndef DMX_DRIVER_H
#define DMX_DRIVER_H

#include <inttypes.h>

#define DMX_CHANNELS_MAX 512
#define DMX_MODE_PIN 13 // HIGH/LOW -- OUT/IN

enum DMXDriverMode {
  UNKNOWN,
  SLAVE,
  SELECT,
  MASTER,
};

extern "C" {
typedef void (*dmxUpdateFn)(void);
}

class DMX512Driver {
public:
  void init(DMXDriverMode mode = SLAVE, int modePin = DMX_MODE_PIN);
  void maxChannel(int channels);
  void write(int channel, uint8_t data);
  uint8_t read(int channel);
  uint8_t *getBuffer();
  bool receive(uint8_t wait = 50);
  unsigned long noDataSince();
  void attachOnUpdate(dmxUpdateFn fn);
  bool dataUpdated();
  void resetUpdated();
  void terminate();
};

extern DMX512Driver DMXDriver;
#endif
