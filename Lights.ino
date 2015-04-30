#include <SPI.h>
#include <TCL.h>

// Pulse delay controlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough


// -----------------------------------------
// Pattern Ideas:
// 
// A pattern that just fades to a color, and stays there for a few seconds. It's interesting to have the world be a solid color and stay still, too.
//     Bonus points if the fade got creative sometimes, and went pixel-at-a-time, 
//     or did a bi-directional wipe, flashed away (party mode), or some other cool effect.
// -----------------------------------------


/* Hardware Configuration */
#define ARDUINO_DUE 1
#define DEVELOPER_BOARD 0

/* For Developer Board */
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SOUND_DIAL TCL_POT4

/* Logging */
#define SERIAL_LOGGING 1
#define DEBUG 0

/* Options */
#define TEST_MODE (BMModeFollow)
#define TRANSITION_TIME (10)

/* Tools */
#define MIN(x, y) ((x) > (y) ? y : x)
#define MAX(x, y) ((x) < (y) ? y : x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MOD_DISTANCE(a, b, m) (abs(m / 2. - fmod((3 * m) / 2 + a - b, m)))

static const unsigned int LED_COUNT = 100;

#if DEVELOPER_BOARD
static const bool kHasDeveloperBoard = true;
#else
static const bool kHasDeveloperBoard = false;
#endif
#define FRAME_DURATION 20

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
// static unsigned long x= analogRead(A0)+123456789;
// static unsigned long y= analogRead(A1)+362436069;
// static unsigned long z= analogRead(A2)+521288629;

/** @ingroup random
Random number generator. A faster replacement for Arduino's random function,
which is too slow to use with Mozzi.  
Based on Marsaglia, George. (2003). Xorshift RNGs. http://www.jstatsoft.org/v08/i14/xorshift.pdf
@return a random 32 bit integer.
@todo check timing of xorshift96(), rand() and other PRNG candidates.
 */

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
  return (unsigned int) ((((xorshift96() & 0xFFFF) * (maxval-minval))>>16) + minval);
}

unsigned int fast_rand(unsigned int maxval)
{
  return (unsigned int) (((xorshift96() & 0xFFFF) * (maxval))>>16);
}

/* End Mozzi Random */

struct Color {
  byte red;
  byte green;
  byte blue;
  byte filler;
};
typedef struct Color Color;

static const Color kBlackColor   = (Color){.red=0,    .green=0,    .blue=0   };
static const Color kRedColor     = (Color){.red=0xFF, .green=0,    .blue=0   };
static const Color kOrangeColor  = (Color){.red=0xFF, .green=0x60, .blue=0   };
static const Color kYellowColor  = (Color){.red=0xFF, .green=0xFF, .blue=0   };
static const Color kGreenColor   = (Color){.red=0,    .green=0xFF, .blue=0   };
static const Color kCyanColor    = (Color){.red=0,    .green=0xFF, .blue=0xFF};
static const Color kBlueColor    = (Color){.red=0,    .green=0,    .blue=0xFF};
static const Color kVioletColor  = (Color){.red=0x8E, .green=0x25, .blue=0xFB};
static const Color kMagentaColor = (Color){.red=0xFF, .green=0,    .blue=0xFF};
static const Color kWhiteColor   = (Color){.red=0xFF, .green=0xFF, .blue=0xFF};

Color RGBRainbow[] = {kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor};
Color NamedRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kVioletColor, kMagentaColor};

extern int __bss_end;
extern void *__brkval;

#if DEBUG
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

static struct Color MakeColor(byte r, byte g, byte b)
{
  Color c;
  c.red = r;
  c.green = g;
  c.blue = b;
  return c;
}

static bool ColorIsEqualToColor(Color c1, Color c2)
{
  return (c1.red == c2.red && c1.green == c2.green && c1.blue == c2.blue);
}

