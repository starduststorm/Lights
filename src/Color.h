#ifndef COLOR_H
#define COLOR_H

#include <FastLED.h>
#if USE_STL
#include <string>
#endif

struct Color {
  byte red;
  byte green;
  byte blue;

  Color() {
    red = green = blue = 0;
  }

  Color(CRGB crgb) {
    red = crgb.r;
    green = crgb.g;
    blue = crgb.b;
  }

  Color(byte r, byte g, byte b) {
    red = r;
    green = g;
    blue = b;
  }

#if USE_STL
  std::string description() {
    char buf[16];
    snprintf(buf, 16, "(%u, %u, %u)", red, green, blue);
    std::string desc = buf;
    return desc;
  }
#endif
};
typedef struct Color Color;

static const Color kBlackColor(0,0,0);
static const Color kRedColor(0xFF,0,0);
static const Color kOrangeColor(0xFF,0x60,0);
static const Color kYellowColor(0xFF, 0xFF, 0);
static const Color kGreenColor(0,0xFF,0);
static const Color kCyanColor(0, 0xFF, 0xFF);
static const Color kBlueColor(0,0, 0xFF);
static const Color kIndigoColor(0x4B, 0, 0x82);
static const Color kVioletColor(0x8E, 0x25, 0xFB);
static const Color kMagentaColor(0xFF, 0, 0xFF);
static const Color kWhiteColor(0xFF, 0xFF, 0xFF);

struct Color MakeColor(byte r, byte g, byte b);
static const Color kNightColor = MakeColor(0, 0, 0x10);

bool ColorIsEqualToColor(Color c1, Color c2);

Color ColorWithInterpolatedColors(Color c1, Color c2, uint8_t transition, uint8_t intensity);

bool ColorTransitionWillProduceWhite(Color c1, Color c2);

class Palette {
public:
  Palette(unsigned int count, ...);
  ~Palette();
  Color randomColor();
  Color getColor(float location);
  unsigned int count;
private:
  Color *colors;
};

extern Palette RGBRainbow;
extern Palette NamedRainbow;
extern Palette ROYGBIVRainbow;

#endif // COLOR_H

