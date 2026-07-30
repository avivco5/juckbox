#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

class DisplayDriver {
public:
    static constexpr int Width = 480;
    static constexpr int Height = 320;

    bool begin();
    void flush();
    void setBacklight(bool enabled);

    Arduino_Canvas* canvas() { return _canvas; }

private:
    void initializePanel();
    void sendCommand(uint8_t command, const uint8_t* data, uint8_t length);

    Arduino_ESP32QSPI* _bus = nullptr;
    Arduino_Canvas* _canvas = nullptr;
    uint16_t* _transferBuffer = nullptr;
};

extern DisplayDriver displayDriver;
