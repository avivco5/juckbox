#include "JukeboxAudioOutput.h"

#include "Es8311Codec.h"

namespace {
constexpr i2s_port_t AudioPort = I2S_NUM_0;
constexpr int MclkPin = 17;
constexpr int BclkPin = 18;
constexpr int WordSelectPin = 21;
constexpr int DataOutPin = 15;
}

JukeboxAudioOutput::JukeboxAudioOutput(Es8311Codec& codec)
    : _codec(codec)
{
    hertz = 44100;
    channels = 2;
    SetGain(0.68f);
}

JukeboxAudioOutput::~JukeboxAudioOutput()
{
    stop();
}

bool JukeboxAudioOutput::SetRate(int hz)
{
    if (hz <= 0)
    {
        return false;
    }
    if (hertz == hz)
    {
        return true;
    }

    if (_active)
    {
        if (i2s_set_clk(
                AudioPort,
                hz,
                I2S_BITS_PER_SAMPLE_16BIT,
                I2S_CHANNEL_STEREO) != ESP_OK)
        {
            Serial.printf("[I2S] Cannot set %d Hz\n", hz);
            return false;
        }
        if (!_codec.setSampleRate(hz))
        {
            return false;
        }
    }

    hertz = hz;
    return true;
}

bool JukeboxAudioOutput::SetChannels(int channelCount)
{
    if (channelCount < 1 || channelCount > 2)
    {
        return false;
    }
    channels = channelCount;
    return true;
}

bool JukeboxAudioOutput::begin()
{
    if (_active)
    {
        return true;
    }

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = hertz;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 10;
    config.dma_buf_len = 256;
    config.use_apll = true;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

    if (i2s_driver_install(AudioPort, &config, 0, nullptr) != ESP_OK)
    {
        Serial.println("[I2S] Driver install failed");
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = MclkPin;
    pins.bck_io_num = BclkPin;
    pins.ws_io_num = WordSelectPin;
    pins.data_out_num = DataOutPin;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(AudioPort, &pins) != ESP_OK)
    {
        Serial.println("[I2S] Pin setup failed");
        i2s_driver_uninstall(AudioPort);
        return false;
    }

    i2s_zero_dma_buffer(AudioPort);
    _active = true;
    if (!_codec.setSampleRate(hertz) ||
        !_codec.setMuted(false))
    {
        stop();
        return false;
    }

    delay(8);
    _codec.setAmplifier(true);
    Serial.printf(
        "[I2S] Started: %u Hz, MCLK=%u Hz\n",
        static_cast<unsigned>(hertz),
        static_cast<unsigned>(hertz * 256U));
    return true;
}

bool JukeboxAudioOutput::ConsumeSample(int16_t sample[2])
{
    if (!_active)
    {
        return false;
    }

    int16_t stereo[2] = {sample[0], sample[1]};
    MakeSampleStereo16(stereo);
    const int16_t left = Amplify(stereo[LEFTCHANNEL]);
    const int16_t right = Amplify(stereo[RIGHTCHANNEL]);
    const uint32_t frame =
        static_cast<uint16_t>(left) |
        (static_cast<uint32_t>(
             static_cast<uint16_t>(right)) << 16);

    size_t bytesWritten = 0;
    i2s_write(
        AudioPort,
        &frame,
        sizeof(frame),
        &bytesWritten,
        0);
    return bytesWritten == sizeof(frame);
}

void JukeboxAudioOutput::flush()
{
    if (!_active)
    {
        return;
    }

    const uint32_t silence = 0;
    size_t bytesWritten = 0;
    for (int i = 0; i < 512; ++i)
    {
        i2s_write(
            AudioPort,
            &silence,
            sizeof(silence),
            &bytesWritten,
            portMAX_DELAY);
    }
}

bool JukeboxAudioOutput::stop()
{
    if (!_active)
    {
        _codec.setAmplifier(false);
        return true;
    }

    _codec.setAmplifier(false);
    _codec.setMuted(true);
    i2s_zero_dma_buffer(AudioPort);
    i2s_driver_uninstall(AudioPort);
    _active = false;
    Serial.println("[I2S] Stopped");
    return true;
}
