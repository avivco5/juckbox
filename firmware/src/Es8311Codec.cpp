#include "Es8311Codec.h"

#include <Wire.h>

namespace {
constexpr uint8_t CodecAddress = 0x18;
constexpr int AmplifierEnablePin = 1;

constexpr uint8_t ResetReg = 0x00;
constexpr uint8_t Clock1Reg = 0x01;
constexpr uint8_t Clock2Reg = 0x02;
constexpr uint8_t Clock3Reg = 0x03;
constexpr uint8_t Clock4Reg = 0x04;
constexpr uint8_t Clock5Reg = 0x05;
constexpr uint8_t Clock6Reg = 0x06;
constexpr uint8_t Clock7Reg = 0x07;
constexpr uint8_t Clock8Reg = 0x08;
constexpr uint8_t DacInterfaceReg = 0x09;
constexpr uint8_t AdcInterfaceReg = 0x0A;
constexpr uint8_t System0BReg = 0x0B;
constexpr uint8_t System0CReg = 0x0C;
constexpr uint8_t System0DReg = 0x0D;
constexpr uint8_t System0EReg = 0x0E;
constexpr uint8_t System10Reg = 0x10;
constexpr uint8_t System11Reg = 0x11;
constexpr uint8_t System12Reg = 0x12;
constexpr uint8_t System13Reg = 0x13;
constexpr uint8_t System14Reg = 0x14;
constexpr uint8_t Adc15Reg = 0x15;
constexpr uint8_t Adc16Reg = 0x16;
constexpr uint8_t Adc17Reg = 0x17;
constexpr uint8_t Adc1BReg = 0x1B;
constexpr uint8_t Adc1CReg = 0x1C;
constexpr uint8_t DacMuteReg = 0x31;
constexpr uint8_t DacVolumeReg = 0x32;
constexpr uint8_t DacRampReg = 0x37;
constexpr uint8_t GpioReg = 0x44;
constexpr uint8_t GeneralPurposeReg = 0x45;
}

Es8311Codec es8311Codec;

bool Es8311Codec::writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(CodecAddress);
    Wire.write(reg);
    Wire.write(value);
    const uint8_t result = Wire.endTransmission();
    if (result != 0)
    {
        Serial.printf(
            "[ES8311] I2C write failed reg=0x%02X error=%u\n",
            reg,
            result);
        return false;
    }
    return true;
}

