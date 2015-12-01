


/* Hardware Configuration */
#define ARDUINO_DUE 0
#define DEVELOPER_BOARD 0

/* For Developer Board */
#define BRIGHTNESS_DIAL TCL_POT3
#define MODE_DIAL TCL_POT1
#define SPEED_DIAL TCL_POT4
//#define SOUND_DIAL TCL_POT2
#define WS2811 (true) // if False, assume we can use TCL

/* Logging */
#define SERIAL_LOGGING 1
#define DEBUG 1

/* Options */
#define TEST_MODE (ModeRainbow)
#define TRANSITION_TIME (80)
#define FRAME_DURATION 80

#if DEVELOPER_BOARD
static const bool kHasDeveloperBoard = true;
#else
static const bool kHasDeveloperBoard = false;
#endif

