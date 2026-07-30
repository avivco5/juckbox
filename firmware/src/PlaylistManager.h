#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "StorageManager.h"

struct Playlist {
    String              name;
    String              filename;
    std::vector<String> songs;
};

class PlaylistManager {
public:
    std::vector<String> listPlaylists();
    Playlist            loadPlaylist(const String& filename);
    bool                savePlaylist(const Playlist& pl);
    bool                createPlaylist(const String& name, const std::vector<String>& songs);

    String allPlaylistsJson();
    String playlistJson(const String& filename);
};

extern PlaylistManager playlistManager;