bool Es8311Codec::readRegister(uint8_t reg, uint8_t& value)
{
    Wire.beginTransmission(CodecAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    if (Wire.requestFrom(
            static_cast<int>(CodecAddress),
            1) != 1)
    {
        return false;
    }

    value = Wire.read();
    return true;
}

bool Es8311Codec::begin()
{
    pinMode(AmplifierEnablePin, OUTPUT);
    setAmplifier(false);

    Wire.beginTransmission(CodecAddress);
    if (Wire.endTransmission() != 0)
    {
        Serial.println("[ES8311] No response at I2C address 0x18");
        return false;
    }

    uint8_t chipId1 = 0;
    uint8_t chipId2 = 0;
    uint8_t chipVersion = 0;
    readRegister(0xFD, chipId1);
    readRegister(0xFE, chipId2);
    readRegister(0xFF, chipVersion);
    Serial.printf(
        "[ES8311] Found: ID=%02X%02X version=%02X\n",
        chipId1,
        chipId2,
        chipVersion);

    bool ok = true;
    ok &= writeRegister(GpioReg, 0x08);
    ok &= writeRegister(GpioReg, 0x08);
    ok &= writeRegister(Clock1Reg, 0x30);
    ok &= writeRegister(Clock2Reg, 0x00);
    ok &= writeRegister(Clock3Reg, 0x10);
    ok &= writeRegister(Adc16Reg, 0x24);
    ok &= writeRegister(Clock4Reg, 0x10);
    ok &= writeRegister(Clock5Reg, 0x00);
    ok &= writeRegister(System0BReg, 0x00);
    ok &= writeRegister(System0CReg, 0x00);
    ok &= writeRegister(System10Reg, 0x1F);
    ok &= writeRegister(System11Reg, 0x7F);
    ok &= writeRegister(ResetReg, 0x80);

    // ESP32 is the I2S master; ES8311 is the slave and uses the MCLK pin.
    ok &= writeRegister(ResetReg, 0x80);
    ok &= writeRegister(Clock1Reg, 0x3F);

    uint8_t clock6 = 0;
    if (readRegister(Clock6Reg, clock6))
    {
        ok &= writeRegister(Clock6Reg, clock6 & ~0x20);
    }
    else
    {
        ok = false;
    }

    ok &= writeRegister(System13Reg, 0x10);
    ok &= writeRegister(Adc1BReg, 0x0A);
    ok &= writeRegister(Adc1CReg, 0x6A);
    ok &= writeRegister(GpioReg, 0x58);

    // I2S, 16-bit samples. Keep ADC muted; enable the DAC path.
    ok &= writeRegister(DacInterfaceReg, 0x0C);
    ok &= writeRegister(AdcInterfaceReg, 0x4C);
    ok &= writeRegister(Adc17Reg, 0xBF);
    ok &= writeRegister(System0EReg, 0x02);
    ok &= writeRegister(System12Reg, 0x00);
    ok &= writeRegister(System14Reg, 0x1A);
    ok &= writeRegister(System0DReg, 0x01);
    ok &= writeRegister(Adc15Reg, 0x40);
    ok &= writeRegister(DacRampReg, 0x08);
    ok &= writeRegister(GeneralPurposeReg, 0x00);
    ok &= writeRegister(GpioReg, 0x58);

    // 0xBF is 0 dB. User volume is applied in the PCM output.
    ok &= writeRegister(DacVolumeReg, 0xBF);
    _ready = ok;
    if (!_ready || !setSampleRate(44100) || !setMuted(true))
    {
        _ready = false;
        Serial.println("[ES8311] Initialization failed");
        return false;
    }

    Serial.println("[ES8311] Codec initialized");
    return true;
}

bool Es8311Codec::setSampleRate(uint32_t sampleRate)
{
    if (!_ready && _sampleRate != 0)
    {
        return false;
    }

    switch (sampleRate)
    {
        case 8000:
        case 11025:
        case 12000:
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
            break;
        default:
            Serial.printf(
                "[ES8311] Unsupported sample rate: %u Hz\n",
                static_cast<unsigned>(sampleRate));
            return false;
    }

    if (_sampleRate == sampleRate)
    {
        return true;
    }

    // MCLK is generated at 256 * sample rate. These divider values are
    // taken from Espressif's ES8311 driver for that clock relationship.
    const uint8_t oversampling =
        sampleRate <= 16000 ? 0x20 : 0x10;
    bool ok = true;
    ok &= writeRegister(Clock2Reg, 0x00);
    ok &= writeRegister(Clock5Reg, 0x00);
    ok &= writeRegister(Clock3Reg, 0x10);
    ok &= writeRegister(Clock4Reg, oversampling);
    ok &= writeRegister(Clock7Reg, 0x00);
    ok &= writeRegister(Clock8Reg, 0xFF);
    ok &= writeRegister(Clock6Reg, 0x03);

    if (ok)
    {
        _sampleRate = sampleRate;
        Serial.printf(
            "[ES8311] Sample rate: %u Hz\n",
            static_cast<unsigned>(sampleRate));
    }
    return ok;
}

bool Es8311Codec::setMuted(bool muted)
{
    uint8_t value = 0;
    if (!readRegister(DacMuteReg, value))
    {
        return false;
    }
    value &= 0x9F;
    if (muted)
    {
        value |= 0x60;
    }
    return writeRegister(DacMuteReg, value);
}

void Es8311Codec::setAmplifier(bool enabled)
{
    // The board's PA enable signal is inverted.
    digitalWrite(AmplifierEnablePin, enabled ? LOW : HIGH);
}
