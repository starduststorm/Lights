
#include "WS2811.h"
#include "Color.h"
#include "ColorMaker.h"

typedef enum {
  ModeWaves,
  ModeFire,
  ModeBlueFire,
  
  ModeLightningBugs,
  ModeParity,
#if ARDUINO_DUE
  ModeInterferingWaves,
#endif
  ModeRainbow,
  ModeAccumulator,
  ModeCount,
  // These are all either boring or need work.
  ModePinkFire,
  ModeTwinkle,

  ModeOneBigWave,
  ModeBoomResponder,
  ModeBounce,
} Mode;

struct SpeedRange {
  float low;
  float high;
};
typedef struct SpeedRange SpeedRange;

static struct SpeedRange SpeedRangeMake(float low, float high)
{
  SpeedRange r;
  r.low = low;
  r.high = high;
  return r;
}

static const float kSpeedMax = 2.0;
static const float kSpeedNormalMin = 0.5;
static const float kSpeedMin = 0.4;
static const SpeedRange kSpeedNormalRange = SpeedRangeMake(kSpeedNormalMin, kSpeedMax);

static SpeedRange kModeRanges[ModeCount] = {0};

static const unsigned int kInterferringWavesNum = 7;

#pragma mark - 

class Scene {
private:
  unsigned int _lightCount=0;
  Mode _mode;
  unsigned long _modeStart=0;
  unsigned long _lastTick=0;
  
  Light **_lights;
  
#if MEGA_WS2811
  WS2811Renderer *ws2811Renderer;
#elif TEENSY_WS2812
  CRGB leds[LED_COUNT];
#endif

  float _globalSpeed; // Multiplier for global follow and fade speed
  ColorMaker *_colorMaker = NULL;

  // "Follow" convenience counter
  float _followLeader=0;
  float _followSpeed; // number of lights lights / s that the _followLeader moves
  bool _directionIsReversed;
  
  // Mode specific data
  unsigned int _followColorIndex=0;
  Color *_colorScratch=NULL;
  float *_sceneVariation = NULL;
  unsigned int _soundPeak;
  float *_leaders = NULL; // for interfering waves
  unsigned long _timeMarker=0;
  
  //
  
  void transitionAll(Color c, float rate);
  
  void updateStrand();
  SpeedRange speedRangeForMode(Mode mode);

public:
  void applyAll(Color c);
  void tick();
  Scene(unsigned int ledCount);
  void setMode(Mode mode);
  ~Scene();
  Mode randomMode();
};

#pragma mark - Private

float getBrightness()
{
  if (!kHasDeveloperBoard) {
    return DEFAULT_BRIGHNESS;
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

void Scene::updateStrand()
{
  float brightnessAdjustment = getBrightness();
  
  float soundMultiplier = 1.0;
  bool useSound = false;
#ifdef SOUND_DIAL
  float soundSensitivity = PotentiometerRead(SOUND_DIAL, 10, 204);
  useSound = soundSensitivity < 200;
  
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
#endif
  
  // Update per-pixel
#if ARDUINO_TCL
  TCL.sendEmptyFrame();
#endif
  for (unsigned int i = 0; i < _lightCount; ++i) {
    Light *light = _lights[i];
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
    red = min(red, 255);
    green = min(green, 255);
    blue = min(blue, 255);
    
    if (red + green + blue < 5) {
      // Avoid fades like yellow -> black showing (1,0,0) == red right at the end of the fadeout
      red = green = blue = 0;
    }
    
#if ARDUINO_TCL
    TCL.sendColor(red, green, blue);
#elif MEGA_WS2811
    // Color corrections for the WS2811 strands I use
    red = min(1.1 * red, 255);
    
    ws2811Renderer->setPixel(i, red, green, blue);
#elif TEENSY_WS2812 
    // RGB == GRB? *shrug*
    leds[i] = CRGB(green, red, blue);
#endif
  }

  // Send to strand
#if ARDUINO_TCL
  TCL.sendEmptyFrame();
#elif MEGA_WS2811
  ws2811Renderer->render();
#elif TEENSY_WS2812
  FastLED.show(); 
#endif
}

#pragma mark - Convenience

void Scene::applyAll(Color c)
{
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->color = c;
  }
}

void Scene::transitionAll(Color c, float rate)
{
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionToColor(c, rate);
  }
}

