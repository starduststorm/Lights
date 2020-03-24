
#include "Arduino.h"
#include "WS2811.h"
#include "Utilities.h"

#if MEGA_WS2811

WS2811Renderer::WS2811Renderer(unsigned int numPixels)
{
  this->numPixels = numPixels;
  pixelBuffer = (uint8_t *)malloc(numPixels * 3);
  memset(pixelBuffer, 0, numPixels * 3);
  
  pinMode(DIGITAL_PIN,OUTPUT);
  digitalWrite(DIGITAL_PIN,0);
}

WS2811Renderer::~WS2811Renderer()
{
  free(pixelBuffer);
}

void WS2811Renderer::setPixel(unsigned int index, byte red, byte green, byte blue)
{
  if (index < numPixels) {
    uint8_t *p = &pixelBuffer[index * 3]; 
    *p++ = red;  
    *p++ = green;
    *p = blue;      
  }
}

void WS2811Renderer::render()
{
  /*---------------------------------------------------------------------
  Acrobotic - 01/10/2013
  Author: x1sc0 
  Platforms: Arduino Uno R3
  File: bitbang_whitish.ino

  Description: 
  This code sample accompanies the "How To Control 'Smart' RGB LEDs: 
  WS2811, WS2812, and WS2812B (Bitbanging Tutorial)" Instructable 
  (http://www.instructables.com/id/Bitbanging-step-by-step-Arduino-control-of-WS2811-/) 
  
  License:
  Beerware License; if you find the code useful, and we happen to cross 
  paths, you're encouraged to buy us a beer. The code is distributed hoping
  that you in fact find it useful, but  without warranty of any kind.
  ------------------------------------------------------------------------*/
  
  static uint32_t t_f = 0;
  while((micros() - t_f) < 50L);  // wait for 50us (data latch)
  
  cli(); // Disable interrupts so that timing is as precise as possible
  volatile uint8_t
   *p     = pixelBuffer,   // Copy the start address of our data array
    val   = *p++,      // Get the current byte value & point to next byte
    high  = PORT |  _BV(PORT_PIN), // Bitmask for sending HIGH to pin
    low   = PORT & ~_BV(PORT_PIN), // Bitmask for sending LOW to pin
    tmp   = low,       // Swap variable to adjust duty cycle 
    nbits = 8;  // Bit counter for inner loop
  volatile uint16_t
    nbytes = numPixels * 3; // Byte counter for outer loop
  asm volatile(
  // The volatile attribute is used to tell the compiler not to optimize 
  // this section.  We want every instruction to be left as is.
  //
  // Generating an 800KHz signal (1.25us period) implies that we have
  // exactly 20 instructions clocked at 16MHz (0.0625us duration) to 
  // generate either a 1 or a 0---we need to do it within a single 
  // period. 
  // 
  // By choosing 1 clock cycle as our time unit we can keep track of 
  // the signal's phase (T) after each instruction is executed.
  //
  // To generate a value of 1, we need to hold the signal HIGH (maximum)
  // for 0.8us, and then LOW (minimum) for 0.45us.  Since our timing has a
  // resolution of 0.0625us we can only approximate these values. Luckily, 
  // the WS281X chips were designed to accept a +/- 300ns variance in the 
  // duration of the signal.  Thus, if we hold the signal HIGH for 13 
  // cycles (0.8125us), and LOW for 7 cycles (0.4375us), then the variance 
  // is well within the tolerated range.
  //
  // To generate a value of 0, we need to hold the signal HIGH (maximum)
  // for 0.4us, and then LOW (minimum) for 0.85us.  Thus, holding the
  // signal HIGH for 6 cycles (0.375us), and LOW for 14 cycles (0.875us)
  // will maintain the variance within the tolerated range.
  //
  // For a full description of each assembly instruction consult the AVR
  // manual here: http://www.atmel.com/images/doc0856.pdf
    // Instruction        CLK     Description                 Phase
   "nextbit:\n\t"         // -    label                       (T =  0) 
    "sbi  %0, %1\n\t"     // 2    signal HIGH                 (T =  2) 
    "sbrc %4, 7\n\t"      // 1-2  if MSB set                  (T =  ?)          
    "mov  %6, %3\n\t"     // 0-1  tmp'll set signal high     (T =  4) 
    "dec  %5\n\t"         // 1    decrease bitcount           (T =  5) 
    "nop\n\t"             // 1    nop (idle 1 clock cycle)    (T =  6)
    "st   %a2, %6\n\t"    // 2    set PORT to tmp             (T =  8)
    "mov  %6, %7\n\t"     // 1    reset tmp to low (default)  (T =  9)
    "breq nextbyte\n\t"   // 1-2  if bitcount ==0 -> nextbyte (T =  ?)                
    "rol  %4\n\t"         // 1    shift MSB leftwards         (T = 11)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "nop\n\t"             // 1    nop                         (T = 18)
    "rjmp nextbit\n\t"    // 2    bitcount !=0 -> nextbit     (T = 20)
   "nextbyte:\n\t"        // -    label                       -
    "ldi  %5, 8\n\t"      // 1    reset bitcount              (T = 11)
    "ld   %4, %a8+\n\t"   // 2    val = *p++                  (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "sbiw %9,1\n\t"       // 2    decrease bytecount          (T = 18)
    "brne nextbit\n\t"    // 2    if bytecount !=0 -> nextbit (T = 20)
    ::
    // Input operands         Operand Id (w/ constraint)
    "I" (_SFR_IO_ADDR(PORT)), // %0
    "I" (PORT_PIN),           // %1
    "e" (&PORT),              // %a2
    "r" (high),               // %3
    "r" (val),                // %4
    "r" (nbits),              // %5
    "r" (tmp),                // %6
    "r" (low),                // %7
    "e" (p),                  // %a8
    "w" (nbytes)              // %9
  );
  sei();                          // Enable interrupts
  t_f = micros();                 // t_f will be used to measure the 50us 
                                  // latching period in the next call of the 
                                  // function.
}

#endif

