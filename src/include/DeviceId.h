#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <EEPROM.h>
#include <inttypes.h>

#define DEVICE_ID_ADDRESS 0
#define DEVICE_ID_SEQ 2018
#define DEVICE_START_ADDRESS 1
#define DEVICE_ADDR_SPACE 6
#define DEVICE_ADDR_MARK 10

struct DeviceID {
  int seq;
  int addr;
  int addr_space;
  uint8_t marker;

  void clearAddressSpace() {
    for (int i = 0; i < EEPROM.length(); i++)
      EEPROM.write(i, 0);
  }

  void commit() { EEPROM.put(DEVICE_ID_ADDRESS, *this); }
  void update(const uint16_t a) { addr = a > 512 || a < 1 ? 1 : a; }
  void incr() { update(addr - 1); }
  void decr() { update(addr + 1); }
  void init() {
    DeviceID m_id;
    EEPROM.get(DEVICE_ID_ADDRESS, m_id);
    if (m_id.seq != DEVICE_ID_SEQ) {
      clearAddressSpace();
      m_id = {DEVICE_ID_SEQ, DEVICE_START_ADDRESS, DEVICE_ADDR_SPACE,
              DEVICE_ADDR_MARK};
      m_id.commit();
    }
    seq = m_id.seq;
    addr = m_id.addr;
    addr_space = m_id.addr_space;
    marker = m_id.marker;
  }
};

DeviceID ID;

#endif
