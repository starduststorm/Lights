
#include "WS2811.h"
#include "Color.h"
#include "ColorMaker.h"
#include "Config.h"
#include "palettes.h"

#if ARDUINO_TCL
#include <TCL.h>
#endif

typedef enum {
  ModeWaves,
  ModeFire,
  ModeBlueFire,
  ModeGreenFire,
  ModePinkFire,
  ModeLightningBugs,
  ModeParity,
  ModeInterferingWaves,
  ModeRainbow,
  ModeAccumulator,
  ModeCount,
  // These are all either boring or need work.
  
  ModeTwinkle,
  ModeBoomResponder,
  ModeBounce,
} Mode;

static const bool kLightningBugsIsEasterEgg = false;

#if DEVELOPER_BOARD
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

static const float kSpeedMax = 3.0;
static const float kSpeedNormalMin = 0.5;
static const float kSpeedMin = 0.4;
static const SpeedRange kSpeedNormalRange = SpeedRangeMake(kSpeedNormalMin, kSpeedMax);

static SpeedRange kModeRanges[ModeCount] = {0};
#endif

static const unsigned int kInterferringWavesNum = LED_COUNT / 20;

class Scene {
private:
  unsigned int _lightCount=0;
  Mode _mode;
  uint32_t _modeStart=0;
  uint32_t _lastTick=0;
  
  Light **_lights;
  
#if MEGA_WS2811
  WS2811Renderer *ws2811Renderer;
#elif FAST_LED
  CRGBArray<LED_COUNT> leds;
#endif

  float _globalSpeed; // Multiplier for global follow and fade speed
  ColorMaker *_colorMaker = NULL;

  // "Follow" convenience counter
  float _followLeader=0;
  int _followSpeed; // number of lights lights / s that the _followLeader moves
  bool _directionIsReversed;
  
  // Mode specific data
  unsigned int _followColorIndex=0;
  Color *_colorScratch=NULL;
  float *_sceneVariation = NULL;
  unsigned int _soundPeak;
  float *_leaders = NULL; // for interfering waves
  unsigned long _timeMarker=0;
  
  PaletteRotation<CRGBPalette256> paletteRotation;

  //
  
  void transitionAll(Color c, int durationMillis);
  
  void updateStrand();
#if DEVELOPER_BOARD
  SpeedRange speedRangeForMode(Mode mode);
#endif

public:
  void applyAll(Color c);
  void tick();
  Scene(unsigned int ledCount);
  void setMode(Mode mode);
  ~Scene();
  Mode randomMode();
};

uint8_t getBrightness()
{
  if (!kHasDeveloperBoard) {
    return DEFAULT_BRIGHNESS;
  }
  static uint8_t brightnessAdjustment = 0xFF;
#if DEVELOPER_BOARD
  static int brightMin = 200;
  static int brightMax = 900;
  int val = analogRead(BRIGHTNESS_DIAL);
  if (val < brightMin) {
    brightMin = val;
  }
  if (val > brightMax) {
    brightMax = val;
  }
  val = map(val, 0, 1024, 0, 0xFF);
  
  if (abs(val - brightnessAdjustment) > 15) {
    brightnessAdjustment = val;
  }
#endif
  return brightnessAdjustment;
}

Color _adjustColorForScene(Color c, uint8_t brightness)
{
  if (brightness != 0xFF) {
    uint8_t dimmingFactor = dim8_raw(brightness);

    c.red = scale8(c.red, dimmingFactor);
    c.green = scale8(c.green, dimmingFactor);
    c.blue = scale8(c.blue, dimmingFactor);
  }
  
  return c;
}

