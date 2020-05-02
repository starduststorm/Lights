#ifndef COLORMAKER_H
#define COLORMAKER_H

#include "Color.h"

class ColorMaker {
public:
  ColorMaker();
  ~ColorMaker();

  unsigned int getColorCount() {
    return count;
  }
  void prepColors(unsigned int count, unsigned long duration);
  Color getColor(unsigned int index);
  uint8_t fadeProgress(int index);
  void tick();
  
  void reset();

private:
  unsigned long duration; // in millis
  unsigned int count;
  
  Color *colors = NULL;
  Color *colorTargets = NULL;
  unsigned long *colorStarts = NULL;
  Color *colorCache = NULL;
  bool *colorCacheHits = NULL;
};

ColorMaker::ColorMaker()
{
  
}

ColorMaker::~ColorMaker()
{
  this->reset();
}

void ColorMaker::prepColors(unsigned int count, unsigned long duration) // duration per color target
{
  this->reset();
  this->count = count;
  this->duration = duration;
  
  if (count > 0) {
    colors = (Color *)malloc(count * sizeof(Color));
    colorTargets = (Color *)malloc(count * sizeof(Color));
    colorStarts = (unsigned long *)malloc(count * sizeof(unsigned long));
    colorCache = (Color *)malloc(count * sizeof(Color));
    colorCacheHits = (bool *)malloc(count * sizeof(bool));

    for (unsigned int i = 0; i < count; ++i) {
      colors[i] = NamedRainbow.randomColor();
      colorTargets[i] = NamedRainbow.randomColor();
      colorStarts[i] = millis();
      colorCache[i] = kBlackColor;
      colorCacheHits[i] = false;
    }
  }
}

uint8_t ColorMaker::fadeProgress(int index) {
  uint8_t progress = min((uint8_t)0xFF, 0xFF * (millis() - colorStarts[index]) / duration);
  return progress;
}

Color ColorMaker::getColor(unsigned int index)
{
#if DEBUG_ASSERT
  if (index >= this->count) {
    logf("GETTING OUT OF BOUNDS COLOR at %u >= %u", index, this->count);
  }
#endif
  if (colorCacheHits[index] == false) {
    colorCache[index] = ColorWithInterpolatedColors(colors[index], colorTargets[index], fadeProgress(index), 0xFF);
    colorCacheHits[index] = true;
  }
  return colorCache[index];
}

void ColorMaker::tick()
{
  for (unsigned int i = 0; i < count; ++i) {
    uint8_t progress = fadeProgress(i);
    if (progress == 0xFF) {
      colorStarts[i] = millis();
      colors[i] = colorTargets[i];
      colorTargets[i] = NamedRainbow.randomColor();
    }
    colorCache[i] = kBlackColor;
    colorCacheHits[i] = false;
  }
}
  
void ColorMaker::reset()
{
  free(colors);
  colors = NULL;
  free(colorTargets);
  colorTargets = NULL;
  free(colorStarts);
  colorStarts = NULL;
  free(colorCache);
  colorCache = NULL;
  free(colorCacheHits);
  colorCacheHits = NULL;

  count = 0;
}

#endif // COLORMAKER_H
