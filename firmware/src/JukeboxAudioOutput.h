#pragma once

#include <AudioOutput.h>
#include <driver/i2s.h>

class Es8311Codec;

class JukeboxAudioOutput : public AudioOutput {
public:
    explicit JukeboxAudioOutput(Es8311Codec& codec);
    ~JukeboxAudioOutput() override;

    bool SetRate(int hz) override;
    bool SetChannels(int channels) override;
    bool begin() override;
    bool ConsumeSample(int16_t sample[2]) override;
    bool stop() override;
    void flush() override;

private:
    Es8311Codec& _codec;
    bool _active = false;
};
