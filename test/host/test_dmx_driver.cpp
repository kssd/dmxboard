#include <cassert>
#include <cstdio>
#include "Arduino.h"
#include <avr/interrupt.h>
#include "DMXDriver.h"

volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
static unsigned long now = 100;
static int configuredPin = -1, configuredMode = -1;
static int writtenPin = -1, writtenValue = -1;
static int callbacks;

unsigned long millis() { return now; }
void pinMode(int pin, int mode) { configuredPin = pin; configuredMode = mode; }
void digitalWrite(int pin, int value) { writtenPin = pin; writtenValue = value; }
void USART_RX_vect();

// Internal state has no public getter; inspect it without changing firmware.
extern volatile unsigned int _dmxMaxChannel;
extern uint8_t *_dmxDataLastPtr;

static void onUpdate() {
  assert(UCSR0B == 0);
  assert(DMXDriver.dataUpdated());
  ++callbacks;
}

static void receiveByte(uint8_t value, bool framingError = false) {
  UCSR0A = framingError ? (1 << FE0) : 0;
  UDR0 = value;
  USART_RX_vect();
}

int main() {
  assert(DMXDriver.dataUpdated());
  DMXDriver.resetUpdated();
  assert(!DMXDriver.dataUpdated());

  uint8_t *buffer = DMXDriver.getBuffer();
  buffer[0] = 77;
  DMXDriver.write(-1, 11);
  assert(DMXDriver.read(1) == 11 && DMXDriver.read(0) == 11);
  DMXDriver.write(0, 12);
  assert(DMXDriver.read(-100) == 12);
  DMXDriver.write(DMX_CHANNELS_MAX + 1, 255);
  assert(DMXDriver.read(DMX_CHANNELS_MAX) == 255);
  assert(DMXDriver.read(DMX_CHANNELS_MAX + 100) == 255);
  DMXDriver.write(DMX_CHANNELS_MAX, 0);
  assert(DMXDriver.read(DMX_CHANNELS_MAX) == 0 && buffer[0] == 77);
  DMXDriver.write(42, 123);
  assert(buffer[42] == 123);
  buffer[42] = 45;
  assert(DMXDriver.read(42) == 45 && DMXDriver.getBuffer() == buffer);
  assert(!DMXDriver.dataUpdated());

  const int requested[] = {-1, 0, 1, 32, DMX_CHANNELS_MAX, DMX_CHANNELS_MAX + 1};
  const unsigned int expected[] = {1, 1, 1, 32, DMX_CHANNELS_MAX, DMX_CHANNELS_MAX};
  for (unsigned int i = 0; i < sizeof(requested) / sizeof(requested[0]); ++i) {
    DMXDriver.maxChannel(requested[i]);
    assert(_dmxMaxChannel == expected[i]);
    assert(_dmxDataLastPtr == buffer + expected[i]);
  }
  DMXDriver.maxChannel(1);
  DMXDriver.write(42, 45);
  assert(_dmxMaxChannel == 42 && _dmxDataLastPtr == buffer + 42);

  // Set the full range before init: this does not promise init clears beyond its prior limit.
  DMXDriver.maxChannel(DMX_CHANNELS_MAX);
  UCSR0A = UCSR0B = UCSR0C = UBRR0H = UBRR0L = 255;
  DMXDriver.init(SLAVE, 7);
  assert(configuredPin == 7 && configuredMode == OUTPUT);
  assert(writtenPin == 7 && writtenValue == LOW);
  const uint8_t receiveEnabled = (1 << RXEN0) | (1 << RXCIE0);
  assert(UCSR0B == receiveEnabled && UCSR0A == 0);
  assert(UBRR0H == 0 && UBRR0L == 3);
  assert(UCSR0C == ((1 << USBS0) | (3 << UCSZ00)));
  assert(_dmxMaxChannel == DMX_CHANNELS_MAX);
  assert(_dmxDataLastPtr == buffer + DMX_CHANNELS_MAX);
  for (int i = 0; i <= DMX_CHANNELS_MAX; ++i) assert(buffer[i] == 0);
  now += 25;
  assert(DMXDriver.noDataSince() == 25);

  // shortcut: only callback dispatch is checked; use hardware tests for receive alignment/timing.
  DMXDriver.attachOnUpdate(onUpdate);
  receiveByte(0, true);
  receiveByte(0);
  assert(DMXDriver.noDataSince() == 0);
  receiveByte(99);
  assert(callbacks == 1 && UCSR0B == receiveEnabled);
  assert(!DMXDriver.dataUpdated());
  receiveByte(0);
  assert(callbacks == 1);
  DMXDriver.attachOnUpdate(nullptr);
  receiveByte(88);
  assert(callbacks == 1 && !DMXDriver.dataUpdated());

  const uint8_t format = UCSR0C, baud = UBRR0L;
  DMXDriver.terminate();
  assert(UCSR0B == 0 && UCSR0C == format && UBRR0L == baud);
  std::puts("DMXDriver host tests passed");
}
