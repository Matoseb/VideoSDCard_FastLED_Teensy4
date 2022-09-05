#include <OctoWS2811.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <Wire.h>
#include <FastLED.h>
#include <Arduino.h>

#define LED_WIDTH 28  // number of LEDs horizontally
#define LED_HEIGHT 40 // number of LEDs vertically (must be multiple of 8) // 43
#define VIDEO_WIDTH 28
#define VIDEO_HEIGHT 40
#define LED_LAYOUT 0

#define FILENAME "VIDEO.BIN"

elapsedMicros elapsedSinceLastFrame = 0;

bool playing = false;

#pragma region LEDS

const int ledsPerStrip = 142;
const int ledsScreen = LED_WIDTH * LED_HEIGHT / 8;
DMAMEM int displayMemory[ledsScreen * 6];
int drawingMemory[ledsScreen * 6];

const int numPins = 1;
byte pinList[numPins] = {14};
CRGB rgbarray[numPins * ledsPerStrip];

// OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, WS2811_800kHz);
OctoWS2811 octo(ledsScreen, displayMemory, drawingMemory, WS2811_800kHz, numPins, pinList);
// OctoWS2811 octo(ledsScreen, displayMemory, drawingMemory, WS2811_RGB | WS2811_800kHz, numPins, pinList);
File videofile;

template <EOrder RGB_ORDER = GRB, uint8_t CHIP = WS2811_800kHz>
class CTeensy4Controller : public CPixelLEDController<RGB_ORDER, 8, 0xFF>
{
  OctoWS2811 *pocto;

public:
  CTeensy4Controller(OctoWS2811 *_pocto)
      : pocto(_pocto){};

  virtual void init() {}
  virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> &pixels)
  {

    
    uint32_t i = 0;
    while (pixels.has(1))
    {
      uint8_t r = pixels.loadAndScale0();
      uint8_t g = pixels.loadAndScale1();
      uint8_t b = pixels.loadAndScale2();
      
      pocto->setPixel(i++, r, g, b);
      pixels.stepDithering();
      pixels.advanceData();
    }

    pocto->show();
  }
};

CTeensy4Controller<GRB, WS2811_800kHz> *pcontroller;

#pragma endregion LEDS

void setup()
{

  // while (!Serial) ;
  delay(50);

  Serial.println("VideoSDcard");

  octo.begin();
  octo.show();

  pcontroller = new CTeensy4Controller<GRB, WS2811_800kHz>(&octo);

  FastLED.setBrightness(50);
  FastLED.addLeds(pcontroller, rgbarray, numPins * ledsPerStrip);

  if (!SD.begin(BUILTIN_SDCARD))
    stopWithErrorMessage("Could not access SD card");
  Serial.println("SD card ok");
  videofile = SD.open(FILENAME, FILE_READ);
  if (!videofile)
    stopWithErrorMessage("Could not read " FILENAME);
  Serial.println("File opened");
  playing = true;
  elapsedSinceLastFrame = 0;
}

// read from the SD card, true=ok, false=unable to read
// the SD library is much faster if all reads are 512 bytes
// this function lets us easily read any size, but always
// requests data from the SD library in 512 byte blocks.
//

void loop()
{
  unsigned char header[5];

  if (playing)
  {
    if (sd_card_read(header, 5))
    {
      if (header[0] == '*')
      {
        // found an image frame
        unsigned int size = (header[1] | (header[2] << 8)) * 3;
        unsigned int usec = header[3] | (header[4] << 8);
        unsigned int readsize = size;

        // Serial.printf("v: %u %u", size, usec);

        if (readsize > sizeof(drawingMemory))
        {
          readsize = sizeof(drawingMemory);
        }
        if (sd_card_read(drawingMemory, readsize))
        {
          // Serial.printf(", us = %u", (unsigned int)elapsedSinceLastFrame);

          for (int i = 0; i < (numPins * ledsPerStrip); i++)
          {
            rgbarray[i] = octo.getPixel(i);
          }

          while (elapsedSinceLastFrame < usec)
            ; // wait
          elapsedSinceLastFrame -= usec;
          // leds.show();
          FastLED.show();
        }
        else
        {
          error("unable to read video frame data");
          return;
        }
        if (readsize < size)
        {
          sd_card_skip(size - readsize);
        }
      }
      else
      {
        error("unknown header");
        return;
      }
    }
    else
    {
      error("unable to read 5-byte header");
      return;
    }
  }
  else
  {
    delay(2000);
    videofile = SD.open(FILENAME, FILE_READ);
    if (videofile)
    {
      Serial.println("File opened");
      playing = true;
      elapsedSinceLastFrame = 0;
    }
  }
}
