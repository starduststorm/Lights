#include <SPI.h>
#include <TCL.h>

// Pulse delay contorlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough

static const unsigned int LED_COUNT = 50;

struct Color {
  byte red;
  byte green;
  byte blue;
};
typedef struct Color Color;

static const Color kRedColor = (Color){0xFF, 0, 0};
static const Color kOrangeColor = (Color){0xFF, 0x60, 0x0};
static const Color kYellowColor = (Color){0xFF, 0xFF, 0};
static const Color kGreenColor = (Color){0, 0xFF, 0};
static const Color kCyanColor = (Color){0, 0xFF, 0xFF};
static const Color kBlueColor = (Color){0, 0, 0xFF};
static const Color kVioletColor = (Color){0x8E, 0x25, 0xFB};
static const Color kMagentaColor = (Color){0xFF, 0, 0xFF};

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

typedef enum {
  BMModeFollow,
  BMModeCount,
} BMMode;

#define MIN(x, y) ((x) > (y) ? y : x);
#define MAX(x, y) ((x) < (y) ? y : x);

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
  
  int modeState; // For the Scene mode to use to store state
};

BMLight::BMLight() : transitionRate(0)
{
}

void BMLight::transitionToColor(Color transitionTargetColor, float rate)
{
  targetColor = transitionTargetColor;
  originalColor = color;
  transitionRate = rate;
  transitionProgress = 0;
}

void BMLight::transitionTick(unsigned long millis, unsigned int frameDuration)
{
  if (transitionRate > 0) {
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
  
  void applyAll(byte r, byte g, byte b);
  
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
  for (int i=0; i<LED_COUNT; i++) {
    TCL.sendColor(_lights[i].color.red, _lights[i].color.green, _lights[i].color.blue);
  }
  TCL.sendEmptyFrame();
}

#pragma mark - Convenience

void BMScene::applyAll(byte r, byte g, byte b)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i].color = MakeColor(r, g, b);
  }
}

#pragma mark - Public

BMScene::BMScene(unsigned int lightCount) : _mode((BMMode)0), frameDuration(100)
{
  _lightCount = lightCount;
  _lights = new BMLight[_lightCount];
  _lastTick = millis();
  _lastFrame = _lastTick;
  
  applyAll(0, 0, 0);
}

BMScene::~BMScene()
{
}

void BMScene::setMode(BMMode mode)
{
  _mode = mode;
  
  switch (_mode) {
    case BMModeFollow:
      _followColor = 0;
      _followLeader = 0;
      break;
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
      default: // Turn all off
        applyAll(0, 0, 0);
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
}

static BMScene *gLights;

void setup()
{
  TCL.begin();
  
  gLights = new BMScene(LED_COUNT);
  gLights->setMode(BMModeFollow);
  gLights->frameDuration = 100;
}

void loop()
{
  gLights->tick();
}