// Transition and intensity are both in the range [0, 100]
static Color ColorWithInterpolatedColors(Color c1, Color c2, int transition, int intensity)
{
  // This is all integer math for speediness
  byte r, g, b;
  r = c1.red - transition * c1.red / 100 + transition * c2.red / 100;
  r = intensity * r / 100;
  g = c1.green - transition * c1.green / 100 + transition * c2.green / 100;
  g = intensity * g / 100;
  b = c1.blue - transition * c1.blue / 100 + transition * c2.blue / 100;
  b = intensity * b / 100;
  
  return MakeColor(r, g, b);
}

typedef enum {
  BMLightTransitionLinear = 0,
  BMLightTransitionEaseIn,
  BMLightTransitionEaseOut,
} BMLightTransitionCurve;

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
//  char *buf = (char *)calloc(strlen(format) + 200, sizeof(char));
  va_list argptr;
  va_start(argptr, format);
  char *buf = NULL;
  vasprintf(&buf, format, argptr);
//  vsnprintf(buf, 200, format, argptr); // don't have vasprintf
  va_end(argptr);
  Serial.println(buf);
  free(buf);
#endif
}

typedef enum {
  BMModeFollow = 0,
  BMModeWaves,
  BMModeOneBigWave,
  BMModeParity,
  BMModeCount,
  BMModeFire,
  BMModeBlueFire,
  BMModeLightningBugs,
  BMModeInterferingWaves,
  BMModeBoomResponder,
  BMModeBounce,
} BMMode;

struct DelayRange {
  unsigned int low;
  unsigned int high;
};
typedef struct DelayRange DelayRange;

static struct DelayRange MakeDelayRange(unsigned int low, unsigned int high)
{
  DelayRange r;
  r.low = low;
  r.high = high;
  return r;
}

static const unsigned int kMaxFrameDuration = 112;
static const unsigned int kMaxStandardFrameDuration = kMaxFrameDuration - 2;
static const unsigned int kMinStandardFrameDuration = 2;
static const DelayRange kDelayRangeStandard = MakeDelayRange(kMinStandardFrameDuration, kMaxStandardFrameDuration);

static DelayRange kModeRanges[BMModeCount] = {0};

class BMLight {
public:
  BMLight();
  
  Color color;
  Color targetColor;
  Color originalColor;
  
  float transitionProgress; // [0, 1]
  float transitionRate; // percentage to transition per frame
  BMLightTransitionCurve transitionCurve;
  
  void transitionToColor(Color transitionTargetColor, float rate, BMLightTransitionCurve curve);
  void transitionToColor(Color transitionTargetColor, float rate);
  void transitionTick(float multiplier);
  bool isTransitioning();
#if SERIAL_LOGGING
  void printDescription();
#endif
  
  int modeState; // For the Scene mode to use to store state
};

BMLight::BMLight() : transitionRate(0)
{
}

void BMLight::transitionToColor(Color transitionTargetColor, float rate, BMLightTransitionCurve curve)
{
  if (rate <= 0) {
    rate = 1;
  }
  targetColor = transitionTargetColor;
  originalColor = color;
  transitionRate = rate / 100.0;
  transitionProgress = 0;
  transitionCurve = curve;
}

void BMLight::transitionToColor(Color transitionTargetColor, float rate)
{
  transitionToColor(transitionTargetColor, rate, BMLightTransitionLinear);
}

void BMLight::transitionTick(float multiplier)
{
  if (transitionRate > 0) {
    transitionProgress = MIN(transitionProgress + multiplier * transitionRate, 1.0);
    float curvedTransitionProgress = transitionProgress;
    
    switch (transitionCurve) {
      case BMLightTransitionLinear:
        break;
      case BMLightTransitionEaseIn:
        curvedTransitionProgress *= curvedTransitionProgress;
        break;
      case BMLightTransitionEaseOut:
        curvedTransitionProgress = 1 - curvedTransitionProgress;
        curvedTransitionProgress *= curvedTransitionProgress;
        curvedTransitionProgress = 1 - curvedTransitionProgress;
        break;
    }
    
    color.red = originalColor.red + curvedTransitionProgress * (targetColor.red - originalColor.red);
    color.green = originalColor.green + curvedTransitionProgress * (targetColor.green - originalColor.green);
    color.blue = originalColor.blue + curvedTransitionProgress * (targetColor.blue - originalColor.blue);
    
    if (transitionProgress >= 1) {
      transitionRate = 0;
    }
  }
}

