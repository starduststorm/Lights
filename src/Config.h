
#ifndef CONFIG_H
#define CONFIG_H

/* Hardware Configuration */
#define ARDUINO_DUE 0

#ifndef TCL_h
// constants from TCL.h, the original Total Control Lighting library
#define TCL_POT1 A0
#define TCL_POT2 A1
#define TCL_POT3 A2
#define TCL_POT4 A3
#define TCL_MOMENTARY1 4
#define TCL_MOMENTARY2 5
#define TCL_SWITCH1 6
#define TCL_SWITCH2 7
#endif

// Strand/cpu types
#define MEGA_WS2811   0  // triggers use of mega-specific assembly
#define ARDUINO_TCL   0  // Total Control Lighting (tm)
#ifndef FASTLED_PIXEL_TYPE
#define FASTLED_PIXEL_TYPE WS2811
#endif

// AVR does not support e.g. std::string
#define USE_STL TEENSY

/* Logging */
#define DEBUG 0
#define WAIT_FOR_SERIAL 0

/* Options */
// #define TEST_MODE (ModeParity)
#define MODE_TIME (80)
#define DEFAULT_BRIGHNESS 0xFF

#if DEVELOPER_BOARD
static const bool kHasDeveloperBoard = true;
#else
static const bool kHasDeveloperBoard = false;
#endif

#define FAST_LED      (!ARDUINO_TCL)

/* For Developer Board */
#if DEVELOPER_BOARD
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SPEED_DIAL TCL_POT4
#endif

#endif // CONFIG_H
