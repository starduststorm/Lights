
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
  LightTransitionEaseIn,
  LightTransitionEaseOut,
} LightTransitionCurve;


class Light {
public:
  Light();
  
  Color color;
  Color targetColor;
  Color originalColor;
  
  float progress; // [0, 1]
  float duration; // in seconds
  LightTransitionCurve curve;
  
  void transitionToColor(Color transitionTargetColor, float rate, LightTransitionCurve curve);
  void transitionToColor(Color transitionTargetColor, float rate);
  void stopTransition();
  void transitionTick(unsigned long milliseconds);
  bool isTransitioning();
#if SERIAL_LOGGING
  void printDescription();
#endif
  
  int modeState; // For the Scene mode to use to store state
};

Light::Light() : duration(0), progress(1.0)
{
}

// FIXME: Add a special transition mode which ignores global speed, use for lightning bugs, power switch, etc.

void Light::transitionToColor(Color transitionTargetColor, float durationInSeconds, LightTransitionCurve curve)
{
  if (durationInSeconds <= 0) {
    durationInSeconds = 1;
  }
  targetColor = transitionTargetColor;
  originalColor = color;
  duration = durationInSeconds;
  progress = 0;
  curve = curve;
}

void Light::transitionToColor(Color transitionTargetColor, float duration)
{
  transitionToColor(transitionTargetColor, duration, LightTransitionLinear);
}

void Light::stopTransition()
{
  progress = 1.0;
}

void Light::transitionTick(unsigned long milliseconds)
{
  if (progress < 1.0) {
    progress = MIN(progress + milliseconds / (duration * 1000), 1.0);
    float curvedTransitionProgress = progress;
    
    switch (curve) {
      case LightTransitionLinear:
        break;
      case LightTransitionEaseIn:
        curvedTransitionProgress *= curvedTransitionProgress;
        break;
      case LightTransitionEaseOut:
        curvedTransitionProgress = 1 - curvedTransitionProgress;
        curvedTransitionProgress *= curvedTransitionProgress;
        curvedTransitionProgress = 1 - curvedTransitionProgress;
        break;
    }
    
    color.red = originalColor.red + curvedTransitionProgress * (targetColor.red - originalColor.red);
    color.green = originalColor.green + curvedTransitionProgress * (targetColor.green - originalColor.green);
    color.blue = originalColor.blue + curvedTransitionProgress * (targetColor.blue - originalColor.blue);
    
    if (progress >= 1) {
      if (!ColorIsEqualToColor(color, targetColor)) {
        logf("Not equal!, progress = %f, curvedprogress = %f, color = (%i, %i, %i)", progress, curvedTransitionProgress, (int)color.red, (int)color.green, (int)color.blue);
      }
      progress = 1.0;
    }
  }
}

bool Light::isTransitioning()
{
  return (progress < 1.0);
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
