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
// * Refactor cruft in Scene.h so patterns are actually modularized, just subclass so they can store their own mode vars
// * Factor our light transitions from Light.h and put them in an animation class that can handle multiple lights at a time. Goal: Interferring waves should not have to crossfade
// * Get rid of "Twinkle." It sucks. Replace it with something good.
// * Pattern with several follow leads traveling around in various directions and auto colors, colors are blended additively.
// * 
// -----------------------------------------
//
#include <Wire.h>

#include <SPI.h>

#include "Config.h"
#include "Utilities.h"
#include "Color.h"
#include "Light.h"
#include "Scene.h"
#include "WS2811.h"

#if ARDUINO_TCL
#include <TCL.h>
#endif

#define UNCONNECTED_PIN_1 A9
#define UNCONNECTED_PIN_2 A3

static Scene *gLights;

#if WAIT_FOR_SERIAL
static bool serialTimeout = false;
#endif
// static unsigned long setupDoneTime;

void setup() {
  int serialBaud = 57600;
#if SERIAL_BAUD
  serialBaud = SERIAL_BAUD;
#endif
  Serial.begin(serialBaud);
#if WAIT_FOR_SERIAL
  long setupStart = millis();
  while (!Serial) {
    if (millis() - setupStart > 5000) {
      serialTimeout = true;
      break;
    }
  }
  #if MEGA
    // mega can't print floats? lol
    logf("^^^ begin - waited %is for Serial", (int)((millis() - setupStart) / 1000.));
#else
    logf("^^^ begin - waited %0.2fs for Serial", (millis() - setupStart) / 1000.);
#endif
  
#elif DEBUG
  delay(2000);
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
  randomSeed(lsb_noise(UNCONNECTED_PIN_1, 8 * sizeof(uint32_t)));
  random16_add_entropy(lsb_noise(UNCONNECTED_PIN_2, 8 * sizeof(uint16_t)));
  
  gLights = new Scene(LED_COUNT);
#ifdef TEST_MODE
  gLights->setMode(TEST_MODE);
#else
  gLights->setMode(gLights->randomMode());
#endif
}

void loop()
{
  static unsigned int framesPast = 0;
  static unsigned long lastFrameratePrint = 0;
  
  unsigned long mils = millis();
  
  if (mils - lastFrameratePrint > 4000) {
    float framerate = 1000.0 * framesPast / (mils - lastFrameratePrint);
#if MEGA
    // mega can't print floats? lol
    logf("Framerate: %i", (int)framerate);
#else
    logf("Framerate: %0.2f", framerate);
#endif
    lastFrameratePrint = mils;
    framesPast = 0;
  } else {
    framesPast++;
  }
  
#if DEBUG && MEGA
  static unsigned long lastMemoryPrint = 0;
  if (mils - lastMemoryPrint > 10000) {
    Serial.print("Memory free: ");
    Serial.print(get_free_memory());
    Serial.println(" bytes");
    lastMemoryPrint = mils;
  }
#endif
  gLights->tick();
}
