
#include "Arduino.h"
#include "Color.h"

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

bool ColorIsNoColor(Color c)
{
  return c.filler != 0;
}

// Transition and intensity are both in the range [0, 100]
Color ColorWithInterpolatedColors(Color c1, Color c2, int transition, int intensity)
{
  // This is all integer math for speediness
  byte r, g, b;
  r = c1.red - transition * c1.red / 100 + transition * c2.red / 100;
  r = intensity * r / 100;
  g = c1.green - transition * c1.green / 100 + transition * c2.green / 100;
  g = intensity * g / 100;
  b = c1.blue - transition * c1.blue / 100 + transition * c2.blue / 100;
  b = intensity * b / 100;
  
  return MakeColor(r, g, b);
}

