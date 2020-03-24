
#ifndef CONFIG_H
#define CONFIG_H

/* Hardware Configuration */
#define ARDUINO_DUE 0
#define DEVELOPER_BOARD 0

// Strand/cpu types
#define MEGA_WS2811   0  // triggers use of mega-specific assembly
#define ARDUINO_TCL   0  // Total Control Lighting (tm)
#define TEENSY 1
#define FASTLED_PIXEL_TYPE WS2811

#define FAST_LED_PIN_1 11
#define FAST_LED_PIN_2 14

/* Logging */
#define SERIAL_LOGGING 1
#define DEBUG 0

/* Options */
// #define TEST_MODE (ModeAccumulator)
#define MODE_TIME (80)
#define DEFAULT_BRIGHNESS 1.0

#if DEVELOPER_BOARD
static const bool kHasDeveloperBoard = true;
#else
static const bool kHasDeveloperBoard = false;
#endif

#define FAST_LED      (TEENSY)

/* For Developer Board */
#if DEVELOPER_BOARD
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SPEED_DIAL TCL_POT4
//#define SOUND_DIAL TCL_POT2
#endif

#endif // CONFIG_H
