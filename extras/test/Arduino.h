
#ifndef _ARDUINO_H
#define _ARDUINO_H

#include <inttypes.h>
#include <cstring>

#define PROGMEM
#define F(str) (str)

unsigned long millis();

extern "C" {
char* ultoa(unsigned long value, char *string, int radix);
}

#endif