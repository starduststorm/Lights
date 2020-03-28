
#include "Config.h"
#include "Arduino.h"
#include "Color.h"
#include "Utilities.h"

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

 int lsb_noise(int pin, int numbits) {
  // TODO: Use Entropy.h? Probs not needed just to randomize pattern.
  int noise = 0;
  for (int i = 0; i < numbits; ++i) {
    int val = analogRead(pin);
    noise = (noise << 1) | (val & 1);
  }
  return noise;
}

void fast_srand()
{
  x += lsb_noise(A0, 8 * sizeof(unsigned long));
  y += lsb_noise(A1, 8 * sizeof(unsigned long));
  z += lsb_noise(A2, 8 * sizeof(unsigned long));
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

unsigned int fast_rand(unsigned int minval, unsigned int upperBound)
{
  unsigned int val = (unsigned int) ((((xorshift96() & 0xFFFF) * (upperBound-minval))>>16) + minval);
  return val;
}

unsigned int fast_rand(unsigned int upperBound)
{
  unsigned int val = (unsigned int) (((xorshift96() & 0xFFFF) * (upperBound))>>16);
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

static int vasprintf(char** strp, const char* fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  char tmp[1];
  int size = vsnprintf(tmp, 1, fmt, ap2);
  if (size <= 0) {
    strp=NULL;
    return size;
  }
  va_end(ap2);
  size += 1;
  *strp = (char*)malloc(size * sizeof(char));
  return vsnprintf(*strp, size, fmt, ap);
}

void logf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  char *buf;
  vasprintf(&buf, format, argptr);
  va_end(argptr);
  Serial.println(buf ? buf : "LOGF MEMORY ERROR");
#if DEBUG
  Serial.flush();
#endif
  free(buf);
}

int mod_wrap(int x, int m) {
  int result = x % m;
  return result < 0 ? result + m : result;
}

