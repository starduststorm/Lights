

typedef enum {
  ModeFollow = 0,
  ModeWaves,
  ModeOneBigWave,
  ModeParity,
  ModeCount,
  ModeFire,
  ModeBlueFire,
  ModeLightningBugs,
  ModeInterferingWaves,
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
  DelayRange rangeForMode(Mode mode);
  Color getAutomaticColor(unsigned int i);
public:
  void applyAll(Color c);
  void tick();
  Scene(unsigned int ledCount);
  void setMode(Mode mode);
  ~Scene();
  Mode randomMode();
  
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

void Scene::updateStrand()
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
    
    TCL.sendColor(min(red, 255), min(green, 255), min(blue, 255));
  }
  TCL.sendEmptyFrame();
  logf("Sent a strand update!");
  Color color = _lights[_lightCount - 1]->color;
  logf("Last color was (%u, %u, %u)", color.red, color.green, color.blue);
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

Scene::Scene(unsigned int lightCount) : _mode((Mode)-1), frameDuration(100)
{
  kModeRanges[ModeFollow] = kDelayRangeStandard;
  kModeRanges[ModeFire] = MakeDelayRange(50, kMaxStandardFrameDuration);
  kModeRanges[ModeBlueFire] = MakeDelayRange(50, kMaxStandardFrameDuration);
  kModeRanges[ModeLightningBugs] = MakeDelayRange(kMaxStandardFrameDuration, kMaxFrameDuration);
  kModeRanges[ModeWaves] = kDelayRangeStandard;
  kModeRanges[ModeOneBigWave] = MakeDelayRange(kMinStandardFrameDuration, 40);
  kModeRanges[ModeInterferingWaves] = kDelayRangeStandard;
  kModeRanges[ModeParity] = MakeDelayRange(0, kMaxStandardFrameDuration);
  
  _lightCount = lightCount;
  _lights = new Light*[_lightCount];
  for (int i = 0; i < _lightCount; ++i) {
    _lights[i] = new Light();
  }
  _lastTick = millis();
  _lastFrame = _lastTick;
  
  applyAll(kBlackColor);
}

Scene::~Scene()
{
}

Color Scene::getAutomaticColor(unsigned int i)
{
  return ColorWithInterpolatedColors(_automaticColors[i], _automaticColorsTargets[i], _automaticColorsProgress[i], 100);
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

static const Color kNightColor = MakeColor(0, 0, 0x10);
static Scene *gLights;

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
      case ModeInterferingWaves:
        _frameDurationMultiplier = 1 / 30.; // Interferring waves doesn't use light transitions fades, needs every tick to fade.
        _followLeader = _smoothLeader = 0;
        _automaticColorsCount = _lightCount / 10;
        break;
      case ModeWaves:
      case ModeOneBigWave: {
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
        logf("In ::tick for ModeFollow");
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
      
      case ModeInterferingWaves: {
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
     
     case ModeParity:
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
     
     case ModeBoomResponder:
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
      setMode((Mode)((_mode + 1) % ModeCount));
      button1Down = true;
    }
  } else {
    button1Down = false;
  }
}
