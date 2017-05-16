// Pulse delay controlled by potentiometer
// Rotates modes every minute or so
// Since delay is constant across modes, this won't change from calm to frantic suddenly
// Majority of LEDs should be on at all times to avoid not lighting area enough


// -----------------------------------------
//
// Pattern Ideas: 
// A pattern that just fades to a color, and stays there for a few seconds. It's interesting to have the world be a solid color and stay still, too.
//     Bonus points if the fade got creative sometimes, and went pixel-at-a-time, 
//     or did a bi-directional wipe, flashed away (party mode), or some other cool effect.
// A pattern like Accumulator but with wider sweeps of color, maybe less blending.
// -----------------------------------------
//
// TODOs!: 
// * Don't use enum count for picking from the mode list. Just put all the modes in the enum, and list them out for each deployment kind so I can toggle.
// * Refactor cruft in Scene.h so patterns are actually modularized.
// * Get rid of "Twinkle." It sucks. Replace it with something good.
// -----------------------------------------
//
#include <Wire.h>
static const unsigned int LED_COUNT = 100;

#include <SPI.h>
#include <TCL.h>

#include "Config.h"
#include "Utilities.h"
#include "Color.h"
#include "Light.h"
#include "Scene.h"
#include "WS2811.h"

static Scene *gLights;

void setup()
{
#if SERIAL_LOGGING
  int baud = 9600;
  Serial.begin(baud);
  logf("%ul millis: Serial logging started at %i baud", millis(), baud);
#endif

#if ARDUINO_TCL && ARDUINO_DUE
  // The Due is much faster, needs a higher clock divider to run the SPI at the right rate.
  // ATMega runs at clock div 2 for 4 MHz, the Due runs at 84 MHz, so needs clock div 42 for 4 MHz.
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(42);
#elif ARDUINO_TCL
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
}

void loop()
{
#if SERIAL_LOGGING
  static unsigned int framesPast = 0;
  static unsigned long lastPrint = 0;
  
  unsigned long mils = millis();
  
  if (mils - lastPrint > 4000) {
    float framerate = 1000.0 * framesPast / (mils - lastPrint);
    logf("Framerate: %0.2f", framerate);
    lastPrint = mils;
    framesPast = 0;
  } else {
    framesPast++;
  }
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

