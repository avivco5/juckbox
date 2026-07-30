#pragma once

#include <Arduino.h>

class Es8311Codec {
public:
    bool begin();
    bool setSampleRate(uint32_t sampleRate);
    bool setMuted(bool muted);
    void setAmplifier(bool enabled);

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);

    bool _ready = false;
    uint32_t _sampleRate = 0;
};

extern Es8311Codec es8311Codec;