#pragma mark - Public

void setSpeedRangeForMode(SpeedRange speedRange, Mode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    kModeRanges[mode] = speedRange;
  }
}

Scene::Scene(unsigned int lightCount) : _mode((Mode)-1), _globalSpeed(1.0)
{ 
  setSpeedRangeForMode(SpeedRangeMake(0.7, 1.3), ModeFire);
  setSpeedRangeForMode(speedRangeForMode(ModeFire), ModeBlueFire);
  setSpeedRangeForMode(speedRangeForMode(ModeFire), ModePinkFire);
  
  setSpeedRangeForMode(SpeedRangeMake(kSpeedMin, kSpeedMin + 0.2), ModeLightningBugs);
  
  _lightCount = lightCount;
  _lights = new Light*[_lightCount];
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i] = new Light();
  }
  _lastTick = millis();
  
#if MEGA_WS2811
  ws2811Renderer = new WS2811Renderer(LED_COUNT);
#elif TEENSY_WS2812
  LEDS.addLeds<WS2812, TEENSY_PIN, RGB>(leds, LED_COUNT);
  LEDS.setBrightness(100);
#endif
  
  applyAll(kBlackColor);

  _colorMaker = new ColorMaker();
}

Scene::~Scene()
{
  delete _colorMaker;
#if MEGA_WS2811
  delete ws2811Renderer;
#endif
  delete[] _lights;
}

SpeedRange Scene::speedRangeForMode(Mode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    if (kModeRanges[mode].low == 0 && kModeRanges[mode].high == 0) {
      // It's just unset, treat it like middle speed
      return kSpeedNormalRange;
    }
    return kModeRanges[mode];
  }
  return kSpeedNormalRange;
}

Mode Scene::randomMode()
{
  int matchCount = 0;
  Mode matchingModes[ModeCount];
  for (int i = 0; i < ModeCount; ++i) {
    Mode mode = (Mode)i;
    bool modeAllowed = true;

#if MEGA_WS2811
    // Easter Egg (Christmas Egg?)
    // If lights are left on for more than 8 hours, start including the lightning bugs scene
    // If somone wanders about the house late at night, sees lightning bugs, and goes "huh, cool" I'll consider it a success.
    static long kEightHours = 8L * 60L * 60L * 1000L;
    if (mode == ModeLightningBugs && millis() < kEightHours) {
      modeAllowed = false;
    }
#elif DEVELOPER_BOARD
    // We have a dial for speed, which includes and excludes various modes
    SpeedRange range = speedRangeForMode(mode);
    modeAllowed = (_globalSpeed >= range.low && _globalSpeed <= range.high);
#else
    // Lightning bugs not allowed because we can't dial away from it
    if (mode == ModeLightningBugs) {
      modeAllowed = false;
    }
#endif
    if (modeAllowed) {
      matchingModes[matchCount++] = mode;
    }
  }
  if (matchCount > 0) {
    return matchingModes[fast_rand(matchCount)];
  } else {
    // No matches. Pick any mode.
    return (Mode)fast_rand(ModeCount);
  }
}

