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
static const Color kVioletColor  = (Color){.red=0x8E, .green=0x25, .blue=0xFB};
static const Color kMagentaColor = (Color){.red=0xFF, .green=0,    .blue=0xFF};
static const Color kWhiteColor   = (Color){.red=0xFF, .green=0xFF, .blue=0xFF};

Color RGBRainbow[] = {kRedColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kMagentaColor};
Color NamedRainbow[] = {kRedColor, kOrangeColor, kYellowColor, kGreenColor, kCyanColor, kBlueColor, kVioletColor, kMagentaColor};

static struct Color MakeColor(byte r, byte g, byte b)
{
  Color c;
  c.red = r;
  c.green = g;
  c.blue = b;
  return c;
}

static bool ColorIsEqualToColor(Color c1, Color c2)
{
  return (c1.red == c2.red && c1.green == c2.green && c1.blue == c2.blue);
}

// Transition and intensity are both in the range [0, 100]
static Color ColorWithInterpolatedColors(Color c1, Color c2, int transition, int intensity)
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

