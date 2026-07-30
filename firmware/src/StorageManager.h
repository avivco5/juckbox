#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <vector>

struct SongInfo {
    int    id;
    String name;
    String path;
    size_t size;
};

class StorageManager {
public:
    bool begin();
    bool isMounted() const { return _mounted; }

    bool                  createFolderStructure();
    std::vector<SongInfo> listSongs();

    // Upload helpers – caller opens, writes chunks, then closes
    File openSongForWrite(const String& filename);
    bool songExists(const String& filename);

    // Settings
    String loadSettings();
    bool   saveSettings(const String& json);

    // Playlists
    std::vector<String> listPlaylists();
    String              loadPlaylist(const String& filename);
    bool                savePlaylist(const String& filename, const String& json);

    static String sanitizeFilename(const String& raw);
    static bool   isAllowedExtension(const String& filename);

private:
    bool      _mounted = false;
    SPIClass* _spi     = nullptr;
};

extern StorageManager storageManager;
