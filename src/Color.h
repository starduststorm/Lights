
#ifndef COLOR_H
#define COLOR_H

struct Color {
  byte red;
  byte green;
  byte blue;
  byte filler;
};
typedef struct Color Color;

static const Color kBlackColor   = (Color){.red=0,    .green=0,    .blue=0   };
static const Color kRedColor     = (Color){.red=0xFF, .green=0,    .blue=0   };
static const Color kOrangeColor  = (Color){.red=0xFF, .green=0x60, .blue=0   };
static const Color kYellowColor  = (Color){.red=0xFF, .green=0xFF, .blue=0   };
static const Color kGreenColor   = (Color){.red=0,    .green=0xFF, .blue=0   };
static const Color kCyanColor    = (Color){.red=0,    .green=0xFF, .blue=0xFF};
static const Color kBlueColor    = (Color){.red=0,    .green=0,    .blue=0xFF};
static const Color kIndigoColor  = (Color){.red=0x4B, .green=0,    .blue=0x82};
static const Color kVioletColor  = (Color){.red=0x8E, .green=0x25, .blue=0xFB};
static const Color kMagentaColor = (Color){.red=0xFF, .green=0,    .blue=0xFF};
static const Color kWhiteColor   = (Color){.red=0xFF, .green=0xFF, .blue=0xFF};
static const Color kNoColor      = (Color){.red=0, .green=0, .blue=0, .filler=0xFF};

struct Color MakeColor(byte r, byte g, byte b);
static const Color kNightColor = MakeColor(0, 0, 0x10);

bool ColorIsEqualToColor(Color c1, Color c2);

bool ColorIsNoColor(Color c);
// Transition and intensity are both in the range [0, 100]
Color ColorWithInterpolatedColors(Color c1, Color c2, int transition, int intensity);

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

