#include <avr/interrupt.h>

#include "Arduino.h"
#include "DMXDriver.h"

#if !defined(DMX_USE_PORT1) && defined(USART_RX_vect)
#define UCSRnA UCSR0A
#define RXCn RXC0
#define TXCn TXC0
#define UCSRnB UCSR0B
#define RXCIEn RXCIE0
#define TXCIEn TXCIE0
#define UDRIEn UDRIE0
#define RXENn RXEN0
#define TXENn TXEN0
#define UCSRnC UCSR0C
#define USBSn USBS0
#define UCSZn0 UCSZ00
#define UPMn0 UPM00
#define UBRRnH UBRR0H
#define UBRRnL UBRR0L
#define UDRn UDR0
#define UDREn UDRE0
#define FEn FE0
#define USARTn_RX_vect USART_RX_vect
#define USARTn_TX_vect USART_TX_vect
#define USARTn_UDRE_vect USART_UDRE_vect
#else
#error "ONLY ATMega3268 Supported"
#endif

#define SERIAL_8N1 ((0 << USBSn) | (0 << UPMn0) | (3 << UCSZn0))
#define SERIAL_8N2 ((1 << USBSn) | (0 << UPMn0) | (3 << UCSZn0))
#define SERIAL_8E1 ((0 << USBSn) | (2 << UPMn0) | (3 << UCSZn0))
#define SERIAL_8E2 ((1 << USBSn) | (2 << UPMn0) | (3 << UCSZn0))

#define BREAKSPEED 100000
#define BREAKFORMAT SERIAL_8E1
#define DMXSPEED 250000
#define DMXFORMAT SERIAL_8N2

#define _calc_prescaler(B) (((((F_CPU) / 8) / (B)) - 1) / 2)

typedef enum { IDLE, BREAK, DATA } RcvState;

volatile uint8_t _dmxRcvState;
volatile int _dmxChannel;

volatile unsigned int _dmxMaxChannel = 32;
/**
 * time (millis) for the last recevied dmx packet
 **/
volatile unsigned long _dmxLastPacket = 0;

bool _dmxUpdated = true;
volatile dmxUpdateFn _dmxOnUpdateFn = NULL;

uint8_t _dmxData[DMX_CHANNELS_MAX + 1];
uint8_t *_dmxDataPtr;
uint8_t *_dmxDataLastPtr;

void _driver_baud(uint16_t baud_setting, uint8_t format);
void _driver_write(uint8_t data);

DMXDriverMode _dmxMode;
int _dmxModePin;

DMX512Driver DMXDriver;
void DMX512Driver::init(DMXDriverMode mode, int modePin) {
  _dmxMode = UNKNOWN;
  _dmxModePin = modePin;
  _dmxRcvState = IDLE;
  _dmxChannel = 0;
  _dmxDataPtr = _dmxData;
  _dmxLastPacket = millis();

  for (unsigned int i = 0; i <= _dmxMaxChannel; i++)
    _dmxData[i] = 0;

  _dmxMode = mode;

  _dmxMaxChannel = DMX_CHANNELS_MAX;
  _dmxDataLastPtr = _dmxData + _dmxMaxChannel;

  switch (_dmxMode) {
  case MASTER:
    pinMode(_dmxModePin, OUTPUT);
    digitalWrite(_dmxModePin, HIGH);

    UCSRnB = (1 << TXENn) | (1 << TXCIEn);
    _driver_baud(_calc_prescaler(BREAKSPEED), BREAKFORMAT);
    _driver_write((uint8_t)0);
    _dmxMaxChannel = 32;
    break;
  case SLAVE:
    pinMode(_dmxModePin, OUTPUT);
    digitalWrite(_dmxModePin, LOW);

    UCSRnB = (1 << RXENn) | (1 << RXCIEn);
    _driver_baud(_calc_prescaler(DMXSPEED), DMXFORMAT);
    _dmxMaxChannel = DMX_CHANNELS_MAX;
    _dmxDataLastPtr = _dmxData + _dmxMaxChannel;
    break;
  default:
    break;
  }
}

