
#include <FastLED.h>

#include "Arduino.h"
#include "Color.h"
#include "Utilities.h"

Palette RGBRainbow(6, kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor);
Palette NamedRainbow(9, kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kIndigoColor, kVioletColor, kMagentaColor);
Palette ROYGBIVRainbow(7, kRedColor, kOrangeColor, kYellowColor, kGreenColor, kBlueColor, kIndigoColor, kVioletColor);

struct Color MakeColor(byte r, byte g, byte b)
{
  Color c;
  c.red = r;
  c.green = g;
  c.blue = b;
  return c;
}

bool ColorIsEqualToColor(Color c1, Color c2)
{
  return (c1.red == c2.red && c1.green == c2.green && c1.blue == c2.blue);
}

Color ColorWithInterpolatedColors(Color c1, Color c2, uint8_t transition, uint8_t intensity)
{
  byte r, g, b;
  r = c1.red - transition * c1.red / 0xFF + transition * c2.red / 0xFF;
  r = intensity * r / 0xFF;
  g = c1.green - transition * c1.green / 0xFF + transition * c2.green / 0xFF;
  g = intensity * g / 0xFF;
  b = c1.blue - transition * c1.blue / 0xFF + transition * c2.blue / 0xFF;
  b = (int)intensity * b / 0xFF;
  
  return MakeColor(r, g, b);
}

bool ColorTransitionWillProduceWhite(Color c1, Color c2)
{
  byte halfRed = ((int)(c1.red + c2.red) >> 1);
  byte halfGreen = ((int)(c1.green + c2.green) >> 1);
  byte halfBlue = ((int)(c1.blue + c2.blue) >> 1);
  
  if (halfRed > 50 && halfGreen > 50 && halfBlue > 50) {
    return true;
  }
  return false;
}

Palette::Palette(unsigned int count, ...)
{
  colors = (Color *)malloc(count * sizeof(Color));

  this->count = count;

  va_list args;
  va_start(args, count);
  
  for (unsigned int i = 0; i < count; ++i) {
    colors[i] = va_arg(args, Color);
  }
  
  va_end(args);
}

Palette::~Palette()
{
  delete colors;
}

Color Palette::randomColor()
{
  return colors[fast_rand(count)];
}

Color Palette::getColor(float location)
{
  // FIXME: replace with FastLED palettes
  // ensure location is positive and in range
  location = fmodf(fmodf(location, count) + count, count);
  
  if ((int)location == location) {
    return colors[(int)location];
  } else {
    float index;
    float fraction = modff(location, &index);
    Color c1 = colors[(int)index];
    Color c2 = colors[(int)index + 1];
    return ColorWithInterpolatedColors(c1, c2, 0xFF * fraction, 0xFF);
  }
}

