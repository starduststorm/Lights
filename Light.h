
/* Tools */
#define MIN(x, y) ((x) > (y) ? y : x)
#define MAX(x, y) ((x) < (y) ? y : x)
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
  
  float transitionProgress; // [0, 1]
  float transitionRate; // percentage to transition per frame
  LightTransitionCurve transitionCurve;
  
  void transitionToColor(Color transitionTargetColor, float rate, LightTransitionCurve curve);
  void transitionToColor(Color transitionTargetColor, float rate);
  void transitionTick(float multiplier);
  bool isTransitioning();
#if SERIAL_LOGGING
  void printDescription();
#endif
  
  int modeState; // For the Scene mode to use to store state
};

Light::Light() : transitionRate(0)
{
}

void Light::transitionToColor(Color transitionTargetColor, float rate, LightTransitionCurve curve)
{
  if (rate <= 0) {
    rate = 1;
  }
  targetColor = transitionTargetColor;
  originalColor = color;
  transitionRate = rate / 100.0;
  transitionProgress = 0;
  transitionCurve = curve;
}

void Light::transitionToColor(Color transitionTargetColor, float rate)
{
  transitionToColor(transitionTargetColor, rate, LightTransitionLinear);
}

void Light::transitionTick(float multiplier)
{
  if (transitionRate > 0) {
    transitionProgress = MIN(transitionProgress + multiplier * transitionRate, 1.0);
    float curvedTransitionProgress = transitionProgress;
    
    switch (transitionCurve) {
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
    
    if (transitionProgress >= 1) {
      transitionRate = 0;
    }
  }
}

bool Light::isTransitioning()
{
  return (transitionRate != 0);
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
