#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class DisplayService {
public:
    DisplayService();
    void begin();
    void loop();
    void redraw();

private:
    Adafruit_ST7789 _tft;
    uint32_t _lastRefresh = 0;
};
