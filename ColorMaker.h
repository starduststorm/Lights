#ifndef COLORMAKER_H
#define COLORMAKER_H

#include "Color.h"

class ColorMaker {
public:
  ColorMaker();
  ~ColorMaker();

  void prepColors(unsigned int count, float rate);
  Color getColor(unsigned int index);
  void tick();
  
  void reset();

  Color palette[];
private:
  float rate;
  unsigned int count;
  
  Color *colors = NULL;
  Color *colorTargets = NULL;
  float *colorProgress = NULL; // 0-100%
  Color *colorCache = NULL;
};

ColorMaker::ColorMaker()
{
  
}

ColorMaker::~ColorMaker()
{
  this->reset();
}

void ColorMaker::prepColors(unsigned int count, float rate)
{
  this->reset();
  this->count = count;
  this->rate = rate;
  
  if (count > 0) {
    colors = (Color *)malloc(count * sizeof(Color));
    colorTargets = (Color *)malloc(count * sizeof(Color));
    colorProgress = (float *)malloc(count * sizeof(float));
    colorCache = (Color *)malloc(count * sizeof(Color));
    
    for (int i = 0; i < count; ++i) {
      colors[i] = NamedRainbow.randomColor();
      colorTargets[i] = NamedRainbow.randomColor();
      colorProgress[i] = 0;
      colorCache[i] = kNoColor;
    }
  }
}

Color ColorMaker::getColor(unsigned int index)
{
  if (ColorIsNoColor(colorCache[index])) {
    colorCache[index] = ColorWithInterpolatedColors(colors[index], colorTargets[index], colorProgress[index], 100);
  }
  return colorCache[index];
}

void ColorMaker::tick()
{
  for (int i = 0; i < count; ++i) {
    if (colorProgress[i] == 0 || colorProgress[i] >= 100) {
      colorProgress[i] = 0;
      colors[i] = colorTargets[i];
      colorTargets[i] = NamedRainbow.randomColor();
    }
    colorProgress[i] += rate;
    colorCache[i] = kNoColor;
  }
}
  
void ColorMaker::reset()
{
  free(colors);
  colors = NULL;
  free(colorTargets);
  colorTargets = NULL;
  free(colorProgress);
  colorProgress = NULL;
  free(colorCache);
  colorCache = NULL;

  count = 0;
}

#endif // COLORMAKER_H

