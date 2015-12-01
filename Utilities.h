

struct Color;

void logf(const char *format, ...);

float PotentiometerReadf(int pin, float rangeMin, float rangeMax);
long PotentiometerRead(int pin, int rangeMin, int rangeMax);

void fast_srand();
unsigned int fast_rand(unsigned int minval, unsigned int maxval);
unsigned int fast_rand(unsigned int maxval);
#if DEBUG
int get_free_memory();
#endif

#if SERIAL_LOGGING
void PrintColor(Color c);
#endif

void logf(const char *format, ...);