void DMX512Driver::maxChannel(int channels) {
  if (channels < 1)
    channels = 1;
  if (channels > DMX_CHANNELS_MAX)
    channels = DMX_CHANNELS_MAX;
  _dmxMaxChannel = channels;
  _dmxDataLastPtr = _dmxData + channels;
}

uint8_t DMX512Driver::read(int channel) {
  if (channel < 1)
    channel = 1;
  if (channel > DMX_CHANNELS_MAX)
    channel = DMX_CHANNELS_MAX;
  return _dmxData[channel];
}

void DMX512Driver::write(int channel, uint8_t data) {
  if (channel < 1)
    channel = 1;
  else if (channel > DMX_CHANNELS_MAX)
    channel = DMX_CHANNELS_MAX;
  if (data < 0)
    data = 0;
  else if (data > 255)
    data = 255;

  _dmxData[channel] = data;

  if (channel > _dmxMaxChannel) {
    _dmxMaxChannel = channel;
    _dmxDataLastPtr = _dmxData + _dmxMaxChannel;
  }
}

uint8_t *DMX512Driver::getBuffer() { return _dmxData; }

unsigned long DMX512Driver::noDataSince() {
  unsigned long now = millis();
  return (now - _dmxLastPacket);
}

void DMX512Driver::attachOnUpdate(dmxUpdateFn fn) { _dmxOnUpdateFn = fn; }

bool DMX512Driver::dataUpdated() { return _dmxUpdated; }

void DMX512Driver::resetUpdated() { _dmxUpdated = false; }

void DMX512Driver::terminate() { UCSRnB = 0; }

void _driver_baud(uint16_t baud, uint8_t format) {
  UCSRnA = 0;
  UBRRnH = baud >> 8;
  UBRRnL = baud;
  UCSRnC = format;
}

void _driver_write(uint8_t data) { UDRn = data; }

ISR(USARTn_RX_vect) {
  uint8_t USARTstate = UCSRnA;
  uint8_t data = UDRn;
  uint8_t state = _dmxRcvState;

  if (USARTstate & (1 << FEn)) {
    _dmxRcvState = BREAK;
    _dmxDataPtr = _dmxData;
  } else if (state == BREAK) {
    if (data == 0) {
      _dmxRcvState = DATA;
      _dmxLastPacket = millis();
      _dmxDataPtr++;
    } else {
      _dmxRcvState = IDLE;
    }
  } else if (state == DATA) {
    if (data != *_dmxDataPtr) {
      _dmxUpdated = true;
      *_dmxDataPtr = data;
    }

    if (_dmxDataPtr == _dmxDataLastPtr)
      // FIXME (kssd) when channel publishes all 512 channels with update
      // { //-- uncomment for fix
      _dmxRcvState = IDLE;

    if ((_dmxUpdated) && (_dmxOnUpdateFn)) {
      UCSRnB = 0;
      _dmxOnUpdateFn();
      UCSRnB = (1 << RXENn) | (1 << RXCIEn);
      _driver_baud(_calc_prescaler(DMXSPEED), DMXFORMAT);
    }
    // } // -- uncomment for fix
    _dmxUpdated = false;
  }
  _dmxDataPtr++;
}

ISR(USARTn_TX_vect) {
  if (_dmxMode == MASTER && _dmxChannel == -1) {
    // data transmitted (resume break)
    _driver_baud(_calc_prescaler(BREAKSPEED), BREAKFORMAT);
    _driver_write((uint8_t)0);
    _dmxChannel = 0;
  } else if (_dmxChannel == 0) {
    // Break sent
    _driver_baud(_calc_prescaler(DMXSPEED), DMXFORMAT);
    UCSRnB = (1 << TXENn) | (1 << UDRIEn);
    _driver_write((uint8_t)0);
    _dmxChannel = 1;
  }
}

ISR(USARTn_UDRE_vect) {
  _driver_write(_dmxData[_dmxChannel++]);
  if (_dmxChannel > _dmxMaxChannel) {
    _dmxChannel = -1;
    UCSRnB = (1 << TXENn) | (1 << TXCIEn);
  }
}