void Scene::setMode(Mode mode)
{
  if (mode != _mode) {
    // Transition away from old mode
    switch (_mode) {
      case ModeLightningBugs:
        // When ending lightning bugs, have all the bugs go out
        transitionAll(kNightColor, 1);
        break;
      default:
        break;
    }
    
    _mode = mode;
    
    _soundPeak = 0;
    
    int automaticColorsCount = 0;
    float automaticColorsDuration = 2.0;
    
    free(_colorScratch);
    _colorScratch = NULL;
    
    free(_sceneVariation);
    _sceneVariation = NULL;
    
    free(_leaders);
    _leaders = NULL;
    
    // Initialize new mode
    for (unsigned int i = 0; i < _lightCount; ++i) {
      _lights[i]->modeState = 0;
    }

    _followLeader = fast_rand(_lightCount);
    _followSpeed = 8; // 8 lights per second by default
    _timeMarker = 0;
    
    switch (_mode) {
      case ModeLightningBugs:
        transitionAll(kNightColor, 0.4);
        break;
#if ARDUINO_DUE
      case ModeInterferingWaves:
        automaticColorsCount = _lightCount / (float)kInterferringWavesNum;
        _sceneVariation = (float *)malloc(automaticColorsCount * sizeof(float));
        _leaders = (float *)malloc(automaticColorsCount * sizeof(float));
        for (int i = 0; i < automaticColorsCount; ++i) {
          _sceneVariation[i] = ((int)fast_rand(0, 80) - 40) / 10.0;
        }
        automaticColorsDuration = 3.0;
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        break;
#endif
      case ModeWaves:
      case ModeOneBigWave: {
        automaticColorsCount = 1;
        automaticColorsDuration = 4.0;
        break;
      case ModeRainbow:
        _followColorIndex = fast_rand(ROYGBIVRainbow.count);
        break;
      case ModeAccumulator:
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        break;
      case ModeTwinkle:
        for (unsigned i = 0; i < _lightCount; ++i) {
          Color color = ROYGBIVRainbow.randomColor();
          _lights[i]->transitionToColor(color, 0.1);
        }
        break;
      default: break;
      }
    }
    _colorMaker->prepColors(automaticColorsCount, automaticColorsDuration);
    _directionIsReversed = (fast_rand(2) == 0);
    _modeStart = millis();
  }
}