bool BMLight::isTransitioning()
{
  return (transitionRate != 0);
}

#if SERIAL_LOGGING
void BMLight::printDescription()
{
  char buf[20];
  snprintf(buf, 20, "%p", this);
  Serial.print("<Light ");
  Serial.print(buf);
  Serial.print(" isTransitioning = ");
  Serial.print(isTransitioning() ? "yes" : "no");
  Serial.print(">");
}
#endif

#pragma mark - 

class BMScene {
private:
  unsigned int _lightCount=0;
  BMMode _mode;
  unsigned long _modeStart=0;
  unsigned long _lastTick=0;
  unsigned long _lastFrame=0;
  
  BMLight **_lights;
  
  // Mode specific options
  float _frameDurationMultiplier;
  int _automaticColorsCount=0;
  
  // Mode specific data
  int _followLeader=0;
  float _smoothLeader=0;
  Color _followColor;
  unsigned int _followColorIndex=0;
  
  Color *_automaticColors;
  Color *_automaticColorsTargets;
  int *_automaticColorsProgress; // 0-100%
  
  float _transitionProgress=0;
  Color _targetColor;
  bool _directionIsReversed;
  unsigned int _soundPeak;
  
  
  void transitionAll(Color c, float rate);
  
  void updateStrand();
  DelayRange rangeForMode(BMMode mode);
  Color getAutomaticColor(unsigned int i);
public:
  void applyAll(Color c);
  void tick();
  BMScene(unsigned int ledCount);
  void setMode(BMMode mode);
  ~BMScene();
  BMMode randomMode();
  
  unsigned int frameDuration; // millisecond delay between frames
  float frameDurationFloat; // Needed to compare values to current potentiometer to ignore noise
};

#pragma mark - Private

float getBrightness()
{
  if (!kHasDeveloperBoard) {
    return 1.0;
  }
  static float brightnessAdjustment = 1.0;
  static int brightMin = 200;
  static int brightMax = 900;
  int val = analogRead(BRIGHTNESS_DIAL);
  if (val < brightMin) {
    brightMin = val;
  }
  if (val > brightMax) {
    brightMax = val;
  }
  
  static int lastChangeVal = -1.0;
  if (abs(val - lastChangeVal) > 40) {
    brightnessAdjustment = (val - brightMin) / (float)brightMax;
    lastChangeVal = val;
  }
  return brightnessAdjustment;
}

void BMScene::updateStrand()
{
  float brightnessAdjustment = getBrightness();
  
  float soundSensitivity = PotentiometerRead(SOUND_DIAL, 10, 204);
  bool useSound = soundSensitivity < 200;
  float soundMultiplier;
  if (useSound) {
    const int pin = 15;
    unsigned int soundReading = analogRead(pin);
    
    if (soundReading > _soundPeak) {
      _soundPeak = soundReading;
    } else {
      _soundPeak *= 0.9;
    }
    soundMultiplier = 0.5 + _soundPeak / soundSensitivity;
  } else {
    _soundPeak = 0;
  }
  
  TCL.sendEmptyFrame();
  for (int i = 0; i < _lightCount; ++i) {
    BMLight *light = _lights[i];
    float red = light->color.red, green = light->color.green, blue = light->color.blue;
    
    if (useSound) {
      red *= soundMultiplier;
      green *= soundMultiplier;
      blue *= soundMultiplier;
    }
    
    if (brightnessAdjustment < 0.95) {
      red *= brightnessAdjustment;
      green *= brightnessAdjustment;
      blue *= brightnessAdjustment;
    }
    
    TCL.sendColor(min(red, 255), min(green, 255), min(blue, 255));
  }
  TCL.sendEmptyFrame();
  logf("Sent a strand update!");
  Color color = _lights[_lightCount - 1]->color;
  logf("Last color was (%u, %u, %u)", color.red, color.green, color.blue);
}

