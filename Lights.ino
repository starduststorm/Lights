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
//
// TODO: Don't use enum count for picking from the mode list. Just put all the modes in the enum, and list them out for each deployment kind so I can toggle.
//

static const unsigned int LED_COUNT = 100;

#include <SPI.h>
#include <TCL.h>

#import "Config.h"
#import "Utilities.h"
#import "Color.h"
#import "Light.h"
#import "Scene.h"
#import "WS2811.h"

static Scene *gLights;

void setup()
{
#if SERIAL_LOGGING
  int baud = 9600;
  Serial.begin(baud);
  logf("%ul millis: Serial logging started at %i baud", millis(), baud);
#endif

#if ARDUINO_DUE
  // The Due is much faster, needs a higher clock divider to run the SPI at the right rate.
  // ATMega runs at clock div 2 for 4 MHz, the Due runs at 84 MHz, so needs clock div 42 for 4 MHz.
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(42);
#elif !WS2811
  TCL.begin();
#endif
  
#if DEVELOPER_BOARD
  TCL.setupDeveloperShield();
#endif
  
  fast_srand();
  
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
#if SERIAL_LOGGING
  static unsigned int loopCount2 = 0;
  if (loopCount2 % 100 == 0)
    logf("loop #%i", loopCount2);
  loopCount2++;
#endif
  
#if DEBUG && SERIAL_LOGGING && !ARDUINO_DUE
  static int loopCount = 0;
  if (loopCount++ > 1000) {
    Serial.print("Memory free: ");
    Serial.print(get_free_memory());
    Serial.println(" bytes");
    loopCount = 0;
  }
#endif
  gLights->tick();
}

