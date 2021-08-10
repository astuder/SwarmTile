#include "Arduino.h"
#include <sys/timeb.h>

unsigned long millis() {
    struct timeb now;
    ftime(&now);
    return 1000 * now.time + now.millitm;
}

extern "C" {
char* ultoa(unsigned long value, char *string, int radix)
{
  char tmp[33];
  char *tp = tmp;
  long i;
  unsigned long v = value;
  char *sp;

  if ( string == NULL )
  {
    return 0;
  }

  if (radix > 36 || radix <= 1)
  {
    return 0;
  }

  while (v || tp == tmp)
  {
    i = v % radix;
    v = v / radix;
    if (i < 10)
      *tp++ = i+'0';
    else
      *tp++ = i + 'a' - 10;
  }

  sp = string;


  while (tp > tmp)
    *sp++ = *--tp;
  *sp = 0;

  return string;
}
}