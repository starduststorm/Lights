#include <SPI.h>
#include <TCL.h>

// Pulse delay controlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough

#define SERIAL_LOGGING 0
#define DEBUG 0
#define TRANSITION_TIME (30)
#define TEST_MODE (BMModeParity)

#define MIN(x, y) ((x) > (y) ? y : x)
#define MAX(x, y) ((x) < (y) ? y : x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MOD_DISTANCE(a, b, m) (abs(m / 2. - fmod((3 * m) / 2 + a - b, m)))

#define MODE_DIAL TCL_POT1

static const unsigned int LED_COUNT = 150;
 
long PotentiometerRead(int pin, int rangeMin, int rangeMax)
{
  // Potentiometer has range [0, 1023]
  return round(analogRead(pin) / 1023.0 * (rangeMax - rangeMin) + rangeMin);
}

struct Color {
  byte red;
  byte green;
  byte blue;
};
typedef struct Color Color;

static const Color kBlackColor = (Color){0, 0, 0};
static const Color kRedColor = (Color){0xFF, 0, 0};
static const Color kOrangeColor = (Color){0xFF, 0x60, 0x0};
static const Color kYellowColor = (Color){0xFF, 0xFF, 0};
static const Color kGreenColor = (Color){0, 0xFF, 0};
static const Color kCyanColor = (Color){0, 0xFF, 0xFF};
static const Color kBlueColor = (Color){0, 0, 0xFF};
static const Color kVioletColor = (Color){0x8E, 0x25, 0xFB};
static const Color kMagentaColor = (Color){0xFF, 0, 0xFF};
static const Color kWhiteColor = (Color){0xFF, 0xFF, 0xFF};

Color RGBRainbow[] = {kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor};
Color NamedRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kVioletColor, kMagentaColor, kWhiteColor};

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

// Transition and intensity are both in the range [0, 1]
static Color ColorWithInterpolatedColors(Color c1, Color c2, float transition, float intensity)
{
  byte r, g, b;
  r = (((float)c2.red - c1.red) * transition + c1.red) * intensity;
  g = (((float)c2.green - c1.green) * transition + c1.green) * intensity;
  b = (((float)c2.blue - c1.blue) * transition + c1.blue) * intensity;
  
  return MakeColor(r, g, b);
}

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

