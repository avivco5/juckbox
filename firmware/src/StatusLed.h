#pragma once

class Adafruit_NeoPixel;

enum class LedMode {
    WhenPlaying,
    Always,
};

class StatusLed {
public:
    bool begin();
    void update();

    void setMode(LedMode mode) { _mode = mode; }
    LedMode mode() const { return _mode; }

private:
    Adafruit_NeoPixel* _pixel = nullptr;
    unsigned int _hue = 0;
    LedMode _mode = LedMode::WhenPlaying;
};

extern StatusLed statusLed;
