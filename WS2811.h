
#import "Config.h"

#if WS2811

#define DIGITAL_PIN   (51)         // Digital port number
#define PORT          (PORTB)     // Digital pin's port
#define PORT_PIN      (PORTB2)    // Digital pin's bit position

class WS2811Renderer {
private:
  uint8_t* pixelBuffer = NULL;
  unsigned int numPixels;
public:
  WS2811Renderer(unsigned int numPixels);
  ~WS2811Renderer();
  void setPixel(unsigned int index, byte red, byte green, byte blue);
  void render();
};

#endif

