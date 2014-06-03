#include <SPI.h>
#include <TCL.h>

// Pulse delay contorlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough

#define SERIAL_LOGGING 0

#define MIN(x, y) ((x) > (y) ? y : x)
#define MAX(x, y) ((x) < (y) ? y : x)

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
Color ColorWithInterpolatedColors(Color c1, Color c2, unsigned int transition, unsigned int intensity)
{
  byte r, g, b;
  r = (MAX(c2.red - c1.red, 0) * transition / 100 + c1.red) * intensity / 100;
  g = (MAX(c2.green - c1.green, 0) * transition / 100 + c1.green) * intensity / 100;
  b = (MAX(c2.blue - c1.blue, 0) * transition / 100 + c1.blue) * intensity / 100;
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
  BMModeFollow = 1,
  BMModeFire,
  BMModeLightningBugs,
  BMModeCount,
} BMMode;

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
  
  BMLight *_lights;
  
  // Mode specific data
  int _followLeader;
  unsigned int _followColor;
  
  void applyAll(Color c);
  void transitionAll(Color c, float rate);
  
  void updateStrand();
public:
  void tick();
  BMScene(unsigned int ledCount);
  void setMode(BMMode mode);
  ~BMScene();
  
  unsigned int frameDuration; // millisecond delay between frames
};

#pragma mark - Private

void BMScene::updateStrand()
{
  TCL.sendEmptyFrame();
  for (int i = 0; i < _lightCount; ++i) {
    TCL.sendColor(_lights[i].color.red, _lights[i].color.green, _lights[i].color.blue);
  }
  TCL.sendEmptyFrame();
}

#pragma mark - Convenience

void BMScene::applyAll(Color c)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i].color = c;
  }
}

void BMScene::transitionAll(Color c, float rate)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i].transitionToColor(c, rate);
  }
}

#pragma mark - Public

BMScene::BMScene(unsigned int lightCount) : _mode((BMMode)0), frameDuration(100)
{
  _lightCount = lightCount;
  _lights = new BMLight[_lightCount];
  _lastTick = millis();
  _lastFrame = _lastTick;
  
  applyAll(kBlackColor);
}

BMScene::~BMScene()
{
}

static const Color kNightColor = MakeColor(0, 0, 0x10);

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
    
    // Initialize new mode
    for (int i = 0; i < _lightCount; ++i) {
      _lights[i].modeState = 0;
    }
    
    switch (_mode) {
      case BMModeFollow:
        _followColor = 0;
        _followLeader = 0;
        break;
      case BMModeLightningBugs:
        transitionAll(kNightColor, 10);
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
  
  if (frameTime > frameDuration) {
    switch (_mode) {
      case BMModeFollow: {
        Color colors[] = {kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor};
        
        _lights[_followLeader].transitionToColor(colors[_followColor], 5);
        _followLeader = (_followLeader + 1);
        if (_followLeader >= _lightCount) {
          _followLeader = _followLeader % _lightCount;
          _followColor = (_followColor + 1) % (sizeof(colors) / sizeof(colors[0]));
        }
        break;
      }
      
      case BMModeFire: {
        // Interpolate, fade, and snap between two colors
        Color c1 = MakeColor(0xFF, 0x30, 0);
        Color c2 = MakeColor(0xFF, 0x80, 0);
        for (int i = 0; i < _lightCount; ++i) {
          if (!(_lights[i].isTransitioning())) {
            long choice = random(100);
            
            if (choice < 10) {
              // 10% of the time, fade slowly to black
              _lights[i].transitionToColor(kBlackColor, 20);
            } else {
              // Otherwise, fade or snap to another color
              Color color2 = (random(2) ? c1 : c2);
              if (choice < 95) {
                Color mixedColor = ColorWithInterpolatedColors(_lights[i].color, color2, random(101), random(101));
                _lights[i].transitionToColor(mixedColor, 40);
              } else {
                _lights[i].color = color2;
              }
            }
          }
        }
        break;
      }
      
      case BMModeLightningBugs: {
        for (int i = 0; i < _lightCount; ++i) {
          if (!_lights[i].isTransitioning()) {
            switch (_lights[i].modeState) {
              case 1:
                // When putting a bug out, fade to black first, otherwise we fade from yellow(ish) to blue and go through white.
                _lights[i].transitionToColor(kBlackColor, 20);
                _lights[i].modeState = 2;
                break;
              case 2:
                _lights[i].transitionToColor(kNightColor, 20);
                break;
              default:
                if (random(100) == 0) {
                  // Blinky blinky
                  _lights[i].transitionToColor(MakeColor(0xD0, 0xFF, 0), 30);
                  _lights[i].modeState = 1;
                }
                break;
            }
          }
        }
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
    _lights[i].transitionTick(tickTime, frameDuration);
  }
  
  updateStrand();
  _lastTick = time;
  
  if (time - _modeStart > 30 * 1000) {
    setMode((BMMode)((_mode + 1) % BMModeCount));
  }
}

static BMScene *gLights;

void setup()
{
  TCL.begin();
  
#if SERIAL_LOGGING
  Serial.begin(9600);
#endif

  gLights = new BMScene(LED_COUNT);
  gLights->setMode(BMModeFollow);
  gLights->frameDuration = 100;
}

void loop()
{
  gLights->tick();
}


