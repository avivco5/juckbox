#pragma once

#include <Arduino.h>
#include <vector>

struct LibrarySong {
    String name;
    String path;
    size_t size = 0;
    uint32_t durationSeconds = 0;
};

class SongLibrary {
public:
    bool begin();
    bool scan();

    bool isMounted() const { return _mounted; }
    const String& status() const { return _status; }
    const std::vector<LibrarySong>& songs() const { return _songs; }

private:
    static bool isAudioFile(const String& filename);
    static String displayName(const String& filename);

    bool _mounted = false;
    String _status = "SD NOT READY";
    std::vector<LibrarySong> _songs;
};

extern SongLibrary songLibrary;