void Scene::tick()
{
  unsigned long long time = millis();
  unsigned long long tickTime = (time - _lastTick) * _globalSpeed;
  _lastTick = time;

  // Fade transitions
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionTick(tickTime);
  }
  
  static bool allOff = false;
  if (kHasDeveloperBoard && digitalRead(TCL_SWITCH2) == LOW) {
    if (!allOff) {
      for (unsigned int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionToColor(kBlackColor, 1, LightTransitionEaseOut);
      }
      allOff = true;
    }
    if (_lights[0]->isTransitioning()) {
      updateStrand();
    } else {
      // Just sleep after we're done fading
      delay(100);
    }
    return;
  } else {
    allOff = false;
  }
  
  _followLeader += (_directionIsReversed ? -1 : 1) * (_followSpeed * tickTime / 1000.0);
  _followLeader = fmodf(_followLeader + _lightCount, _lightCount);

  _colorMaker->tick(tickTime);
  
  switch (_mode) {
    case ModeFire:
    case ModeBlueFire:
    case ModePinkFire: {
      // Interpolate, fade, and snap between two colors
      Color c1, c2;
      if (_mode == ModePinkFire) {
        c1 = MakeColor(0xFF, 0x0, 0xB4);
        c2 = MakeColor(0xFF, 0x0, 0xB4);
      } else if (_mode == ModeBlueFire) {
        c1 = MakeColor(0x30, 0x10, 0xFF);
        c2 = MakeColor(0, 0xB0, 0xFF);
      } else {
        c1 = MakeColor(0xFF, 0x30, 0);
        c2 = MakeColor(0xFF, 0x80, 0); 
      }
      for (unsigned int i = 0; i < _lightCount; ++i) {
        Light *light = _lights[i];
        if (!(light->isTransitioning())) {
          long choice = fast_rand(100);
          
          if (choice < 10) {
            // 10% of the time, fade slowly to black
            light->transitionToColor(kBlackColor, 0.5);
          } else {
            // Otherwise, fade or snap to another color
            Color color2 = (fast_rand(2) ? c1 : c2);
            if (choice < 95) {
              Color mixedColor = ColorWithInterpolatedColors(light->color, color2, fast_rand(101), fast_rand(101));
              light->transitionToColor(mixedColor, 0.24);
            } else {
              light->color = color2;
              // after setting the color, do a fade to this same color to keep the light "busy" for a short time.
              light->transitionToColor(color2, 0.1);
            }
          }
        }
      }
      break;
    }
    
    case ModeLightningBugs: {
      for (unsigned int i = 0; i < _lightCount; ++i) {
        Light *light = _lights[i];
        if (!light->isTransitioning()) {
          switch (light->modeState) {
            case 1:
              // When putting a bug out, fade to black first, otherwise we fade from yellow(ish) to blue and go through white.
              light->transitionToColor(kBlackColor, 0.2);
              light->modeState = 2;
              break;
            case 2:
              light->transitionToColor(kNightColor, 0.1);
              light->modeState = 0;
              break;
            default:
              if (fast_rand(10000) == 0) {
                // Blinky blinky
                light->transitionToColor(MakeColor(0xD0, 0xFF, 0), 0.1);
                light->modeState = 1;
              }
              break;
          }
        }
      }
      break;
    }
    
    case ModeBounce: {
      static int direction = 1;
      _lights[(int)_followLeader]->transitionToColor(kBlackColor, 0.4);
      _followLeader = _followLeader + direction;
      _lights[(int)_followLeader]->color = RGBRainbow.randomColor();
      if (_followLeader == _lightCount - 1  || _followLeader == 0) {
        direction = -direction;
      }
      break;
    }
    
    case ModeWaves:
    case ModeOneBigWave: {
      const unsigned int waveLength = (_mode == ModeWaves ? 15 : _lightCount);
      // Needs to fade out over less than half a wave, so there are some off in the middle.
      const float fadeDuration = (waveLength / 2.0 - 4) / (_followSpeed * _globalSpeed);
      
      Color waveColor = _colorMaker->getColor(0);
      for (unsigned int i = 0; i < _lightCount / waveLength; ++i) {
        unsigned int turnOnLeaderIndex = ((int)_followLeader + i * waveLength) % _lightCount;
        unsigned int turnOffLeaderIndex = ((int)_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount;
        _lights[turnOnLeaderIndex]->transitionToColor(waveColor, fadeDuration, LightTransitionEaseIn);
        _lights[turnOffLeaderIndex]->transitionToColor(kBlackColor, fadeDuration, LightTransitionEaseOut);
      }
      break;
    }
    
    case ModeRainbow: {
      const unsigned int waveLength = 7;
      const float fadeDuration = (waveLength - 2) / (_followSpeed * _globalSpeed);
      
      for (unsigned int i = 0; i < _lightCount / waveLength; ++i) {
        unsigned int changeIndex = ((int)_followLeader + i * waveLength) % _lightCount;
        Color waveColor = ROYGBIVRainbow.getColor(_followColorIndex + i);
        _lights[changeIndex]->transitionToColor(waveColor, fadeDuration , LightTransitionEaseIn);
      }
      break;
    }
    
#if ARDUINO_DUE
    case ModeInterferingWaves: {
      const int waveLength = 18;
      const float halfWave = waveLength / 2;
      
      // For the first 3 seconds of interfering waves, fade from previous mode
      static const int kFadeTime = 3000;
      unsigned long modeTime = millis() - _modeStart;
      bool inModeTransition = modeTime < kFadeTime;
      
      memset(_colorScratch, 0, _lightCount * sizeof(Color));
      
      float lightsChunk = _lightCount / (float)kInterferringWavesNum;
      for (int waveIndex = 0; waveIndex < kInterferringWavesNum; ++waveIndex) {
        if (waveIndex < kInterferringWavesNum / 2.0) { // Half the colors going in each direction
          _leaders[waveIndex] = _followLeader + 2 * waveIndex * lightsChunk + _sceneVariation[waveIndex];
        } else {
          int normalizedWavedIndex = waveIndex - kInterferringWavesNum / 2.0;
          _leaders[waveIndex] = _lightCount - (_followLeader + 2 * normalizedWavedIndex * lightsChunk + lightsChunk) + _sceneVariation[waveIndex];
        }
        
        Color waveColor = _colorMaker->getColor(waveIndex);
        
        for (int w = -halfWave; w < halfWave; ++w) {
          int lightIndex = (int)(_leaders[waveIndex] + w + _lightCount) % _lightCount;
          float distance = MOD_DISTANCE(lightIndex, _leaders[waveIndex], _lightCount);
          if (distance < halfWave) {
            
            Color existingColor = _colorScratch[lightIndex];
            
            float litRatio = (existingColor.red + existingColor.green + existingColor.blue) / (float)(3 * 255);
            // If the existing light is less than 3% lit, use the whole new color. Otherwise smoothly fade into splitting the difference.
            float additionalFade = (litRatio < 0.03 ? 50 : (litRatio > 0.1 ? 0 : (50 - 50 * litRatio / 0.1)));
            
//              logf("Existing color = (%i, %i, %i), litRation = %f, additionalFade = %f", existingColor.red, existingColor.green, existingColor.blue, litRatio, additionalFade);
            
            Color color = ColorWithInterpolatedColors(existingColor, waveColor,
                                                     ((1 - distance / halfWave) * (50 + additionalFade)),
                                                     100);
            _colorScratch[lightIndex] = color;
            
            float red = color.red, green = color.green, blue = color.blue;
            red /= 255;
            red *= red;
            red *= 255;
            
            green /= 255;
            green *= green;
            green *= 255;
            
            blue /= 255;
            blue *= blue;
            blue *= 255;
            
            color.red = red, color.blue = blue, color.green = green;
            
            if (inModeTransition) {
              // Fade from previous mode
              color = ColorWithInterpolatedColors(_lights[lightIndex]->color, color, (float)modeTime / kFadeTime * 100, 100);
            }
            
            _lights[lightIndex]->color = color;
          }
        }
      }
      // Black out all other lights
      for (unsigned int i = 0; i < _lightCount; ++i) {
        if (ColorIsEqualToColor(_colorScratch[i], kBlackColor)) {
          if (inModeTransition) {
            _lights[i]->color = ColorWithInterpolatedColors(_lights[i]->color, kBlackColor, (float)modeTime / kFadeTime * 100, 100);
          } else {
            _lights[i]->color = kBlackColor;
          }
        }
      }
      break;
    }
#endif
    
    case ModeParity: {
      const int parityCount = 2;//(kHasDeveloperBoard ? PotentiometerRead(MODE_DIAL, 1, 5) : 2);
      Color colors[parityCount];
      for (int i = 0; i < parityCount; ++i) {
        Color sourceColor = _lights[i]->color;
        Color targetColor;
        do {
          targetColor = NamedRainbow.randomColor();
        } while (ColorTransitionWillProduceWhite(sourceColor, targetColor));
        colors[i] = targetColor;
      }
      for (unsigned int i = 0; i < _lightCount; ++i) {
        if (!_lights[i]->isTransitioning()) {
          int parity = i % parityCount;
          _lights[i]->transitionToColor(colors[parity], 2);
        }
      }
      break;
    }
    
    case ModeBoomResponder:
      for (unsigned int i = 0; i < _lightCount; ++i) {
        if (!_lights[i]->isTransitioning()) {
          _lights[i]->transitionToColor(NamedRainbow.randomColor(), 1);
        }
      }
      break;
    
    case ModeAccumulator: {
      const int kernelWidth = 1;
     
      const  unsigned int pingRate = 20000 / _lightCount;
      if (time - _timeMarker > pingRate) {
        unsigned int ping = fast_rand(_lightCount);
        Color c = NamedRainbow.randomColor();

        for (int i = (ping - 1); i < ping + 1; ++i) {
          unsigned int light = (i + _lightCount) % _lightCount;
          _lights[light]->transitionToColor(c, 0.25, LightTransitionEaseOut);
        }
        
        _timeMarker = time;
      }
     
      for (unsigned int i = 0; i < _lightCount; ++i) {
        _colorScratch[i] = _lights[i]->color;
      }

      static unsigned long lastBlur = 0;
      if (time - lastBlur > 80) {
        for (unsigned int target = 0; target < _lightCount; ++target) {
          if (_lights[target]->isTransitioning()) {
            continue;
          }
          // Box blur each light from a kernel on both sides
          Color c = kBlackColor;
          unsigned int count = 0;
          Color sourceColor;
          for (int k = -kernelWidth; k <= kernelWidth; ++k) {
            unsigned int source = (target + k + _lightCount) % _lightCount;
            sourceColor = _colorScratch[source];
            if (sourceColor.red + sourceColor.green + sourceColor.blue < 20) {
              continue;
            }
            c.red = (c.red * count + sourceColor.red) / (float)(count + 1);
            c.green = (c.green * count + sourceColor.green) / (float)(count + 1);
            c.blue = (c.blue * count + sourceColor.blue) / (float)(count + 1);
            ++count;
          }
          // And fade out
          c.red *= 0.90;
          c.green *= 0.90;
          c.blue *= 0.90;
         
          _lights[target]->transitionToColor(c, 0.15);
        }
        lastBlur = time;
      }
      break;
    }
    case ModeTwinkle: {
      static Color TwinkleRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor, kVioletColor, kBlackColor, kBlackColor};
      bool anyTransitionHappening = false;
      for (unsigned i = 0; i < _lightCount; ++i) {
        if (_lights[i]->isTransitioning()) {
          anyTransitionHappening = true;
          break;
        }
      }
      if (!anyTransitionHappening) {
        static int parity = 5;
        static int lastSegmentChanged = -1;
        for (int twice = 0; twice < 2; ++twice) {
          int changeSegment;
          do {
            changeSegment = fast_rand(parity);
          } while (changeSegment == lastSegmentChanged);
          lastSegmentChanged = changeSegment;
          
          Color startColor = _lights[changeSegment]->color;
          Color targetColor;
          
          // Black is a possible target, so make sure we don't transition to a completely black strand
          bool acceptableColor = false;
          do {
            targetColor = TwinkleRainbow[fast_rand(ARRAY_SIZE(TwinkleRainbow))];
            
            if (ColorIsEqualToColor(startColor, targetColor)) {
              // Actually change the color
              continue;
            }
            if (ColorTransitionWillProduceWhite(startColor, targetColor)) {
              // Don't fade through white
              continue;
            }
            
            bool targetIsBlackColor = ColorIsEqualToColor(targetColor, kBlackColor);
            if (targetIsBlackColor) {
              bool transitioningToAllBlack = true;
              for (int seg = 0; seg < parity; ++seg) {
                Color segColor = (_lights[seg]->isTransitioning() ? _lights[seg]->targetColor : _lights[seg]->color);
                if (seg != changeSegment && !ColorIsEqualToColor(segColor, kBlackColor)) {
                  transitioningToAllBlack = false;
                  break;
                }
              }
              if (transitioningToAllBlack) {
                // who turned out the lights?
                continue;
              }
            }
            acceptableColor = true;
          } while (!acceptableColor);
          
          for (unsigned i = changeSegment; i < _lightCount; i += parity) {
            _lights[i]->transitionToColor(targetColor, 0.1);
          }
        }
      }
      break;
    }
    
    default: // Turn all off
      applyAll(kBlackColor);
      break;
  }
  
  updateStrand();
  
#ifndef TEST_MODE
  if (time - _modeStart > (unsigned long long)MODE_TIME * 1000) {
    Mode nextMode = randomMode();
    logf("Timed mode change to %i", (int)nextMode);
    _modeStart = time; // in case mode doesn't actually change here.
    setMode(nextMode);
  } else {
#endif
    float newGlobalSpeed = (kHasDeveloperBoard ? PotentiometerReadf(SPEED_DIAL, kSpeedMin, kSpeedMax) : 1.0);
    if (abs(newGlobalSpeed - _globalSpeed) > 0.06) {
      logf("New global speed = %f", newGlobalSpeed);
      _globalSpeed = newGlobalSpeed;
#ifndef TEST_MODE
      // Switch out of modes that are too slow or fast for the new global speed
      SpeedRange range = speedRangeForMode(_mode);
      if (_globalSpeed < range.low || _globalSpeed > range.high) {
        Mode newMode = randomMode();
        logf("Switching out of mode %i due to speed. New mode = %i", (int)_mode, (int)newMode);
        setMode(newMode);
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
      setMode((Mode)((_mode + 1) % ModeCount));
      button1Down = true;
    }
  } else {
    button1Down = false;
  }
}


