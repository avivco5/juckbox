#include "StatusLed.h"

#include <Adafruit_NeoPixel.h>

#include "AudioController.h"

namespace {
// Confirmed by physically probing the board's onboard RGB LED with a
// color-coded test sketch (see RgbPinFinder.cpp) — not documented by
// the vendor's datasheet.
constexpr int RgbLedPin = 40;
constexpr uint8_t Brightness = 80;
constexpr unsigned int HueStepPerTick = 256;
}

StatusLed statusLed;

bool StatusLed::begin()
{
    _pixel = new Adafruit_NeoPixel(1, RgbLedPin, NEO_GRB + NEO_KHZ800);
    _pixel->begin();
    _pixel->setBrightness(Brightness);
    _pixel->clear();
    _pixel->show();
    return true;
}

void StatusLed::update()
{
    if (!_pixel)
    {
        return;
    }

    if (_mode == LedMode::Always || audioController.isPlaying())
    {
        _hue += HueStepPerTick;
        _pixel->setPixelColor(
            0,
            //_pixel->gamma32(_pixel->ColorHSV(_hue)));
            _pixel->gamma32(_pixel->Color(255, 0, 0)));
    }
    else
    {
        _pixel->setPixelColor(0, 0);
    }
    _pixel->show();
}
