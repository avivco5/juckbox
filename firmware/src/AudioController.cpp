#include "AudioController.h"

#include <AudioFileSourceFS.h>
#include <AudioGenerator.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <SD_MMC.h>

#include "Es8311Codec.h"
#include "JukeboxAudioOutput.h"

AudioController audioController;

bool AudioController::begin()
{
    Serial.println("[Audio] Initializing ES8311 audio path...");

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex)
    {
        Serial.println("[Audio] ERROR: cannot create audio mutex");
        return false;
    }

    if (!es8311Codec.begin())
    {
        Serial.println("[Audio] ERROR: ES8311 codec not found");
        return false;
    }

    _output = new JukeboxAudioOutput(es8311Codec);
    if (!_output)
    {
        Serial.println("[Audio] ERROR: cannot allocate I2S output");
        return false;
    }
    setVolume(_volume);

    const BaseType_t taskResult = xTaskCreatePinnedToCore(
        audioTaskEntry,
        "jukebox_audio",
        8192,
        this,
        2,
        &_task,
        0);
    if (taskResult != pdPASS)
    {
        Serial.println("[Audio] ERROR: cannot create playback task");
        delete _output;
        _output = nullptr;
        return false;
    }

    _ready = true;
    Serial.println("[Audio] Ready: MP3/WAV -> I2S -> ES8311 -> speaker");
    return true;
}

bool AudioController::play(const String& path)
{
    if (!_ready || !_mutex || !_output)
    {
        Serial.println("[Audio] Play rejected: audio is not ready");
        return false;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    stopLocked(true);

    _source = new AudioFileSourceFS(SD_MMC, path.c_str());
    if (!_source || !_source->isOpen())
    {
        Serial.printf("[Audio] ERROR: cannot open %s\n", path.c_str());
        delete _source;
        _source = nullptr;
        xSemaphoreGive(_mutex);
        return false;
    }

    String lowerPath = path;
    lowerPath.toLowerCase();
    if (lowerPath.endsWith(".mp3"))
    {
        _decoder = new AudioGeneratorMP3();
    }
    else if (lowerPath.endsWith(".wav"))
    {
        _decoder = new AudioGeneratorWAV();
    }

    if (!_decoder)
    {
        Serial.printf("[Audio] ERROR: unsupported file %s\n", path.c_str());
        delete _source;
        _source = nullptr;
        xSemaphoreGive(_mutex);
        return false;
    }

    _output->SetGain(static_cast<float>(_volume) / 100.0f);
    if (!_decoder->begin(_source, _output))
    {
        Serial.printf("[Audio] ERROR: decoder failed for %s\n", path.c_str());
        delete _decoder;
        delete _source;
        _decoder = nullptr;
        _source = nullptr;
        _output->stop();
        xSemaphoreGive(_mutex);
        return false;
    }

    _elapsedBeforeResumeMs = 0;
    _resumeStartedAt = millis();
    _paused = false;
    _playing = true;
    _trackEnded = false;
    Serial.printf("[Audio] Playing: %s\n", path.c_str());
    xSemaphoreGive(_mutex);
    return true;
}

void AudioController::pause()
{
    if (!_mutex)
    {
        return;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (_decoder && _playing)
    {
        _elapsedBeforeResumeMs += millis() - _resumeStartedAt;
        _playing = false;
        _paused = true;
        es8311Codec.setAmplifier(false);
        Serial.println("[Audio] Paused");
    }
    xSemaphoreGive(_mutex);
}

void AudioController::resume()
{
    if (!_mutex)
    {
        return;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (_decoder && _paused)
    {
        _resumeStartedAt = millis();
        _paused = false;
        _playing = true;
        es8311Codec.setAmplifier(true);
        Serial.println("[Audio] Resumed");
    }
    xSemaphoreGive(_mutex);
}

void AudioController::stop()
{
    if (!_mutex)
    {
        return;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    stopLocked(true);
    xSemaphoreGive(_mutex);
    Serial.println("[Audio] Stopped");
}

void AudioController::setVolume(int volume)
{
    _volume = constrain(volume, 0, 100);
    if (_output)
    {
        _output->SetGain(static_cast<float>(_volume) / 100.0f);
    }
    Serial.printf("[Audio] Volume: %d%%\n", _volume);
}

uint32_t AudioController::elapsedSeconds() const
{
    uint32_t elapsed = _elapsedBeforeResumeMs;
    if (_playing)
    {
        elapsed += millis() - _resumeStartedAt;
    }
    return elapsed / 1000;
}

void AudioController::stopLocked(bool resetElapsed)
{
    if (_decoder)
    {
        if (_decoder->isRunning())
        {
            _decoder->stop();
        }
        delete _decoder;
        _decoder = nullptr;
    }

    if (_output)
    {
        _output->stop();
    }

    delete _source;
    _source = nullptr;
    _playing = false;
    _paused = false;
    if (resetElapsed)
    {
        _elapsedBeforeResumeMs = 0;
        _resumeStartedAt = 0;
    }
}

void AudioController::audioTaskEntry(void* context)
{
    static_cast<AudioController*>(context)->audioTask();
}

void AudioController::audioTask()
{
    while (true)
    {
        bool active = false;
        xSemaphoreTake(_mutex, portMAX_DELAY);
        if (_decoder && _playing && !_paused)
        {
            active = true;
            if (!_decoder->loop())
            {
                if (_playing)
                {
                    _elapsedBeforeResumeMs += millis() - _resumeStartedAt;
                }
                Serial.println("[Audio] End of track");
                stopLocked(false);
                _trackEnded = true;
            }
        }
        xSemaphoreGive(_mutex);

        vTaskDelay(active ? 1 : pdMS_TO_TICKS(10));
    }
}

void AudioController::update()
{
    // Playback is serviced by the dedicated audio task.
}

bool AudioController::consumeTrackEnded()
{
    if (!_mutex)
    {
        return false;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool result = _trackEnded;
    _trackEnded = false;
    xSemaphoreGive(_mutex);
    return result;
}
