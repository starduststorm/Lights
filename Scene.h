
#include "WS2811.h"
#include "Color.h"

Color RGBRainbow[] = {kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor};
Color NamedRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kIndigoColor, kVioletColor, kMagentaColor};
Color ROYGBIVRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kBlueColor, kIndigoColor, kVioletColor};

typedef enum {
  ModeWaves,
  ModeOneBigWave,
  ModeParity,
  ModeFire,
  ModeBlueFire,
  ModeLightningBugs,
#if ARDUINO_DUE
  ModeInterferingWaves,
#endif
  ModeRainbow,
  ModeAccumulator,
  ModeCount,
  ModeFollow,
  ModeBoomResponder,
  ModeBounce,
} Mode;

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

static DelayRange kModeRanges[ModeCount] = {0};

#pragma mark - 

class Scene {
private:
  unsigned int _lightCount=0;
  Mode _mode;
  unsigned long _modeStart=0;
  unsigned long _lastTick=0;
  unsigned long _lastFrame=0;
  
  Light **_lights;
  
#if WS2811
  WS2811Renderer *ws2811Renderer;
#endif
  
  // Mode specific options
  float _frameDurationMultiplier;
  int _automaticColorsCount=0;
  
  // Mode specific data
  int _followLeader=0;
  float _smoothLeader=0;
  Color _followColor;
  unsigned int _followColorIndex=0;
  Color *_colorScratch=NULL;
  
  float *_sceneVariation = NULL;
  
  Color *_automaticColors = NULL;
  Color *_automaticColorsTargets = NULL;
  float *_automaticColorsProgress = NULL; // 0-100%
  float  _automaticColorsRate;
  Color *_automaticColorsCache = NULL;
   
  float _transitionProgress=0;
  Color _targetColor;
  bool _directionIsReversed;
  unsigned int _soundPeak;
  
  void transitionAll(Color c, float rate);
  
  void updateStrand();
  DelayRange rangeForMode(Mode mode);
  Color getAutomaticColor(unsigned int i);
  void clearAutomaticColorsCache();
public:
  void applyAll(Color c);
  void tick();
  Scene(unsigned int ledCount);
  void setMode(Mode mode);
  ~Scene();
  Mode randomMode();
  
  unsigned int frameDuration; // millisecond delay between frames
  float frameDurationFloat; // Needed to compare values to current potentiometer to ignore noise
  
  float *_leaders = NULL; // for interfering waves
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
  
#if !WS2811
  TCL.sendEmptyFrame();
#endif
  for (int i = 0; i < _lightCount; ++i) {
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
    
#if !WS2811
    TCL.sendColor(red, green, blue);
#else
    // Color corrections for the WS2811 strands I use
    red = min(1.1 * red, 255);
    
    ws2811Renderer->setPixel(i, red, green, blue);
#endif
  }
#if !WS2811
  TCL.sendEmptyFrame();
#else
  ws2811Renderer->render();
#endif
}

#pragma mark - Convenience

void Scene::applyAll(Color c)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i]->color = c;
  }
}

void Scene::transitionAll(Color c, float rate)
{
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionToColor(c, rate);
  }
}

#pragma mark - Public

void setDelayRangeForMode(DelayRange delayRange, Mode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    kModeRanges[mode] = delayRange;
  }
}

Scene::Scene(unsigned int lightCount) : _mode((Mode)-1), frameDuration(100)
{
  setDelayRangeForMode(kDelayRangeStandard, ModeFollow);
  setDelayRangeForMode(MakeDelayRange(50, kMaxStandardFrameDuration), ModeFire);
  setDelayRangeForMode(MakeDelayRange(50, kMaxStandardFrameDuration), ModeBlueFire);
  setDelayRangeForMode(MakeDelayRange(kMaxStandardFrameDuration, kMaxFrameDuration), ModeLightningBugs);
  setDelayRangeForMode(kDelayRangeStandard, ModeWaves);
  setDelayRangeForMode(MakeDelayRange(kMinStandardFrameDuration, 40), ModeOneBigWave);
#if ARDUINO_DUE
  setDelayRangeForMode(kDelayRangeStandard, ModeInterferingWaves);
#endif
  setDelayRangeForMode(MakeDelayRange(0, kMaxStandardFrameDuration), ModeParity);
  
  _lightCount = lightCount;
  _lights = new Light*[_lightCount];
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i] = new Light();
  }
  _lastTick = millis();
  _lastFrame = _lastTick;
  
