#pragma once

#include <Arduino.h>

struct TouchPoint {
    uint16_t x = 0;
    uint16_t y = 0;
};

class TouchController {
public:
    bool begin();
    bool read(TouchPoint& point);
    bool isTouchActive() const;
    void rearm();

private:
    void resetHardware();
    bool readRegister(uint16_t reg, uint8_t* data, size_t length);

    uint8_t _maxPoints = 1;
};

extern TouchController touchController;