void Scene::updateStrand()
{
  uint8_t brightnessAdjustment = getBrightness();
  
  // Update per-pixel
#if ARDUINO_TCL
  TCL.sendEmptyFrame();
#endif
  for (unsigned int i = 0; i < _lightCount; ++i) {
    Light *light = _lights[i];
    int red = light->color.red, green = light->color.green, blue = light->color.blue;
    
    Color color = _adjustColorForScene(light->color, brightnessAdjustment);
    Color targetColor = _adjustColorForScene(light->targetColor, brightnessAdjustment);
    Color sourceColor = _adjustColorForScene(light->originalColor, brightnessAdjustment);
    
    // FIXME: comment this
    // Nevermind, this is just a lame excuse for actual low-brightness dithering by clipping off low values if the source or target color has the same number of lit subpixels.
    
    if (light->isTransitioning()) {
      unsigned int numZeroComponents = (color.red == 0 ? 1 : 0)  + (color.green == 0 ? 1 : 0) + (color.blue == 0 ? 1 : 0);
      unsigned int targetNumZeroComponents = (targetColor.red == 0 ? 1 : 0)  + (targetColor.green == 0 ? 1 : 0) + (targetColor.blue == 0 ? 1 : 0);
      unsigned int sourceNumZeroComponents = (sourceColor.red == 0 ? 1 : 0)  + (sourceColor.green == 0 ? 1 : 0) + (sourceColor.blue == 0 ? 1 : 0);
      
      if (red + green + blue < 10  && (numZeroComponents == 1 || numZeroComponents == 2) && numZeroComponents != targetNumZeroComponents && numZeroComponents != sourceNumZeroComponents) {
        // Avoid fades like reddish-yellow -> black showing (1,0,0) == red right at the end of the fadeout
        red = green = blue = 0;
      }
    } else if (red + green + blue < 5) {
      red = green = blue = 0;
    }
      
#if ARDUINO_TCL
    TCL.sendColor(red, green, blue);
#elif MEGA_WS2811
    // Color corrections for the WS2811 strands I use
    red = min(1.1 * red, 255);
    
    ws2811Renderer->setPixel(i, red, green, blue);
#elif FAST_LED
    leds[i] = CRGB(red, green, blue);
#endif
  }

  // Send to strand
#if ARDUINO_TCL
  TCL.sendEmptyFrame();
#elif MEGA_WS2811
  ws2811Renderer->render();
#elif FAST_LED
#ifdef FAST_LED_PIN_2
  // Flip strand 2 to make pattern continuous
  CRGBArray<LED_COUNT/2> ledTemp;
  ledTemp(LED_COUNT/2-1, 0) = leds(LED_COUNT/2, LED_COUNT-1); // mirror
  leds(LED_COUNT/2, LED_COUNT-1) = ledTemp;
#endif
  FastLED.show();
#endif
}

void Scene::applyAll(Color c)
{
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->color = c;
  }
}

void Scene::transitionAll(Color c, int durationMillis)
{
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionToColor(c, durationMillis);
  }
}

#if DEVELOPER_BOARD
void setSpeedRangeForMode(SpeedRange speedRange, Mode mode)
{
  if (mode < ARRAY_SIZE(kModeRanges)) {
    kModeRanges[mode] = speedRange;
  }
}
#endif

Scene::Scene(unsigned int lightCount) : _mode((Mode)-1), _globalSpeed(1.0), paletteRotation(10)
{ 
#if DEVELOPER_BOARD
  setSpeedRangeForMode(SpeedRangeMake(0.7, 1.3), ModeFire);
  setSpeedRangeForMode(speedRangeForMode(ModeFire), ModeBlueFire);
  setSpeedRangeForMode(speedRangeForMode(ModeFire), ModePinkFire);
  setSpeedRangeForMode(speedRangeForMode(ModeFire), ModeGreenFire);
  
  setSpeedRangeForMode(SpeedRangeMake(kSpeedMin, kSpeedMin + 0.2), ModeLightningBugs);
#endif
  
  _lightCount = lightCount;
  _lights = new Light*[_lightCount];
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i] = new Light();
  }
  _lastTick = millis();
  
#if MEGA_WS2811
  ws2811Renderer = new WS2811Renderer(LED_COUNT);
#elif ARDUINO_TCL
  // nothing
#elif FAST_LED
#ifdef FAST_LED_PIN_2
  LEDS.addLeds<FASTLED_PIXEL_TYPE, FAST_LED_PIN_1, RGB>(leds, LED_COUNT/2, LED_COUNT/2);
  LEDS.addLeds<FASTLED_PIXEL_TYPE, FAST_LED_PIN_2, RGB>(leds, LED_COUNT/2);
#elif FAST_LED_PIN_1
  LEDS.addLeds<FASTLED_PIXEL_TYPE, FAST_LED_PIN_1, RGB>(leds, LED_COUNT);
#else
  LEDS.addLeds<FASTLED_PIXEL_TYPE, RGB>(leds, LED_COUNT);