#if WS2811
  ws2811Renderer = new WS2811Renderer(LED_COUNT);
#endif
  
  applyAll(kBlackColor);
}

Scene::~Scene()
{
}

Color Scene::getAutomaticColor(unsigned int i)
{
  if (ColorIsNoColor(_automaticColorsCache[i])) {
    _automaticColorsCache[i] = ColorWithInterpolatedColors(_automaticColors[i], _automaticColorsTargets[i], _automaticColorsProgress[i], 100);
  }
  return _automaticColorsCache[i];
}

void Scene::clearAutomaticColorsCache()
{
  for (int i = 0; i < _automaticColorsCount; ++i) {
      _automaticColorsCache[i] = kNoColor;
  }
}

DelayRange Scene::rangeForMode(Mode mode)
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

Mode Scene::randomMode()
{
  int matchCount = 0;
  Mode matchingModes[ModeCount];
  for (int i = 0; i < ModeCount; ++i) {
    Mode mode = (Mode)i;
    DelayRange range = rangeForMode(mode);
    if (frameDuration >= range.low && frameDuration <= range.high) {
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
        transitionAll(kNightColor, 10);
        break;
      default:
        break;
    }
    
    _mode = mode;
    _frameDurationMultiplier = 1;
    _soundPeak = 0;
    
    _automaticColorsCount = 0;
    _automaticColorsRate = 1;
    
    free(_automaticColors);
    _automaticColors = NULL;
    free(_automaticColorsTargets);
    _automaticColorsTargets = NULL;
    free(_automaticColorsProgress);
    _automaticColorsProgress = NULL;
    free(_automaticColorsCache);
    _automaticColorsCache = NULL;
    free(_colorScratch);
    _colorScratch = NULL;
    
    free(_sceneVariation);
    _sceneVariation = NULL;
    
    free(_leaders);
    _leaders = NULL;
    
    // Initialize new mode
    for (int i = 0; i < _lightCount; ++i) {
      _lights[i]->modeState = 0;
    }
    
    switch (_mode) {
      case ModeBounce:
      case ModeFollow: {
        // When starting follow, there are sometimes single lights stuck on until the follow lead gets there. 
        // This keeps it from looking odd and too dark
        Color fillColor = ColorWithInterpolatedColors(RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))], kBlackColor, fast_rand(20, 60) / 10.0, 100);
        transitionAll(fillColor, 5);
        break;
      }
      case ModeLightningBugs:
        transitionAll(kNightColor, 10);
        break;
#if ARDUINO_DUE
      case ModeInterferingWaves:
        _frameDurationMultiplier = 1 / 30.; // Interferring waves doesn't use light transitions fades, needs every tick to fade.
        _followLeader = _smoothLeader = 0;
        _automaticColorsCount = _lightCount / 7;
        _sceneVariation = (float *)malloc(_automaticColorsCount * sizeof(float));
        _leaders = (float *)malloc(_automaticColorsCount * sizeof(float));
        for (int i = 0; i < _automaticColorsCount; ++i) {
          _sceneVariation[i] = ((int)fast_rand(0, 80) - 40) / 10.0;
        }
        _automaticColorsRate = 0.1;
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        break;
#endif
      case ModeWaves:
      case ModeOneBigWave: {
        _followLeader = 0;
        _targetColor = RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))];
        _frameDurationMultiplier = 2;
        _automaticColorsCount = 1;
        _automaticColorsRate = 0.2;
        break;
      case ModeRainbow:
        _frameDurationMultiplier = 1;
        _followLeader = 0;
        _followColorIndex = fast_rand(ARRAY_SIZE(ROYGBIVRainbow));
        break;
      case ModeAccumulator:
        _frameDurationMultiplier = 0.5;
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        break;
      }
    }
    _modeStart = millis();
  }
  if (_automaticColorsCount > 0) {
    _automaticColors = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsTargets = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    _automaticColorsProgress = (float *)malloc(_automaticColorsCount * sizeof(float));
    _automaticColorsCache = (Color *)malloc(_automaticColorsCount * sizeof(Color));
    for (int i = 0; i < _automaticColorsCount; ++i) {
      _automaticColors[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsTargets[i] = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
      _automaticColorsProgress[i] = 0;
      _automaticColorsCache[i] = kNoColor;
    }
  }
  _directionIsReversed = (fast_rand(2) == 0);
}

