#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class AudioFileSourceFS;
class AudioGenerator;
class JukeboxAudioOutput;

class AudioController {
public:
    bool begin();
    bool play(const String& path);
    void pause();
    void resume();
    void stop();
    void setVolume(int volume);
    void update();

    bool isReady() const { return _ready; }
    bool isPlaying() const { return _playing; }
    bool isPaused() const { return _paused; }
    bool hasTrack() const { return _decoder != nullptr; }
    int volume() const { return _volume; }
    uint32_t elapsedSeconds() const;

    // Returns true once if the track finished on its own since the last
    // call (i.e. not because of an explicit stop()), then clears the flag.
    bool consumeTrackEnded();

private:
    static void audioTaskEntry(void* context);
    void audioTask();
    void stopLocked(bool resetElapsed);

    SemaphoreHandle_t _mutex = nullptr;
    TaskHandle_t _task = nullptr;
    AudioFileSourceFS* _source = nullptr;
    AudioGenerator* _decoder = nullptr;
    JukeboxAudioOutput* _output = nullptr;

    volatile bool _ready = false;
    volatile bool _playing = false;
    volatile bool _paused = false;
    volatile bool _trackEnded = false;
    int _volume = 68;
    uint32_t _elapsedBeforeResumeMs = 0;
    uint32_t _resumeStartedAt = 0;
};

extern AudioController audioController;
