#include <SPI.h>
#include <TCL.h>

// Pulse delay controlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough


// -----------------------------------------
// Pattern Ideas:
// 
// A pattern that just fades to a color, and stays there for a few seconds. It's interesting to have the world be a solid color and stay still, too.
//     Bonus points if the fade got creative sometimes, and went pixel-at-a-time, 
//     or did a bi-directional wipe, flashed away (party mode), or some other cool effect.
// -----------------------------------------


/* Hardware Configuration */
#define ARDUINO_DUE 1
#define DEVELOPER_BOARD 0

/* For Developer Board */
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SOUND_DIAL TCL_POT4

/* Logging */
#define SERIAL_LOGGING 1
#define DEBUG 0

/* Options */
#define TEST_MODE (ModeFollow)
#define TRANSITION_TIME (10)
#define FRAME_DURATION 20

static const unsigned int LED_COUNT = 100;

#import "Utilities.h"
#import "Color.h"
#import "Light.h"
#import "Scene.h"

void setup()
{
#if SERIAL_LOGGING
  int baud = 9600;
  Serial.begin(baud);
  logf("START");
//  Serial.println("START");
//  Serial.print(millis());
//  Serial.print(" millis: Serial logging started at ");
//  Serial.print(baud);
//  Serial.println(" baud");
  logf("%ul millis: Serial logging started at %i baud", millis(), baud);
#endif

#if ARDUINO_DUE
  // The Due is much faster, needs a higher clock divider to run the SPI at the right rate.
  // ATMega runs at clock div 2 for 4 MHz, the Due runs at 84 MHz, so needs clock div 42 for 4 MHz.
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(42);
#else
  TCL.begin();
#endif
  
#if DEVELOPER_BOARD
  TCL.setupDeveloperShield();
#endif
  
  gLights = new Scene(LED_COUNT);
#ifdef TEST_MODE
  gLights->setMode(TEST_MODE);
#else
  gLights->setMode(gLights->randomMode());
#endif
  gLights->frameDuration = 100;
}

void loop()
{
  static int lead = 0;
  static int c = 0;
  Color color = NamedRainbow[c];
  TCL.sendEmptyFrame();
  for (int i = 0; i < 100; ++i) {
    float distance = ((lead - i + 100) % 100) / 100.0;
    
    float red = color.red * (1 - distance);
    float green = color.green *  (1 - distance);
    float blue = color.blue * (1 - distance);
    
    TCL.sendColor(min(red, 255), min(green, 255), min(blue, 255));
  }
  TCL.sendEmptyFrame();
  lead++;
  if (lead >= 100) {
    lead = 0;
    Color newColor = NamedRainbow[c];
    c = (c + 1) % ARRAY_SIZE(NamedRainbow);
    
    logf("NEXT COLOR index %i/%i = (%i, %i, %i)", (int)(c + 1), (int)ARRAY_SIZE(NamedRainbow), (int)newColor.red, (int)newColor.green, (int)newColor.blue);
    logf("Red = (%i, %i, %i)", (int)kRedColor.red, (int)kRedColor.green, (int)kRedColor.blue);
    logf("sizeof(Color) = %i", (int)sizeof(Color));
  }
  
  return;
  
  
  
#if SERIAL_LOGGING
  static int loopCount2 = 0;
  if (loopCount2 % 100 == 0)
    logf("loop #%i", loopCount2);
  loopCount2++;
#endif
  
#if DEBUG && SERIAL_LOGGING
  static int loopCount = 0;
  if (loopCount++ > 1000) {
    Serial.print("Memory free: ");
    Serial.print(get_free_memory());
    Serial.println(" bytes");
    loopCount = 0;
  }
#endif
  
  gLights->tick();
  logf("got to end of tick");
}