#pragma mark - Convenience

void BMScene::applyAll(Color c)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i]->color = c;
  }
}

void BMScene::transitionAll(Color c, float rate)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionToColor(c, rate);
  }
}

#pragma mark - Public

BMScene::BMScene(unsigned int lightCount) : _mode((BMMode)-1), frameDuration(100)
{
  kModeRanges[BMModeFollow] = kDelayRangeStandard;
  kModeRanges[BMModeFire] = MakeDelayRange(50, kMaxStandardFrameDuration);
  kModeRanges[BMModeBlueFire] = MakeDelayRange(50, kMaxStandardFrameDuration);
  kModeRanges[BMModeLightningBugs] = MakeDelayRange(kMaxStandardFrameDuration, kMaxFrameDuration);
  kModeRanges[BMModeWaves] = kDelayRangeStandard;
  kModeRanges[BMModeOneBigWave] = MakeDelayRange(kMinStandardFrameDuration, 40);
  kModeRanges[BMModeInterferingWaves] = kDelayRangeStandard;
  kModeRanges[BMModeParity] = MakeDelayRange(0, kMaxStandardFrameDuration);
  
  _lightCount = lightCount;
  _lights = new BMLight*[_lightCount];
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i] = new BMLight();
  }
  _lastTick = millis();
  _lastFrame = _lastTick;
  
  applyAll(kBlackColor);
}

BMScene::~BMScene()
{
}

Color BMScene::getAutomaticColor(unsigned int i)
{
  return ColorWithInterpolatedColors(_automaticColors[i], _automaticColorsTargets[i], _automaticColorsProgress[i], 100);
}

DelayRange BMScene::rangeForMode(BMMode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    if (kModeRanges[mode].low == 0 && kModeRanges[mode].high == 0) {
      // It's just unset, treat it like middle speed
      return kDelayRangeStandard;
    }
    return kModeRanges[mode];
  }
  return kDelayRangeStandard;
}

static const Color kNightColor = MakeColor(0, 0, 0x10);
static BMScene *gLights;

BMMode BMScene::randomMode()
{
  int matchCount = 0;
  BMMode matchingModes[BMModeCount];
  for (int i = 0; i < BMModeCount; ++i) {
    BMMode mode = (BMMode)i;
    DelayRange range = rangeForMode(mode);
    if (frameDuration >= range.low && frameDuration <= range.high) {
      matchingModes[matchCount++] = mode;
    }
  }
  if (matchCount > 0) {
    return matchingModes[fast_rand(matchCount)];
  } else {
    // No matches. Pick any mode.
    return (BMMode)fast_rand(BMModeCount);
  }
}

