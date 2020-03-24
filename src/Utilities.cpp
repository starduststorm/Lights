
#include "Config.h"
#include "Arduino.h"
#include "Color.h"

void logf(const char *format, ...);

float PotentiometerReadf(int pin, float rangeMin, float rangeMax)
{
  // Potentiometer has range [0, 1023]
  return analogRead(pin) / 1023.0 * (rangeMax - rangeMin) + rangeMin;
}

long PotentiometerRead(int pin, int rangeMin, int rangeMax)
{
  // Potentiometer has range [0, 1023]
  return round(analogRead(pin) / 1023.0 * (rangeMax - rangeMin) + rangeMin);
}

/* Begin Mozzi Random */

// moved these out of xorshift96() so xorshift96() can be reseeded manually
static unsigned long x = 132456789, y = 362436069, z = 521288629;

/** @ingroup random
Random number generator. A faster replacement for Arduino's random function,
which is too slow to use with Mozzi.  
Based on Marsaglia, George. (2003). Xorshift RNGs. http://www.jstatsoft.org/v08/i14/xorshift.pdf
@return a random 32 bit integer.
@todo check timing of xorshift96(), rand() and other PRNG candidates.
 */

void fast_srand()
{
  x += analogRead(A0);
  y += analogRead(A1);
  z += analogRead(A2);
}

unsigned long xorshift96()
{ //period 2^96-1
    // static unsigned long x=123456789, y=362436069, z=521288629;
    unsigned long t;

    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
}

unsigned int fast_rand(unsigned int minval, unsigned int maxval)
{
  unsigned int val = (unsigned int) ((((xorshift96() & 0xFFFF) * (maxval-minval))>>16) + minval);
  return val;
}

unsigned int fast_rand(unsigned int maxval)
{
  unsigned int val = (unsigned int) (((xorshift96() & 0xFFFF) * (maxval))>>16);
  return val;
}

/* End Mozzi Random */

extern int __bss_end;
extern void *__brkval;

#if DEBUG && !ARDUINO_DUE
int get_free_memory()
{
  int free_memory;

  if((int)__brkval == 0)
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int)__brkval);

  return free_memory;
}
#endif

#if SERIAL_LOGGING
void PrintColor(Color c)
{
  Serial.print("(");
  Serial.print(c.red);
  Serial.print(", ");
  Serial.print(c.green);
  Serial.print(", ");
  Serial.print(c.blue);
  Serial.print(")");
}
#endif

void logf(const char *format, ...)
{
#if SERIAL_LOGGING
  va_list argptr;
  va_start(argptr, format);
#if ARDUINO_DUE
  char *buf = NULL;
  vasprintf(&buf, format, argptr);
#else
  char *buf = (char *)calloc(strlen(format) + 200, sizeof(char));
  vsnprintf(buf, 200, format, argptr); // Uno doesn't have vasprintf
#endif
  va_end(argptr);
  Serial.println(buf);
  free(buf);
#endif
}
