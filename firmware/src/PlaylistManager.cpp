#include "PlaylistManager.h"

PlaylistManager playlistManager;

std::vector<String> PlaylistManager::listPlaylists() {
    return storageManager.listPlaylists();
}

Playlist PlaylistManager::loadPlaylist(const String& filename) {
    Playlist pl;
    pl.filename = filename;

    String raw = storageManager.loadPlaylist(filename);

    JsonDocument doc;
    if (deserializeJson(doc, raw) != DeserializationError::Ok) {
        Serial.printf("[Playlist] Parse error: %s\n", filename.c_str());
        pl.name = filename;
        return pl;
    }

    pl.name = doc["name"] | filename.c_str();
    for (JsonVariant v : doc["songs"].as<JsonArray>()) {
        pl.songs.push_back(v.as<String>());
    }
    return pl;
}

bool PlaylistManager::savePlaylist(const Playlist& pl) {
    JsonDocument doc;
    doc["name"] = pl.name;
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (const String& s : pl.songs) arr.add(s);

    String json;
    serializeJson(doc, json);
    return storageManager.savePlaylist(pl.filename, json);
}

bool PlaylistManager::createPlaylist(const String& name, const std::vector<String>& songs) {
    Playlist pl;
    pl.name     = name;
    pl.filename = StorageManager::sanitizeFilename(name) + ".json";
    pl.songs    = songs;
    return savePlaylist(pl);
}

String PlaylistManager::allPlaylistsJson() {
    auto files = listPlaylists();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const String& f : files) {
        Playlist pl = loadPlaylist(f);
        JsonObject obj = arr.add<JsonObject>();
        obj["filename"]  = f;
        obj["name"]      = pl.name;
        obj["songCount"] = (int)pl.songs.size();
    }
    String out;
    serializeJson(doc, out);
    return out;
}

String PlaylistManager::playlistJson(const String& filename) {
    Playlist pl = loadPlaylist(filename);
    JsonDocument doc;
    doc["name"]     = pl.name;
    doc["filename"] = pl.filename;
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (const String& s : pl.songs) arr.add(s);
    String out;
    serializeJson(doc, out);
    return out;
}
