#include "TouchController.h"

#include <Wire.h>

#include "DisplayDriver.h"

namespace {
constexpr int TouchSda = 38;
constexpr int TouchScl = 39;
constexpr int TouchInterrupt = 47;
constexpr int TouchReset = 48;
constexpr uint8_t TouchAddress = 0x55;

constexpr uint16_t MaxTouchesRegister = 0x0009;
constexpr uint16_t TouchInfoRegister = 0x0010;
constexpr uint16_t FirstPointRegister = 0x0014;
}

TouchController touchController;

bool TouchController::begin()
{
    Wire.begin(TouchSda, TouchScl);
    Wire.setClock(400000);

    pinMode(TouchInterrupt, INPUT_PULLUP);
    pinMode(TouchReset, OUTPUT);
    resetHardware();

    uint8_t maxPoints = 0;
    if (readRegister(MaxTouchesRegister, &maxPoints, 1) &&
        maxPoints > 0 &&
        maxPoints <= 10)
    {
        _maxPoints = maxPoints;
    }

    Serial.printf("[Touch] max points: %u\n", _maxPoints);
    return true;
}

void TouchController::resetHardware()
{
    digitalWrite(TouchReset, LOW);
    delay(12);
    digitalWrite(TouchReset, HIGH);
    delay(110);
}

void TouchController::rearm()
{
    Serial.println("[Touch] rearming controller");
    resetHardware();
}

bool TouchController::readRegister(
    uint16_t reg,
    uint8_t* data,
    size_t length)
{
    // Edge-triggered logging: report when I2C starts/stops failing,
    // not on every poll - read() is called every loop iteration and
    // "no touch right now" is the normal case, not an error.
    static bool lastOk = true;
    static uint32_t failureCount = 0;

    Wire.beginTransmission(TouchAddress);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    bool ok = Wire.endTransmission(false) == 0;

    size_t received = 0;
    if (ok)
    {
        received = Wire.requestFrom(
            static_cast<int>(TouchAddress),
            static_cast<int>(length));
        ok = received == length;
    }

    if (!ok)
    {
        ++failureCount;
        if (lastOk)
        {
            Serial.println(
                "[Touch] I2C read failing - touch may be unresponsive");
        }
        lastOk = false;
        return false;
    }

    if (!lastOk)
    {
        Serial.printf(
            "[Touch] I2C read recovered after %u failed attempt(s)\n",
            static_cast<unsigned>(failureCount));
        failureCount = 0;
    }
    lastOk = true;

    for (size_t i = 0; i < length; ++i)
    {
        data[i] = Wire.read();
    }
    return true;
}

bool TouchController::read(TouchPoint& point)
{
    uint8_t touchInfo = 0;
    if (!readRegister(TouchInfoRegister, &touchInfo, 1) ||
        !(touchInfo & 0x08))
    {
        return false;
    }

    uint8_t data[7] = {};
    if (!readRegister(FirstPointRegister, data, sizeof(data)) ||
        !(data[0] & 0x80))
    {
        return false;
    }

    const uint16_t rawX =
        ((data[0] & 0x3F) << 8) | data[1];
    const uint16_t rawY =
        ((data[2] & 0x3F) << 8) | data[3];

    point.x = std::min<uint16_t>(
        rawY,
        DisplayDriver::Width - 1);
    point.y = rawX >= DisplayDriver::Height
        ? 0
        : DisplayDriver::Height - 1 - rawX;

    return true;
}

bool TouchController::isTouchActive() const
{
    return digitalRead(TouchInterrupt) == LOW;
}