#endif
  // LEDS.setCorrection(0xFF9090); // edit as needed per strand deployment
  LEDS.setBrightness(0xFF);
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

#if DEVELOPER_BOARD
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
#endif

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
    if (kLightningBugsIsEasterEgg && mode == ModeLightningBugs && millis() < kEightHours) {
      modeAllowed = false;
    }
#elif DEVELOPER_BOARD
    // We have a dial for speed, which includes and excludes various modes
    SpeedRange range = speedRangeForMode(mode);
    modeAllowed = (_globalSpeed >= range.low && _globalSpeed <= range.high);
#else
    // Lightning bugs not allowed because we can't dial away from it
    if (mode == ModeLightningBugs) {
      // modeAllowed = false;
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
  logf("Set mode %i->%i", _mode, mode);
  if (mode != _mode) {
    // Transition away from old mode
    switch (_mode) {
      case ModeLightningBugs:
        // When ending lightning bugs, have all the bugs go out
        transitionAll(kNightColor, 1000);
        break;
      default:
        break;
    }
    
    _mode = mode;
    
    _soundPeak = 0;
    
    int automaticColorsCount = 0;
    float automaticColorsDuration = 3000;
    
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

    paletteRotation.maxColorJump = 0xFF;
    
    switch (_mode) {
      case ModeLightningBugs:
        transitionAll(kNightColor, 1200);
        break;
      case ModeInterferingWaves:
        automaticColorsCount = kInterferringWavesNum;
        _sceneVariation = (float *)malloc(automaticColorsCount * sizeof(float));
        _leaders = (float *)malloc(automaticColorsCount * sizeof(float));
        for (int i = 0; i < automaticColorsCount; ++i) {
          _sceneVariation[i] = ((int)fast_rand(0, 80) - 40) / 4.0;
        }
        automaticColorsDuration = 5000;
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        break;
      case ModeWaves: {
        automaticColorsCount = fast_rand(2); // palettize half the time
        logf("  Waves submode %s", automaticColorsCount == 1 ? "1 color" : "palette");
        automaticColorsDuration = 6000;
        
        paletteRotation.secondsPerPalette = 20; 
        paletteRotation.minBrightness = 20;

        _sceneVariation = new float(fast_rand(3) == 0 ? 50 : 20); // wave size, assumes number of lights is roughly divisible by 50
        break;
      case ModeRainbow:
        _followColorIndex = fast_rand(ROYGBIVRainbow.count);
        break;
      case ModeAccumulator: {
        _colorScratch = (Color *)malloc(_lightCount * sizeof(Color));
        _sceneVariation = new float(fast_rand(2)); // palettize sometimes
        paletteRotation.secondsPerPalette = 20;
        break;
      }
      case ModeParity: {
        paletteRotation.secondsPerPalette = 8;
        paletteRotation.maxColorJump = 10;
        _followSpeed = 12;
        break;
      }
      case ModeTwinkle:
        for (unsigned i = 0; i < _lightCount; ++i) {
          Color color = ROYGBIVRainbow.randomColor();
          _lights[i]->transitionToColor(color, 1000);
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
  uint32_t time = millis();
#if DEVELOPER_BOARD
  uint32_t tickTime = MAX(1, (time - _lastTick) * _globalSpeed);
#else 
  uint32_t tickTime = MAX(1, (time - _lastTick));
#endif
  _lastTick = time;

  // Fade transitions
  for (unsigned int i = 0; i < _lightCount; ++i) {
    _lights[i]->transitionTick(tickTime);
  }
  
#if DEVELOPER_BOARD
  static bool allOff = false;
  static bool startedOffFade = false;
  if (kHasDeveloperBoard && digitalRead(TCL_SWITCH2) == LOW) {
    if (!startedOffFade) {
      for (unsigned int i = 0; i < _lightCount; ++i) {
        _lights[i]->transitionToColor(kBlackColor, 1000, LightTransitionEaseInOut);
      }
      startedOffFade = true;
    }
    if (!allOff) {
      updateStrand();
      if (!_lights[0]->isTransitioning()) {
        allOff = true;
      }
    } else {
      // Just sleep after we're done fading
      delay(100);
      // And set all to black periodically for any new strands that get attached, or lose and gain power.
      for (unsigned int i = 0; i < _lightCount; ++i) {
        _lights[i]->color = kBlackColor;
      }
      updateStrand();
    }
    return;
  } else {
    allOff = false;
    startedOffFade = false;
  }
#endif
  _followLeader += (_directionIsReversed ? -1 : 1) * (_followSpeed * tickTime / 1000.0);
  _followLeader = fmodf(_followLeader + _lightCount, _lightCount);

  _colorMaker->tick();
  
  switch (_mode) {
    case ModeFire:
    case ModeBlueFire:
    case ModeGreenFire:
    case ModePinkFire: {
      // Interpolate, fade, and snap between two colors
      Color palette[3];
      if (_mode == ModeGreenFire) {
        palette[0] = MakeColor(0x10, 0xFF, 0x0);
        palette[1] = MakeColor(0xA0, 0xFF, 0x0);
        palette[2] = MakeColor(0x0B, 0x66, 0x13);
      } else if (_mode == ModePinkFire) {
        palette[0] = MakeColor(0xFF, 0x0, 0xFF);
        palette[1] = MakeColor(0xBF, 0x0, 0xFF);
        palette[2] = MakeColor(0xF8, 0x18, 0x94);
      } else if (_mode == ModeBlueFire) {
        palette[0] = MakeColor(0x30, 0x10, 0xFF);
        palette[1] = MakeColor(0, 0xB0, 0xFF);
        palette[2] = MakeColor(0x1, 0xC0, 0xC0);
      } else {
        palette[0] = MakeColor(0xFF, 0x3C, 0);
        palette[1] = MakeColor(0xFF, 0x80, 0);
        palette[2] = MakeColor(0xDD, 0x60, 0x02);
      }
      for (unsigned int i = 0; i < _lightCount; ++i) {
        Light *light = _lights[i];
        if (!(light->isTransitioning())) {
          long choice = fast_rand(100);
          
          if (choice < 10) {
            // 10% of the time, fade slowly to black
            light->transitionToColor(kBlackColor, 500);
          } else {
            // Otherwise, fade or snap to another color
            Color new_color = palette[fast_rand(sizeof(palette)/sizeof(palette[0]))];
            if (choice < 95) {
              Color mixedColor = ColorWithInterpolatedColors(light->color, new_color, fast_rand(0x100), fast_rand(0x100));
              light->transitionToColor(mixedColor, 240);
            } else {
              light->color = new_color;
              // after setting the color, do a fade to this same color to keep the light "busy" for a short time.
              light->transitionToColor(new_color, 100);
            }
          }
        }
      }
      break;
    }
    
    case ModeLightningBugs: {
      // cycle the lightning bugs density over a minute
      unsigned int chance = 1400 + 1000 * sin(M_PI * time / 1000 / 60);
      for (unsigned int i = 0; i < _lightCount; ++i) {
        Light *light = _lights[i];
        if (!light->isTransitioning()) {
          switch (light->modeState) {
            case 1:
              // When putting a bug out, fade to black first, otherwise we fade from yellow(ish) to blue and go through white.
              light->transitionToColor(kBlackColor, 450, LightTransitionEaseInOut);
              light->modeState = 2;
              break;
            case 2:
              light->transitionToColor(kNightColor, 450);
              light->modeState = 0;
              break;
            default:
              if (fast_rand(chance) == 0) {
                // Blinky blinky
                light->transitionToColor(MakeColor(0xD0, 0xFF, 0), 350);
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
      _lights[(int)_followLeader]->transitionToColor(kBlackColor, 400);
      _followLeader = _followLeader + direction;
      _lights[(int)_followLeader]->color = RGBRainbow.randomColor();
      if (_followLeader == _lightCount - 1  || _followLeader == 0) {
        direction = -direction;
      }
      break;
    }
    
    case ModeWaves: {
      const unsigned int waveLength = (float)*_sceneVariation;
      // Needs to fade out over less than half a wave, so there are some off in the middle.
#if DEVELOPER_BOARD
      const int fadeDuration = 1000 * (waveLength / 2) / (_followSpeed * _globalSpeed);
#else
      const int fadeDuration = 1000 * (waveLength / 2) / (_followSpeed);
#endif
      
      Color waveColor = kBlackColor;
      if (_colorMaker->getColorCount() > 0) {
        waveColor = _colorMaker->getColor(0);
      }
      
      unsigned int waveCount = _lightCount / waveLength;
      for (unsigned int i = 0; i < waveCount; ++i) {
        if (_colorMaker->getColorCount() == 0) {
          waveColor = Color(paletteRotation.getPaletteColor(0xFF * i / waveCount));
        }
        unsigned int turnOnLeaderIndex = ((int)_followLeader + i * waveLength) % _lightCount;
        unsigned int turnOffLeaderIndex = ((int)_followLeader + i * waveLength - waveLength / 2 + _lightCount) % _lightCount;

        if (!_lights[turnOnLeaderIndex]->isTransitioning()) {
          _lights[turnOnLeaderIndex]->transitionToColor(waveColor, fadeDuration, LightTransitionEaseInOut);
        }
        if (!_lights[turnOffLeaderIndex]->isTransitioning()) {
          _lights[turnOffLeaderIndex]->transitionToColor(kBlackColor, fadeDuration * 0.75, LightTransitionEaseInOut);
        }
      }
      break;
    }
    
    case ModeRainbow: {
      const unsigned int waveLength = 7;
      const int fadeDuration = 1000 * (waveLength - 2) / (_followSpeed * _globalSpeed) * 0.9;
      
      for (unsigned int i = 0; i < _lightCount / waveLength; ++i) {
        unsigned int changeIndex = ((int)_followLeader + i * waveLength) % _lightCount;
        Color waveColor = ROYGBIVRainbow.getColor(_followColorIndex + i);
        waveColor = ColorWithInterpolatedColors(waveColor, kBlackColor, 0, 0xB0); // dim a little
        if (!_lights[changeIndex]->isTransitioning()) {
          _lights[changeIndex]->transitionToColor(waveColor, fadeDuration, LightTransitionEaseInOut);
        }
      }
      break;
    }
    
    case ModeInterferingWaves: {
      const int waveLength = 18;
      const int halfWave = waveLength / 2;
      
      // For the first 3 seconds of interfering waves, fade from previous mode
      static const int kFadeTime = 3000;
      unsigned long modeTime = millis() - _modeStart;
      bool inModeTransition = modeTime < kFadeTime;
      
      memset(_colorScratch, 0, _lightCount * sizeof(Color));
      float lightsChunk = _lightCount / (float)kInterferringWavesNum;
      for (unsigned int waveIndex = 0; waveIndex < kInterferringWavesNum; ++waveIndex) {
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
            
            uint8_t litRatio = (existingColor.red + existingColor.green + existingColor.blue) / 3;
            // If the existing light is less than about 3% lit, use the whole new color. Otherwise smoothly fade into splitting the difference.
            const uint8_t minLit = 0xFF * 0.03;
            const uint8_t normLit = 0xFF / 10;
            uint8_t additionalFade = (litRatio < minLit ? 0x7F : (litRatio > normLit ? 0 : (0x7F - 0x7F * litRatio / normLit)));
            
//              logf("Existing color = (%i, %i, %i), litRation = %f, additionalFade = %f", existingColor.red, existingColor.green, existingColor.blue, litRatio, additionalFade);
            
            uint8_t fadeProgress = (1 - distance / (float)halfWave) * (0x7F + additionalFade);
            Color color = ColorWithInterpolatedColors(existingColor, waveColor, fadeProgress, 0xFF);
            _colorScratch[lightIndex] = color;
            
            color.red = ease8InOutQuad(color.red);
            color.green = ease8InOutQuad(color.green);
            color.blue = ease8InOutQuad(color.blue);

            if (inModeTransition) {
              // Fade from previous mode
              color = ColorWithInterpolatedColors(_lights[lightIndex]->color, color, 0xFF * modeTime / kFadeTime, 0xFF);
            }
            
            _lights[lightIndex]->color = color;
          }
        }
      }
      // Black out all other lights
      for (unsigned int i = 0; i < _lightCount; ++i) {
        if (ColorIsEqualToColor(_colorScratch[i], kBlackColor)) {
          if (inModeTransition) {
            _lights[i]->color = ColorWithInterpolatedColors(_lights[i]->color, kBlackColor, 0xFF * modeTime / kFadeTime, 0xFF);
          } else {
            _lights[i]->color = kBlackColor;
          }
        }
      }
      break;
    }
    
    case ModeParity: {
      paletteRotation.tick();
      
      const int paletteRange = min(50u, _lightCount / 2);
      const int parityCount = 2;
      for (int i = 0; i < (int)_lightCount; ++i) {
        if (!_lights[i]->isTransitioning()) { // serves to not interrupt existing fades when this pattern starts
          int parity = i % parityCount;
          int paletteIndex = map(i + (parity ? paletteRange - _followLeader : _followLeader), 0, paletteRange, 0, 0x100);

          // to avoid palette discontinuities at the endpoints, "bounce" the palette so read up to 0xFF then back down to 0, then back up.
          paletteIndex = mod_wrap(paletteIndex, 0x200);
          if (paletteIndex > 0xFF) {
            paletteIndex = 0x1FF - paletteIndex;
          }

          Color targetColor = Color(paletteRotation.getPaletteColor(paletteIndex));

          long modeTime = millis() - _modeStart;
          long fadeTime = max(100, (2000 - modeTime) / 5);
          _lights[i]->transitionToColor(targetColor, fadeTime);
        }
      }
      break;
    }
    
    case ModeBoomResponder:
      for (unsigned int i = 0; i < _lightCount; ++i) {
        if (!_lights[i]->isTransitioning()) {
          _lights[i]->transitionToColor(NamedRainbow.randomColor(), 1000);
        }
      }
      break;
    
    case ModeAccumulator: {
      const int kernelWidth = 1;
      
      paletteRotation.tick();
      
  #if DEVELOPER_BOARD
      const unsigned int kPingInterval = 30000 / _lightCount / _globalSpeed;
      const unsigned int kBlurInterval = 50 / _globalSpeed;
  #else
      const unsigned int kPingInterval = 30000 / _lightCount;
      const unsigned int kBlurInterval = 50;
  #endif
      if (time - _timeMarker > kPingInterval) {
        unsigned int ping = fast_rand(_lightCount);
        Color c = kBlackColor;
        if (_sceneVariation && *_sceneVariation) {
          c = Color(paletteRotation.getPaletteColor(random8()));
        } else {
          c = NamedRainbow.randomColor();
        }

        for (unsigned int i = (ping - 1); i <= ping + 1; ++i) {
          unsigned int light = (i + _lightCount) % _lightCount;
          _lights[light]->transitionToColor(c, 250, LightTransitionEaseInOut);
        }
        
        _timeMarker = time;
      }
      
      static unsigned long lastBlur = 0;
      
      for (unsigned int i = 0; i < _lightCount; ++i) {
        Color c = _lights[i]->color;
        _colorScratch[i] = c;
      }
      
      if (time - lastBlur > kBlurInterval) {  
        for (unsigned int target = 0; target < _lightCount; ++target) {
          if (_lights[target]->isTransitioning()) {
            continue;
          }
          Color c = kBlackColor;
          unsigned int count = 0;
          
          float multiplier = 1.0;
          
          for (int k = -kernelWidth; k <= kernelWidth; ++k) {
            unsigned int source = (target + k + _lightCount) % _lightCount;
            Color sourceColor = _colorScratch[source];
            
            if (sourceColor.red + sourceColor.green + sourceColor.blue < 20) {
              continue;
            } else {
              c.red = (c.red * count + sourceColor.red) / (float)(count + 1);
              c.green = (c.green * count + sourceColor.green) / (float)(count + 1);
              c.blue = (c.blue * count + sourceColor.blue) / (float)(count + 1);
              ++count;
            }
          }

          c.red *= 0.92 * multiplier;
          c.green *= 0.92 * multiplier;
          c.blue *= 0.92 * multiplier;
          
          _lights[target]->transitionToColor(c, 200);
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
            _lights[i]->transitionToColor(targetColor, 1000);
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
  if (time - _modeStart > (uint32_t)MODE_TIME * 1000) {
    Mode nextMode = randomMode();
    logf("Timed mode change to %i", (int)nextMode);
    _modeStart = time; // in case mode doesn't actually change here.
    setMode(nextMode);
  } else {
#endif
#if DEVELOPER_BOARD
    float newGlobalSpeed = 1.0;
    newGlobalSpeed = (kHasDeveloperBoard ? PotentiometerReadf(SPEED_DIAL, kSpeedMin, kSpeedMax) : 1.0);

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
#endif
    }
#endif
#ifndef TEST_MODE
  }
#endif

#if DEVELOPER_BOARD
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
#endif
}