typedef enum {
  BMModeFollow = 0,
  BMModeFire,
  BMModeBlueFire,
  BMModeLightningBugs,
  BMModeWaves,
  BMModeInterferingWaves,
  BMModeParity,
  BMModeCount,
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
static const DelayRange kDelayRangeAll = (DelayRange){0, 1000};

static DelayRange kModeRanges[BMModeCount] = {0};

class BMLight {
public:
  BMLight();
  
  Color color;
  Color targetColor;
  Color originalColor;
  
  float transitionProgress; // [0, 1]
  float transitionRate; // percentage to transition per frame
  
  void transitionToColor(Color targetColor, float rate);
  void transitionTick(unsigned long millis, unsigned int frameDuration);
  bool isTransitioning();
#if SERIAL_LOGGING
  void printDescription();
#endif
  
  int modeState; // For the Scene mode to use to store state
};

BMLight::BMLight() : transitionRate(0)
{
}

void BMLight::transitionToColor(Color transitionTargetColor, float rate)
{
  if (rate <= 0) {
    rate = 1;
  }
  targetColor = transitionTargetColor;
  originalColor = color;
  transitionRate = rate;
  transitionProgress = 0;
}

void BMLight::transitionTick(unsigned long millis, unsigned int frameDuration)
{
  if (transitionRate > 0 && millis > 0) {
    transitionProgress = MIN(transitionProgress + (millis / (float)frameDuration) * transitionRate / 100, 1.0);
    
    color.red = originalColor.red + transitionProgress * (targetColor.red - originalColor.red);
    color.green = originalColor.green + transitionProgress * (targetColor.green - originalColor.green);
    color.blue = originalColor.blue + transitionProgress * (targetColor.blue - originalColor.blue);
    
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
  sprintf(buf, "%p", this);
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
  unsigned int _lightCount;
  BMMode _mode;
  unsigned long _modeStart;
  unsigned long _lastTick;
  unsigned long _lastFrame;
  
  BMLight **_lights;
  
  // Mode specific options
  float _frameDurationMultiplier;
  bool _curveBlackIntensity;
  int _automaticColorsCount;
  
  // Mode specific data
  int _followLeader;
  float _smoothLeader;
  Color _followColor;
  unsigned int _followColorIndex;
  
  Color *_automaticColors;
  Color *_automaticColorsTargets;
  float *_automaticColorsProgress;
  
  float _transitionProgress;
  Color _targetColor;
  
  void applyAll(Color c);
  void transitionAll(Color c, float rate);
  
  void updateStrand();
  DelayRange rangeForMode(BMMode mode);
  Color getAutomaticColor(unsigned int i);
public:
  void tick();
  BMScene(unsigned int ledCount);
  void setMode(BMMode mode);
  ~BMScene();
  BMMode randomMode();
  
  unsigned int frameDuration; // millisecond delay between frames
};

#pragma mark - Private

void BMScene::updateStrand()
{
  TCL.sendEmptyFrame();
  for (int i = 0; i < _lightCount; ++i) {
    BMLight *light = _lights[i];
    float red = light->color.red, green = light->color.green, blue = light->color.blue;
    
    if (_curveBlackIntensity) {
      // Curve light intensity along a parabola. This helps fades to and from black appear more clearly for some patterns
      red /= 255;
      red *= red;
      red *= 255;
    
      green /= 255;
      green *= green;
      green *= 255;
  
      blue /= 255;
      blue *= blue;
      blue *= 255;
    }
    TCL.sendColor(red, green, blue);
  }
  TCL.sendEmptyFrame();
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
  kModeRanges[BMModeFollow] = kDelayRangeAll;
  kModeRanges[BMModeFire] = MakeDelayRange(50, 200);
  kModeRanges[BMModeBlueFire] = MakeDelayRange(50, 200);
  kModeRanges[BMModeLightningBugs] = MakeDelayRange(40, 200);
  kModeRanges[BMModeWaves] = kDelayRangeAll;
  kModeRanges[BMModeInterferingWaves] = kDelayRangeAll;
  
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
  return ColorWithInterpolatedColors(_automaticColors[i], _automaticColorsTargets[i], _automaticColorsProgress[i], 1);
}

DelayRange BMScene::rangeForMode(BMMode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    if (kModeRanges[mode].low == 0 && kModeRanges[mode].high == 0) {
      // It's just unset, treat it like "all"
      return kDelayRangeAll;
    }
    return kModeRanges[mode];
  }
  return kDelayRangeAll;
}

static const Color kNightColor = MakeColor(0, 0, 0x10);
static BMScene *gLights;

BMMode BMScene::randomMode()
{
  BMMode newMode;
  while (1) {
    newMode = (BMMode)random(BMModeCount);
    DelayRange range = rangeForMode(newMode);
    if (frameDuration >= range.low && frameDuration <= range.high) {
      break;
    }
  }
  return newMode;
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
    }
    
    _mode = mode;
    _curveBlackIntensity = false;
    _frameDurationMultiplier = 1;
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
        Color fillColor = ColorWithInterpolatedColors(RGBRainbow[random(ARRAY_SIZE(RGBRainbow))], kBlackColor, random(2, 6) / 10.0, 1);
        transitionAll(fillColor, 5);
        break;
      }
      case BMModeLightningBugs:
        transitionAll(kNightColor, 10);
        break;
      case BMModeInterferingWaves:
        _frameDurationMultiplier = 1 / 30.; // Interferring waves doesn't use light transitions fades, needs every tick to fade.
        _followLeader = _smoothLeader = 0;
        _curveBlackIntensity = true;
        _automaticColorsCount = _lightCount / 10;
        break;
      case BMModeWaves:
        _curveBlackIntensity = true;
        _followLeader = 0;
        _targetColor = RGBRainbow[random(ARRAY_SIZE(RGBRainbow))];
        _transitionProgress = 0;
        _frameDurationMultiplier = 2;
        break;
    }
    _modeStart = millis();
  }
  if (_automaticColorsCount > 0) {
    _automaticColors = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsTargets = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsProgress = (float *)malloc(_automaticColorsCount * sizeof(float));
    for (int i = 0; i < _automaticColorsCount; ++i) {
      _automaticColors[i] = NamedRainbow[random(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsTargets[i] = NamedRainbow[random(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsProgress[i] = 0;
    }
  }
}

void BMScene::tick()
{
  static bool allOff = false;
  if (digitalRead(TCL_SWITCH2) == LOW) {
    if (!allOff) {
      for (int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionToColor(kBlackColor, 3);
      }
      allOff = true;
    }
    if (!_lights[0]->isTransitioning()) {
      // Just sleep after we're done fading
      delay(100);
    }
  } else {
    allOff = false;
  }
  
  unsigned long time = millis();
  unsigned long tickTime = time - _lastTick;
  unsigned long frameTime = time - _lastFrame;
  
#if DEBUG && SERIAL_LOGGING
  static int tickCount = 0;
  static float avgTickTime = 0;
  avgTickTime = (avgTickTime * tickCount + tickTime) / (float)(tickCount + 1);
  tickCount++;
  if (tickCount > 100) {
    Serial.print("Avg tick time: ");
    Serial.println(avgTickTime);
    tickCount = 0;
  }
#endif
  
  if (!allOff && frameTime > frameDuration * _frameDurationMultiplier) {
    switch (_mode) {
      case BMModeFollow: {
        _lights[_followLeader]->transitionToColor(RGBRainbow[_followColorIndex], 5);
        _followLeader = (_followLeader + 1);
        if (_followLeader >= _lightCount) {
          _followLeader = _followLeader % _lightCount;
          _followColorIndex = (_followColorIndex + 1) % ARRAY_SIZE(RGBRainbow);
        }
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
            long choice = random(100);
            
            if (choice < 10) {
              // 10% of the time, fade slowly to black
              light->transitionToColor(kBlackColor, 20);
            } else {
              // Otherwise, fade or snap to another color
              Color color2 = (random(2) ? c1 : c2);
              if (choice < 95) {
                Color mixedColor = ColorWithInterpolatedColors(light->color, color2, random(101) / 100.0, random(101) / 100.0);
                light->transitionToColor(mixedColor, 40);
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
                if (random(200) == 0) {
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
        _curveBlackIntensity = true;
        static int direction = 1;
        _lights[_followLeader]->transitionToColor(kBlackColor, 10);
        _followLeader = _followLeader + direction;
        _lights[_followLeader]->color = RGBRainbow[random(ARRAY_SIZE(RGBRainbow))];
        if (_followLeader == _lightCount - 1  || _followLeader == 0) {
          direction = -direction;
        }
        break;
      }
      
      case BMModeWaves: {
        const unsigned int waveLength = PotentiometerRead(MODE_DIAL, 8, 20);
        const int transitionRate = 100 / (_lightCount / waveLength);
        
        // FIXME: Have this 1 automatic color instead.
        
        if (_transitionProgress == 0 || _transitionProgress >= 1) {
          _transitionProgress = 0;
          _followColor = _targetColor;
          _targetColor = NamedRainbow[random(ARRAY_SIZE(NamedRainbow))];
        }
        _transitionProgress += 0.01;
        Color waveColor = ColorWithInterpolatedColors(_followColor, _targetColor, _transitionProgress, 1);
        for (int i = 0; i < _lightCount / waveLength; ++i) {
          _lights[(_followLeader + i * waveLength) % _lightCount]->transitionToColor(waveColor, transitionRate);
          _lights[(_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount]->transitionToColor(kBlackColor, transitionRate);
        }
        _followLeader = (_followLeader + 1) % _lightCount;
        break;
      }
      
      case BMModeInterferingWaves: {
        // FIXME: It's jarring to shift in and out of this mode since it doesn't respect any transitions that were already in effect.
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
            Color c = ColorWithInterpolatedColors(color1, color2, (nearDistance1 / halfWave - nearDistance2 / halfWave) / 2.0 + 0.5, (1 - (nearDistance1 + nearDistance2) / waveLength));
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
         const int parityCount = PotentiometerRead(MODE_DIAL, 1, 5);
         Color colors[parityCount];
         for (int i = 0; i < parityCount; ++i) {
           colors[i] = NamedRainbow[random(ARRAY_SIZE(NamedRainbow))];
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
           _lights[i]->transitionToColor(NamedRainbow[random(ARRAY_SIZE(NamedRainbow))], 10);
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
  for (int i = 0; i < _lightCount; ++i) {  
    _lights[i]->transitionTick(tickTime, frameDuration * _frameDurationMultiplier);
  }
  
  // Automatic colors
  for (int i = 0; i < _automaticColorsCount; ++i) {
    if (_automaticColorsProgress[i] == 0 || _automaticColorsProgress[i] >= 1) {
      _automaticColorsProgress[i] = 0;
      _automaticColors[i] = _automaticColorsTargets[i];
      _automaticColorsTargets[i] = NamedRainbow[random(ARRAY_SIZE(NamedRainbow))];
    }
    _automaticColorsProgress[i] += 0.01;
  }
  
  updateStrand();
  _lastTick = time;
  
#ifndef TEST_MODE
  if (time - _modeStart > TRANSITION_TIME * 1000) {
    setMode(randomMode());
  }
#endif
  unsigned int newFrameDuration = PotentiometerRead(TCL_POT2, 10, 112);
  if (newFrameDuration != frameDuration) {
    frameDuration = newFrameDuration;
  #ifndef TEST_MODE
    // Switch out of modes that are too slow or fast for the new frameDuration
    DelayRange range = rangeForMode(_mode);
    if (newFrameDuration < range.low || newFrameDuration > range.high) {
      setMode(randomMode());
    }
#endif
  }
  
  // This button reads as low state on the first loop for some reason, so start the flag as true to ignore the pres
  static bool button1Down = true;
  if (digitalRead(TCL_MOMENTARY1) == LOW) {
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
  Serial.begin(9600);
#endif

  randomSeed(analogRead(A4));
  TCL.begin();
  TCL.setupDeveloperShield();
  
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
}


