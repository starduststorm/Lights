
#ifndef CONFIG_H
#define CONFIG_H

/* Hardware Configuration */
#define ARDUINO_DUE 0
#define DEVELOPER_BOARD 0

/* For Developer Board */
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SPEED_DIAL TCL_POT4
//#define SOUND_DIAL TCL_POT2

// Strand/cpu types
#define MEGA_WS2811   0  // triggers use of mega-specific assembly
#define ARDUINO_TCL   0  // Total Control Lighting (tm)
#define TEENSY_WS2812 1  // 

#define TEENSY_PIN 2

/* Logging */
#define SERIAL_LOGGING 1
#define DEBUG 0

/* Options */
//#define TEST_MODE (ModeRainbow)
#define TRANSITION_TIME (10)
#define FRAME_DURATION 80
#define DEFAULT_BRIGHNESS 0.5

#if DEVELOPER_BOARD
static const bool kHasDeveloperBoard = true;
#else
static const bool kHasDeveloperBoard = false;
#endif

#endif // CONFIG_H

