#include <SPI.h>
#include <TCL.h>

// Pulse delay controlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough

#define SERIAL_LOGGING 0
#define TRANSITION_TIME (30)

#define MIN(x, y) ((x) > (y) ? y : x)
#define MAX(x, y) ((x) < (y) ? y : x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static const unsigned int LED_COUNT = 50;

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

// Transition and intensity are both in the range 0-100
static Color ColorWithInterpolatedColors(Color c1, Color c2, unsigned int transition, unsigned int intensity)
{
  byte r, g, b;
  r = (((float)c2.red - c1.red) * transition / 100 + c1.red) * intensity / 100;
  g = (((float)c2.green - c1.green) * transition / 100 + c1.green) * intensity / 100;
  b = (((float)c2.blue - c1.blue) * transition / 100 + c1.blue) * intensity / 100;
  
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
  BMModeCount,
  BMModeBounce,
  BMModeTest,
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
  
  // Mode specific data
  float _frameDurationMultiplier;
  int _followLeader;
  Color _followColor;
  unsigned int _followColorIndex;
  bool _curveBlackIntensity;
  float _transitionProgress;
  Color _targetColor;
  
  void applyAll(Color c);
  void transitionAll(Color c, float rate);
  
  void updateStrand();
  DelayRange rangeForMode(BMMode mode);
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
  kModeRanges[BMModeWaves] = MakeDelayRange(50, 200);
  
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

DelayRange BMScene::rangeForMode(BMMode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
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
    
    // Initialize new mode
    for (int i = 0; i < _lightCount; ++i) {
      _lights[i]->modeState = 0;
    }
    
    switch (_mode) {
      case BMModeBounce:
      case BMModeFollow: {
        // When starting follow, there are sometimes single lights stuck on until the follow lead gets there. 
        // This keeps it from looking odd and too dark
        Color fillColor = ColorWithInterpolatedColors(RGBRainbow[random(ARRAY_SIZE(RGBRainbow))], kBlackColor, random(20, 60), 100);
        transitionAll(fillColor, 5);
        // intentional fall-through
      }
      case BMModeTest:
        _followColorIndex = random(ARRAY_SIZE(RGBRainbow));
        _followLeader = 0;
        break;
      case BMModeLightningBugs:
        transitionAll(kNightColor, 10);
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
}

void BMScene::tick()
{
  unsigned long time = millis();
  unsigned long tickTime = time - _lastTick;
  unsigned long frameTime = time - _lastFrame;
  
  if (frameTime > frameDuration * _frameDurationMultiplier) {
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
                Color mixedColor = ColorWithInterpolatedColors(light->color, color2, random(101), random(101));
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
      
      case BMModeTest: {
        const int width = 10;
        const int count = _lightCount / 25;
        for (int i = 0; i < count; ++i) {
          int lead = (_followLeader + i * _lightCount / count) % _lightCount;
          _lights[lead]->transitionToColor(_followColor, 200 / width);
          _lights[(lead + _lightCount - width / 2) % _lightCount]->transitionToColor(kBlackColor, 200 / width);
        }
        _followLeader = (_followLeader + 1) % _lightCount;
        _followColor.red += 5;
        _followColor.green += 4;
        _followColor.blue += 3;
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
        const int waveLength = 10;
        const int transitionRate = 100 / (_lightCount / waveLength);
        if (_transitionProgress == 0 || _transitionProgress >= 1) {
          _transitionProgress = 0;
          _followColor = _targetColor;
          _targetColor = RGBRainbow[random(ARRAY_SIZE(RGBRainbow))];
        }
        _transitionProgress += 0.01;
        Color waveColor = ColorWithInterpolatedColors(_followColor, _targetColor, _transitionProgress * 100, 100);
        for (int i = 0; i < _lightCount / waveLength; ++i) {
          _lights[(_followLeader + i * waveLength) % _lightCount]->transitionToColor(waveColor, transitionRate);
          _lights[(_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount]->transitionToColor(kBlackColor, transitionRate);
        }
        _followLeader = (_followLeader + 1) % _lightCount;
        break;
      }
      
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
  
  updateStrand();
  _lastTick = time;
  
  if (time - _modeStart > TRANSITION_TIME * 1000) {
    setMode(randomMode());
  }
  unsigned int newFrameDuration = (analogRead(TCL_POT2) + 100) / 10.; // Potentiometer has range [0, 1023], map to [10, 112]
  if (newFrameDuration != gLights->frameDuration) {
    gLights->frameDuration = newFrameDuration;
    // Switch out of modes that are too slow or fast for the new frameDuration
    DelayRange range = rangeForMode(_mode);
    if (newFrameDuration < range.low || newFrameDuration > range.high) {
      setMode(randomMode());
    }
  }
}

void setup()
{
  randomSeed(analogRead(0));
  TCL.begin();
  TCL.setupDeveloperShield();
  
#if SERIAL_LOGGING
  Serial.begin(9600);
#endif
  
  gLights = new BMScene(LED_COUNT);
  gLights->setMode(gLights->randomMode());
  gLights->frameDuration = 100;
}

void loop()
{
  gLights->tick();
}