void Scene::tick()
{
  static bool allOff = false;
  if (kHasDeveloperBoard && digitalRead(TCL_SWITCH2) == LOW) {
    if (!allOff) {
      for (int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionToColor(kBlackColor, 3, LightTransitionEaseOut);
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
      case ModeFollow: {
        Color c = RGBRainbow[_followColorIndex];
        _lights[_followLeader]->transitionToColor(c, 3);
        
        _followLeader += (_directionIsReversed ? -1 : 1);
        if (_followLeader < 0 || _followLeader >= _lightCount) {
          _followLeader = (_followLeader + _lightCount) % _lightCount;
          _followColorIndex = (_followColorIndex + 1) % ARRAY_SIZE(RGBRainbow);
        }
        break;
      }
      
      case ModeFire:
      case ModeBlueFire: {
        // Interpolate, fade, and snap between two colors
        Color c1 = (_mode == ModeFire ? MakeColor(0xFF, 0x30, 0) : MakeColor(0x30, 0x10, 0xFF));
        Color c2 = (_mode == ModeFire ? MakeColor(0xFF, 0x80, 0) : MakeColor(0, 0xB0, 0xFF));
        for (int i = 0; i < _lightCount; ++i) {
          Light *light = _lights[i];
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
      
      case ModeLightningBugs: {
        for (int i = 0; i < _lightCount; ++i) {
          Light *light = _lights[i];
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
      
      case ModeBounce: {
        static int direction = 1;
        _lights[_followLeader]->transitionToColor(kBlackColor, 10);
        _followLeader = _followLeader + direction;
        _lights[_followLeader]->color = RGBRainbow[fast_rand(ARRAY_SIZE(RGBRainbow))];
        if (_followLeader == _lightCount - 1  || _followLeader == 0) {
          direction = -direction;
        }
        break;
      }
      
      case ModeWaves:
      case ModeOneBigWave: {
        const unsigned int waveLength = (_mode == ModeWaves ? 15 : _lightCount);
        // Needs to fade out over less than half a wave, so there are some off in the middle.
        const float transitionRate = 80 / (waveLength / 2.0);
        
        Color waveColor = getAutomaticColor(0);
        for (int i = 0; i < _lightCount / waveLength; ++i) {
          unsigned int turnOnLeaderIndex = (_followLeader + i * waveLength) % _lightCount;
          unsigned int turnOffLeaderIndex = (_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount;
          _lights[turnOnLeaderIndex]->transitionToColor(waveColor, transitionRate, LightTransitionEaseIn);
          _lights[turnOffLeaderIndex]->transitionToColor(kBlackColor, transitionRate, LightTransitionEaseOut);
        }
        _followLeader += (_directionIsReversed ? -1 : 1);
        _followLeader = (_followLeader + _lightCount) % _lightCount;
        break;
      }
      
      case ModeRainbow: {
        const unsigned int waveLength = 10;
        const float transitionRate = 120 / waveLength;
        
        for (int i = 0; i < _lightCount / waveLength; ++i) {
          unsigned int changeIndex = (_followLeader + i * waveLength) % _lightCount;
          Color waveColor = ROYGBIVRainbow[(_followColorIndex + i) % ARRAY_SIZE(ROYGBIVRainbow)];
          _lights[changeIndex]->transitionToColor(waveColor, transitionRate, LightTransitionEaseIn);
        }
        _followLeader += (_directionIsReversed ? -1 : 1);
        _followLeader = (_followLeader + _lightCount) % _lightCount;
        break;
      }
      
#if ARDUINO_DUE
      case ModeInterferingWaves: {
        const int waveLength = 10;
        const float halfWave = waveLength / 2;
        
        // For the first 3 seconds of interfering waves, fade from previous mode
        static const int kFadeTime = 3000;
        unsigned long modeTime = millis() - _modeStart;
        bool inModeTransition = modeTime < kFadeTime;

#if ARDUINO_DUE
        bzero(_colorScratch, _lightCount * sizeof(Color));
#else
        memset(_colorScratch, 0, _lightCount * sizeof(Color));
#endif
        
        float lightsChunk = _lightCount / (float)_automaticColorsCount;
        for (int waveIndex = 0; waveIndex < _automaticColorsCount; ++waveIndex) {
          if (waveIndex < _automaticColorsCount / 2.0) { // Half the colors going in each direction
            _leaders[waveIndex] = _smoothLeader + 2 * waveIndex * lightsChunk + _sceneVariation[waveIndex];
          } else {
            int normalizedWavedIndex = waveIndex - _automaticColorsCount / 2.0;
            _leaders[waveIndex] = _lightCount - (_smoothLeader + 2 * normalizedWavedIndex * lightsChunk + lightsChunk) + _sceneVariation[waveIndex];
          }
          
          Color waveColor = getAutomaticColor(waveIndex);
          
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
        _smoothLeader = fmod(_smoothLeader + (15. / frameDuration), _lightCount);
        break;
      }
#endif
      
      case ModeParity:
        if (!_lights[0]->isTransitioning()) {
          const int parityCount = 2;//(kHasDeveloperBoard ? PotentiometerRead(MODE_DIAL, 1, 5) : 2);
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
     
      case ModeBoomResponder:
        for (int i = 0; i < _lightCount; ++i) {
          if (!_lights[i]->isTransitioning()) {
            _lights[i]->transitionToColor(NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))], 10);
          }
        }
        break;
      
      case ModeAccumulator: {
#if ARDUINO_DUE
        const int kernelWidth = 3;
#else
        const int kernelWidth = 1;
#endif
       
        static unsigned long long lastPing = 0;
       
        unsigned long mils = millis();
        const int pingRate = 20000 / _lightCount;
        if (mils - lastPing > pingRate) {
          unsigned int ping = fast_rand(_lightCount);
          Color c = NamedRainbow[fast_rand(ARRAY_SIZE(NamedRainbow))];
          
          _lights[(ping + _lightCount - 1) % _lightCount]->transitionToColor(c, 30);
          _lights[ping]->transitionToColor(c, 30);
          _lights[(ping + 1) % _lightCount]->transitionToColor(c, 15);
         
          lastPing = mils;
        }
       
        for (unsigned int i = 0; i < _lightCount; ++i) {
          _colorScratch[i] = _lights[i]->color;
        }
        
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
         
          _lights[target]->transitionToColor(c, 15);
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
      _automaticColorsProgress[i] += _automaticColorsRate;
    }
    clearAutomaticColorsCache();
  }
  
  updateStrand();
  _lastTick = time;
  
#ifndef TEST_MODE
  if (time - _modeStart > (unsigned long long)TRANSITION_TIME * 1000) {
    Mode nextMode = randomMode();
    logf("Timed mode change to %i", (int)nextMode);
    setMode(nextMode);
  } else {
#endif
    float newFrameDuration = (kHasDeveloperBoard ? PotentiometerReadf(SPEED_DIAL, 10, kMaxFrameDuration - 1) : FRAME_DURATION);
    if (abs(newFrameDuration - frameDurationFloat) > 7) {
      logf("New frame duration = %f", newFrameDuration);
      frameDuration = newFrameDuration;
      frameDurationFloat = newFrameDuration;
#ifndef TEST_MODE
      // Switch out of modes that are too slow or fast for the new frameDuration
      DelayRange range = rangeForMode(_mode);
      if (newFrameDuration < range.low || newFrameDuration > range.high) {
        Mode newMode = randomMode();
        logf("Switching out of mode %i due to frame duration. New mode = %i", (int)_mode, (int)newMode);
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


