#include "DMXDriver.h"
#include "TM1637Display.h"
#include "include/DeviceId.h"
#include "include/ButtonEdges.h"
#include <Arduino.h>

#define DISPLAY_BRIGHTNESS 4
#define DISPLAY_CLOCK 7
#define DISPLAY_DATA 6

#define DEVICE_MODE_PIN 13
/**
 * SET uses an external interrupt on Arduino Mega.
 **/
#define ST_RST_BTN_PN 3

/**
 * DONOT Change following input pins.
 **/
#define INC_BTN_PN 4
#define DEC_BTN_PN 5

enum DeviceState { START, OP, SET, CONFIG, RESET };
enum DeviceEvent { NOUNCE_E, INCR_E, DECR_E, SET_E, INT_E };

volatile DeviceEvent event;
volatile DeviceState mode;
volatile bool updated;
volatile bool displayState;

TM1637Display display(DISPLAY_CLOCK, DISPLAY_DATA);
volatile long lastDisplayToggle = millis();
volatile long lastSet = millis();

template <typename F> void next(DeviceEvent e, DeviceState to, F onExit) {
  if (e != INT_E && event != e)
    return;
  updated = (mode != to);
  mode = to;
  onExit();
}

template <typename F> void on(F onEnter) {
  if (!updated)
    return;
  onEnter();
  updated = false;
}

template <typename F> void in(F exec) {
  if (!updated)
    exec();
}

template <typename F> void inIf(bool grd, F exec) {
  if (!updated && grd)
    exec();
}

void updateDisplay() {
  display.setBrightness(DISPLAY_BRIGHTNESS, displayState);
  uint8_t m = display.encodeDigit(10);
  display.setSegments(&m, 1, 0);
  display.showNumberDec(ID.addr, true, 3, 1);
}

void toggleDisplay() {
  if ((millis() - lastDisplayToggle) < 400l)
    return;
  displayState = !displayState;
  lastDisplayToggle = millis();
  updateDisplay();
}

const uint8_t CTRL_CHANNELS[] = {A0, A1, A2, A3, A4, A5};

void channelClear() {
  for (int i = 0; i < DEVICE_ADDR_SPACE; i++) {
    pinMode(CTRL_CHANNELS[i], OUTPUT);
    analogWrite(CTRL_CHANNELS[i], 128);
  }
}

void updateChannels() {
  for (int i = ID.addr; i < (ID.addr + ID.addr_space); i++) {
    int addr = i - ID.addr;
    analogWrite(CTRL_CHANNELS[addr], i <= 512 ? DMXDriver.read(i) : 0);
  }
}

uint8_t previousButtons;

uint8_t readButtons() {
  return (digitalRead(INC_BTN_PN) == HIGH ? 1 : 0) |
         (digitalRead(DEC_BTN_PN) == HIGH ? 2 : 0);
}

void pollButtons() {
  // shortcut: polling misses sub-loop pulses; use interrupt-capable pins if needed.
  uint8_t rising = risingButtonEdges(readButtons(), previousButtons);
  if (event == SET_E)
    return;
  if (rising & 1)
    event = INCR_E;
  else if (rising & 2)
    event = DECR_E;
}

// TODO(kssd) add debounce if requried.
void onSet() { event = SET_E; }

void setup() {
  channelClear();
  pinMode(ST_RST_BTN_PN, INPUT);

  mode = START;
  updated = true;
  event = NOUNCE_E;
}

void loop() {
  switch (mode) {
  case START:
    on([] {
      ID.init();
      displayState = true;
      detachInterrupt(digitalPinToInterrupt(ST_RST_BTN_PN));
    });

    next(INT_E, OP, [] {
      updateDisplay();
      DMXDriver.init(SLAVE, DEVICE_MODE_PIN);
      attachInterrupt(digitalPinToInterrupt(ST_RST_BTN_PN), onSet, RISING);
    });
    break;
  case OP:
    on([] { DMXDriver.attachOnUpdate(updateChannels); });
    in([] {
      displayState = true;
      updateDisplay();
    });
    next(SET_E, CONFIG, [] { DMXDriver.terminate(); });
    break;
  case CONFIG:
    on([] {
      pinMode(INC_BTN_PN, INPUT);
      pinMode(DEC_BTN_PN, INPUT);
      previousButtons = readButtons();
    });
    pollButtons();
    next(SET_E, SET, [] {});

    next(INCR_E, CONFIG, [] {
      ID.incr();
      displayState = false;
    });
    next(DECR_E, CONFIG, [] {
      ID.decr();
      displayState = false;
    });
    in([] { toggleDisplay(); });
    break;
  case SET:
    on([] { ID.commit(); });
    next(INT_E, START, [] {});
    break;
  default:
    next(INT_E, START, [] { ID.clearAddressSpace(); });
    break;
  }

  event = NOUNCE_E;
}
