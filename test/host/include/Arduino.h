#pragma once

#include <stddef.h>
#include <stdint.h>

#define F_CPU 16000000UL
#define OUTPUT 1
#define HIGH 1
#define LOW 0

unsigned long millis();
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
