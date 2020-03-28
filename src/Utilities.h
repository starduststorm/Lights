
#include "Config.h"

#if DEBUG
#define assert(expr, reason) if (!(expr)) { logf("ASSERTION FAILED: %s", reason); while (1) delay(100); }
#else
#define assert(expr, reason) if (!(expr)) { logf("ASSERTION FAILED: %s", reason); }
#endif

struct Color;

void logf(const char *format, ...);

float PotentiometerReadf(int pin, float rangeMin, float rangeMax);
long PotentiometerRead(int pin, int rangeMin, int rangeMax);

void fast_srand();
int lsb_noise(int pin, int numbits);
unsigned int fast_rand(unsigned int minval, unsigned int maxval);
unsigned int fast_rand(unsigned int maxval);
#if DEBUG
int get_free_memory();
#endif

void PrintColor(Color c);

void logf(const char *format, ...);

int mod_wrap(int x, int m);