void BMScene::setMode(BMMode mode)
{
  if (mode != _mode) {
    // Transition away from old mode
    switch (_mode) {
      case BMModeLightningBugs:
        // When ending lightning bugs, have all the bugs go out
        transitionAll(kNightColor, 10);
        break;
      default:
        break;
    }
    
    _mode = mode;
    _frameDurationMultiplier = 1;
    _soundPeak = 0;
    
    _automaticColorsCount = 0;
    free(_automaticColors);
    _automaticColors = NULL;
    free(_automaticColorsTargets);
    _automaticColorsTargets = NULL;
    free(_automaticColorsProgress);
    _automaticColorsProgress = NULL;
    
    // Initialize new mode
    for (int i = 0; i < _lightCount; ++i) {
      _lights[i]->modeState = 0;
    }
    
    switch (_mode) {
      case BMModeBounce:
      case BMModeFollow: {
        // When starting follow, there are sometimes single lights stuck on until the follow lead gets there. 
        // This keeps it from looking odd and too dark
        Color fillColor = ColorWithInterpolatedColors(RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))], kBlackColor, fast_rand(20, 60) / 10.0, 100);
        transitionAll(fillColor, 5);
        break;
      }
      case BMModeLightningBugs:
        transitionAll(kNightColor, 10);
        break;
      case BMModeInterferingWaves:
        _frameDurationMultiplier = 1 / 30.; // Interferring waves doesn't use light transitions fades, needs every tick to fade.
        _followLeader = _smoothLeader = 0;
        _automaticColorsCount = _lightCount / 10;
        break;
      case BMModeWaves:
      case BMModeOneBigWave: {
        _followLeader = 0;
        _targetColor = RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))];
        _frameDurationMultiplier = 2;
        _automaticColorsCount = 1;
        break;
      }
    }
    _modeStart = millis();
  }
  if (_automaticColorsCount > 0) {
    _automaticColors = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsTargets = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsProgress = (int *)malloc(_automaticColorsCount * sizeof(int));
    for (int i = 0; i < _automaticColorsCount; ++i) {
      _automaticColors[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsTargets[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsProgress[i] = 0;
    }
  }
  _directionIsReversed = (fast_rand(2) == 0);
}

void BMScene::tick()
{
  static bool allOff = false;
  if (kHasDeveloperBoard && digitalRead(TCL_SWITCH2) == LOW) {
    if (!allOff) {
      for (int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionToColor(kBlackColor, 3, BMLightTransitionEaseOut);
      }
      allOff = true;
    }
    if (_lights[0]->isTransitioning()) {
      for (int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionTick(1);
      }
      updateStrand();
    } else {
      // Just sleep after we're done fading
      delay(100);
    }
    return;
  } else {
    allOff = false;
  }
  
  unsigned long long time = millis();
  unsigned long long tickTime = time - _lastTick;
  unsigned long long frameTime = time - _lastFrame;
  
#if DEBUG && SERIAL_LOGGING
  static int tickCount = 0;
  static float avgTickTime = 0;
  avgTickTime = (avgTickTime * tickCount + tickTime) / (float)(tickCount + 1);
  tickCount++;
  if (tickCount > 100) {
    Serial.print("Avg tick time: ");
    Serial.print(avgTickTime);
    Serial.print(", ");
    Serial.print((int)(1000 / avgTickTime));
    Serial.println("fps");
    tickCount = 0;
  }
#endif
  
  if (frameTime > frameDuration * _frameDurationMultiplier) {
    switch (_mode) {
      case BMModeFollow: {
        logf("In ::tick for BMModeFollow");
        delay(1000);
        
        Color c = RGBRainbow[_followColorIndex];
        logf("color = (%i, %i, %i)", (int)c.red, (int)c.green, (int)c.blue);
        
        
        
        
        
        // FIXME: I have a stack smasher somewhere before the first time this is hit.
        
        
        
        
        
        logf("_followLeader = %i, _lights = %p", _followLeader, _lights);
        logf("_lights = %p, _followLeader = %i", _lights, _followLeader);
//        logf("_lights[_followLeader] = %p", _lights[_followLeader]);
//        _lights[_followLeader]->transitionToColor(RGBRainbow[_followColorIndex], 3);
        
//        _followLeader += (_directionIsReversed ? -1 : 1);
//        if (_followLeader < 0 || _followLeader >= _lightCount) {
//          _followLeader = (_followLeader + _lightCount) % _lightCount;
//          _followColorIndex = (_followColorIndex + 1) % ARRAY_SIZE(RGBRainbow);
//        }
        break;
      }
      
      case BMModeFire:
      case BMModeBlueFire: {
        // Interpolate, fade, and snap between two colors
        Color c1 = (_mode == BMModeFire ? MakeColor(0xFF, 0x30, 0) : MakeColor(0x30, 0x10, 0xFF));
        Color c2 = (_mode == BMModeFire ? MakeColor(0xFF, 0x80, 0) : MakeColor(0, 0xB0, 0xFF));
        for (int i = 0; i < _lightCount; ++i) {
          BMLight *light = _lights[i];
          if (!(light->isTransitioning())) {
            long choice = fast_rand(100);
            
            if (choice < 10) {
              // 10% of the time, fade slowly to black
              light->transitionToColor(kBlackColor, 20);
            } else {
              // Otherwise, fade or snap to another color
              Color color2 = (fast_rand(2) ? c1 : c2);
              if (choice < 95) {
                Color mixedColor = ColorWithInterpolatedColors(light->color, color2, fast_rand(101), fast_rand(101));
                light->transitionToColor(mixedColor, 30);
              } else {
                light->color = color2;
              }
            }
          }
        }
        break;
      }
      
      case BMModeLightningBugs: {
        for (int i = 0; i < _lightCount; ++i) {
          BMLight *light = _lights[i];
          if (!light->isTransitioning()) {
            switch (light->modeState) {
              case 1:
                // When putting a bug out, fade to black first, otherwise we fade from yellow(ish) to blue and go through white.
                light->transitionToColor(kBlackColor, 20);
                light->modeState = 2;
                break;
              case 2:
                light->transitionToColor(kNightColor, 20);
                light->modeState = 0;
                break;
              default:
                if (fast_rand(500) == 0) {
                  // Blinky blinky
                  light->transitionToColor(MakeColor(0xD0, 0xFF, 0), 30);
                  light->modeState = 1;
                }
                break;
            }
          }
        }
        break;
      }
      
      case BMModeBounce: {
        static int direction = 1;
        _lights[_followLeader]->transitionToColor(kBlackColor, 10);
        _followLeader = _followLeader + direction;
        _lights[_followLeader]->color = RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))];
        if (_followLeader == _lightCount - 1  || _followLeader == 0) {
          direction = -direction;
        }
        break;
      }
      
      case BMModeWaves:
      case BMModeOneBigWave: {
        const unsigned int waveLength = (_mode == BMModeWaves ? 15 : _lightCount);
        // Needs to fade out over less than half a wave, so there are some off in the middle.
        const float transitionRate = 80 / (waveLength / 2.0);
        
        Color waveColor = getAutomaticColor(0);
        for (int i = 0; i < _lightCount / waveLength; ++i) {
          unsigned int turnOnLeaderIndex = (_followLeader + i * waveLength) % _lightCount;
          unsigned int turnOffLeaderIndex = (_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount;
          _lights[turnOnLeaderIndex]->transitionToColor(waveColor, transitionRate, BMLightTransitionEaseIn);
          _lights[turnOffLeaderIndex]->transitionToColor(kBlackColor, transitionRate, BMLightTransitionEaseOut);
        }
        _followLeader += (_directionIsReversed ? -1 : 1);
        _followLeader = (_followLeader + _lightCount) % _lightCount;
        break;
      }
      
      case BMModeInterferingWaves: {
        // FIXME: It's jarring to shift in and out of this mode since it doesn't respect any transitions that were already in effect.
        // Idea: Have the first N passes use overlapping short transitions instead of setting the color.
        const int waveLength = 12;
        const float halfWave = waveLength / 2;
        
        Color automaticColors[_automaticColorsCount];
        float leaders[_automaticColorsCount];
        float halfColors = _automaticColorsCount / 2.0;
        float lightsChunk = _lightCount / (float)halfColors;
        for (int c = 0; c < _automaticColorsCount; ++c) {
          if (c < halfColors) {
            leaders[c] = _smoothLeader + c * lightsChunk;
          } else {
            leaders[c] = _lightCount - (_smoothLeader + c * lightsChunk);
          }
          automaticColors[c] = getAutomaticColor(c); // cache
        }
        
        for (int i = 0; i < _lightCount; ++i) {
          if (_lights[i]->isTransitioning()) {
            continue;
          }
          int nearColor1 = -1, nearColor2 = -1; // Find the two colors nearest to this light
          float nearDistance1 = 1000, nearDistance2 = 1000;
          for (int c = 0; c < _automaticColorsCount; ++c) {
            float distance = MOD_DISTANCE(i, leaders[c], _lightCount);
            if (distance < nearDistance1 && nearDistance2 <= nearDistance1) {
              nearColor1 = c;
              nearDistance1 = distance;
            } else if (distance < nearDistance2) {
              nearColor2 = c;
              nearDistance2 = distance;
            }
          }
          if (nearDistance1 < halfWave || nearDistance2 < halfWave) {
            Color color1 = (nearDistance1 < halfWave ? automaticColors[nearColor1] : automaticColors[nearColor2]);
            Color color2 = (nearDistance2 < halfWave ? automaticColors[nearColor2] : automaticColors[nearColor1]);
            
            nearDistance1 = MIN(nearDistance1, halfWave);
            nearDistance2 = MIN(nearDistance2, halfWave);
            Color c = ColorWithInterpolatedColors(color1, color2, 
                                                  (nearDistance1 / halfWave - nearDistance2 / halfWave) * 50 + 50, 
                                                  100 * (1 - (nearDistance1 + nearDistance2) / waveLength));
            // FIXME: Curve black intensity
//            c.red /= 255;
//            c.red *= c.red;
//            c.red *= 255;
//          
//            c.green /= 255;
//            c.green *= c.green;
//            c.green *= 255;
//        
//            c.blue /= 255;
//            c.blue *= c.blue;
//            c.blue *= 255;
            
            _lights[i]->color = c;
          } else {
            _lights[i]->color = kBlackColor;
          }
        }
        _smoothLeader = fmod(_smoothLeader + (20. / frameDuration), _lightCount);
        break;
     }
     
     case BMModeParity:
       if (!_lights[0]->isTransitioning()) {
         const int parityCount = (kHasDeveloperBoard ? PotentiometerRead(MODE_DIAL, 1, 5) : 2);
         Color colors[parityCount];
         for (int i = 0; i < parityCount; ++i) {
           colors[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
         }
         for (int i = 0; i < _lightCount; ++i) {
           int parity = i % parityCount;
           _lights[i]->transitionToColor(colors[parity], 2);
         }
       }
       break;
     
     case BMModeBoomResponder:
       for (int i = 0; i < _lightCount; ++i) {
         if (!_lights[i]->isTransitioning()) {
           _lights[i]->transitionToColor(NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))], 10);
         }
       }
       break;
      
      default: // Turn all off
        applyAll(kBlackColor);
        break;
    }
    _lastFrame = time;
  }
  
  // Fade transitions
  float transitionMultiplier = (tickTime / ((float)frameDuration * _frameDurationMultiplier));
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionTick(transitionMultiplier);
  }
  
  if (kHasDeveloperBoard && digitalRead(TCL_SWITCH1) == HIGH) {
    // Sound causes color changes
    // FIXME: This doesn't work. Automatic colors only controls the start of the transition.
//    for (int i = 0; i < _automaticColorsCount; ++i) {
//      _automaticColorsProgress[i] = 1;
//      _automaticColors[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
//    }
  } else {
    // Automatic colors
    for (int i = 0; i < _automaticColorsCount; ++i) {
      if (_automaticColorsProgress[i] == 0 || _automaticColorsProgress[i] >= 100) {
        _automaticColorsProgress[i] = 0;
        _automaticColors[i] = _automaticColorsTargets[i];
        _automaticColorsTargets[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
      }
      _automaticColorsProgress[i] += 1;
    }
  }
  
  updateStrand();
  _lastTick = time;
  
#ifndef TEST_MODE
  if (time - _modeStart > (unsigned long long)TRANSITION_TIME * 1000) {
    setMode(randomMode());
  } else {
#endif
    float newFrameDuration = (kHasDeveloperBoard ? PotentiometerReadf(TCL_POT2, 10, kMaxFrameDuration + 1) : FRAME_DURATION);
    if (abs(newFrameDuration - frameDurationFloat) > 0.9) {
      frameDuration = newFrameDuration;
      frameDurationFloat = newFrameDuration;
#ifndef TEST_MODE
      // Switch out of modes that are too slow or fast for the new frameDuration
      DelayRange range = rangeForMode(_mode);
      if (newFrameDuration < range.low || newFrameDuration > range.high) {
        setMode(randomMode());
      }
#ifndef TEST_MODE
    }
#endif
#endif
  }
  
  // This button reads as low state on the first loop for some reason, so start the flag as true to ignore the pres
  static bool button1Down = true;
  if (kHasDeveloperBoard && digitalRead(TCL_MOMENTARY1) == LOW) {
    if (!button1Down) {
      setMode((BMMode)((_mode + 1) % BMModeCount));
      button1Down = true;
    }
  } else {
    button1Down = false;
  }
}

