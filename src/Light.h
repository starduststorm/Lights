
#if FAST_LED
#include "FastLED.h"
#endif

/* Tools */
#ifndef MIN
#define MIN(x, y) ((x) > (y) ? y : x)
#endif

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? y : x)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MOD_DISTANCE(a, b, m) (abs(m / 2. - fmod((3 * m) / 2 + a - b, m)))

typedef enum {
  LightTransitionLinear = 0,
  LightTransitionEaseInOut,
} LightTransitionCurve;


class Light {
public:
  Light();
  
  Color color;
  Color targetColor;
  Color originalColor;
  
  unsigned long duration; // in millis
  unsigned long transitionStart;
  LightTransitionCurve curve;
  
  void transitionToColor(Color transitionTargetColor, int durationMillis, LightTransitionCurve curve);
  void transitionToColor(Color transitionTargetColor, int durationMillis);
  void stopTransition();
  void transitionTick(unsigned long milliseconds);
  bool isTransitioning();
#if SERIAL_LOGGING
  void printDescription();
#endif
  
  int modeState; // For the Scene mode to use to store state
};

Light::Light() : duration(0), transitionStart(0)
{
}

// FIXME: Add a special transition mode which ignores global speed, use for lightning bugs, power switch, etc.

void Light::transitionToColor(Color transitionTargetColor, int durationMillis, LightTransitionCurve curve)
{
  if (durationMillis <= 0) {
    durationMillis = 1;
  }
  targetColor = transitionTargetColor;
  originalColor = color;
  duration = durationMillis;
  transitionStart = millis();
  curve = curve;
}

void Light::transitionToColor(Color transitionTargetColor, int durationMillis)
{
  transitionToColor(transitionTargetColor, durationMillis, LightTransitionLinear);
}

void Light::stopTransition()
{
  transitionStart = 0;
}

void Light::transitionTick(unsigned long milliseconds)
{
  if (isTransitioning()) {
    uint8_t progress = min((uint8_t)0xFF, 0xFF * (millis() - transitionStart) / duration);
    uint8_t curvedTransitionProgress = progress;
    
    switch (curve) {
      case LightTransitionLinear:
        break;
      case LightTransitionEaseInOut:
        curvedTransitionProgress = ease8InOutQuad(progress);
        break;
    }
    
    color.red = lerp8by8(originalColor.red, targetColor.red,  curvedTransitionProgress);
    color.green = lerp8by8(originalColor.green, targetColor.green, curvedTransitionProgress);
    color.blue = lerp8by8(originalColor.blue, targetColor.blue, curvedTransitionProgress);
    
    if (progress == 0xFF) {
      if (!ColorIsEqualToColor(color, targetColor)) {
        logf("Not equal!, progress = %u, curvedprogress = %u, color = (%i, %i, %i)", progress, curvedTransitionProgress, (int)color.red, (int)color.green, (int)color.blue);
      }
      transitionStart = 0;
    }
  }
}

bool Light::isTransitioning()
{
  return (transitionStart != 0);
}

#if SERIAL_LOGGING
void Light::printDescription()
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