void setup()
{
#if SERIAL_LOGGING
  int baud = 9600;
  Serial.begin(baud);
  logf("START");
//  Serial.println("START");
//  Serial.print(millis());
//  Serial.print(" millis: Serial logging started at ");
//  Serial.print(baud);
//  Serial.println(" baud");
  logf("%ul millis: Serial logging started at %i baud", millis(), baud);
#endif

#if ARDUINO_DUE
  // The Due is much faster, needs a higher clock divider to run the SPI at the right rate.
  // ATMega runs at clock div 2 for 4 MHz, the Due runs at 84 MHz, so needs clock div 42 for 4 MHz.
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(42);
#else
  TCL.begin();
#endif
  
#if DEVELOPER_BOARD
  TCL.setupDeveloperShield();
#endif
  
  gLights = new BMScene(LED_COUNT);
#ifdef TEST_MODE
  gLights->setMode(TEST_MODE);
#else
  gLights->setMode(gLights->randomMode());
#endif
  gLights->frameDuration = 100;
}

void loop()
{
  static int lead = 0;
  static int c = 0;
  Color color = NamedRainbow[c];
  TCL.sendEmptyFrame();
  for (int i = 0; i < 100; ++i) {
    float distance = ((lead - i + 100) % 100) / 100.0;
    
    float red = color.red * (1 - distance);
    float green = color.green *  (1 - distance);
    float blue = color.blue * (1 - distance);
    
    TCL.sendColor(min(red, 255), min(green, 255), min(blue, 255));
  }
  TCL.sendEmptyFrame();
  lead++;
  if (lead >= 100) {
    lead = 0;
    Color newColor = NamedRainbow[c];
    c = (c + 1) % ARRAY_SIZE(NamedRainbow);
    
    logf("NEXT COLOR index %i/%i = (%i, %i, %i)", (int)(c + 1), (int)ARRAY_SIZE(NamedRainbow), (int)newColor.red, (int)newColor.green, (int)newColor.blue);
    logf("Red = (%i, %i, %i)", (int)kRedColor.red, (int)kRedColor.green, (int)kRedColor.blue);
    logf("sizeof(Color) = %i", (int)sizeof(Color));
  }
  
  return;
  
  
  
#if SERIAL_LOGGING
  static int loopCount2 = 0;
  if (loopCount2 % 100 == 0)
    logf("loop #%i", loopCount2);
  loopCount2++;
#endif
  
#if DEBUG && SERIAL_LOGGING
  static int loopCount = 0;
  if (loopCount++ > 1000) {
    Serial.print("Memory free: ");
    Serial.print(get_free_memory());
    Serial.println(" bytes");
    loopCount = 0;
  }
#endif
  
  gLights->tick();
  logf("got to end of tick");
}

