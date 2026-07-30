#include "WebServerManager.h"
#include "StorageManager.h"
#include "AudioController.h"
#include "PlaylistManager.h"
#include "config.h"

WebServerManager webServer;

// ---------------------------------------------------------------------------
//  Public
// ---------------------------------------------------------------------------

bool WebServerManager::begin() {
    Serial.println("[Web] Starting web server...");

    // LittleFS holds index.html / style.css / app.js
    if (!LittleFS.begin(true)) {
        Serial.println("[Web] LittleFS mount failed – upload 'data/' folder with: pio run -t uploadfs");
    } else {
        Serial.println("[Web] LittleFS OK");
    }

    // Allow cross-origin requests (handy during dev with curl / Postman)
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin",  "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    setupStatic();
    setupApiRoutes();
    setupUploadRoute();

    _server.begin();
    Serial.println("[Web] HTTP server ready on port 80");
    addLog("Server started");
    return true;
}

void WebServerManager::addLog(const String& msg) {
    char ts[16];
    snprintf(ts, sizeof(ts), "%lus", millis() / 1000UL);
    String entry = String(ts) + ": " + msg;
    _logs.push_back(entry);
    if (_logs.size() > MAX_LOG_ENTRIES) _logs.erase(_logs.begin());
    Serial.println("[Log] " + entry);
}

// ---------------------------------------------------------------------------
//  Setup helpers
// ---------------------------------------------------------------------------

void WebServerManager::setupStatic() {
    // Root → index.html
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/index.html")) {
            req->send(LittleFS, "/index.html", "text/html");
        } else {
            req->send(200, "text/html",
                "<html><body><h2>ESP32 Jukebox</h2>"
                "<p>Web UI files not found.<br>"
                "Run: <code>pio run -t uploadfs</code></p></body></html>");
        }
    });

    // All other static assets from LittleFS
    _server.serveStatic("/", LittleFS, "/");

    // OPTIONS preflight
    _server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* req) {
        req->send(200);
    });

    // 404
    _server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_OPTIONS) { req->send(200); return; }
        req->send(404, "text/plain", "Not found");
    });
}

void WebServerManager::setupApiRoutes() {
    // GET /api/status
    _server.on("/api/status", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onStatus(req); });

    // GET /api/songs
    _server.on("/api/songs", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onSongs(req); });

    // POST /api/control  (JSON body)
    _server.on("/api/control", HTTP_POST,
        [](AsyncWebServerRequest*) {},   // completion (empty – handled in body cb)
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index + len == total) onControl(req, data, len);
        }
    );

    // GET /api/playlists
    _server.on("/api/playlists", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onPlaylists(req); });

    // GET /api/playlists?name=xxx
    _server.on("/api/playlist", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onGetPlaylist(req); });

    // GET /api/settings
    _server.on("/api/settings", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onGetSettings(req); });

    // POST /api/settings  (JSON body)
    _server.on("/api/settings", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index + len == total) onPostSettings(req, data, len);
        }
    );

    // GET /api/logs
    _server.on("/api/logs", HTTP_GET,
        [this](AsyncWebServerRequest* req) { onLogs(req); });
}

void WebServerManager::setupUploadRoute() {
    _server.on("/api/upload", HTTP_POST,
        [this](AsyncWebServerRequest* req) { onUploadComplete(req); },
        [this](AsyncWebServerRequest* req, const String& filename,
               size_t index, uint8_t* data, size_t len, bool final) {
            onUploadChunk(req, filename, index, data, len, final);
        }
    );
}

// ---------------------------------------------------------------------------
//  Route handlers
// ---------------------------------------------------------------------------

void WebServerManager::onStatus(AsyncWebServerRequest* req) {
    size_t songCount = storageManager.isMounted()
                       ? storageManager.listSongs().size() : 0;

    JsonDocument doc;
    doc["connected"]   = true;
    doc["sdMounted"]   = storageManager.isMounted();
    doc["currentSong"] = audioController.getCurrentSong();
    doc["isPlaying"]   = audioController.isPlaying();
    doc["volume"]      = audioController.getVolume();
    doc["songCount"]   = (int)songCount;
    doc["freeHeap"]    = (int)ESP.getFreeHeap();
    doc["uptime"]      = (long)(millis() / 1000UL);

    String json;
    serializeJson(doc, json);
    sendJson(req, json);
}

void WebServerManager::onSongs(AsyncWebServerRequest* req) {
    if (!storageManager.isMounted()) {
        sendError(req, "SD card not mounted", 503);
        return;
    }

    auto songs = storageManager.listSongs();

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const SongInfo& s : songs) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]   = s.id;
        obj["name"] = s.name;
        obj["path"] = s.path;
        obj["size"] = (long)s.size;
    }

    String json;
    serializeJson(doc, json);
    sendJson(req, json);
}

void WebServerManager::onControl(AsyncWebServerRequest* req,
                                  uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        sendError(req, "Invalid JSON");
        return;
    }

    String action = doc["action"] | "";

    if (action == "play") {
        String song = doc["song"] | "";
        if (song.length() > 0) {
            audioController.play(song);
            addLog("Play: " + song);
        } else {
            audioController.resume();
            addLog("Resume");
        }
    } else if (action == "pause") {
        audioController.pause();
        addLog("Pause");
    } else if (action == "stop") {
        audioController.stop();
        addLog("Stop");
    } else if (action == "next") {
        audioController.next();
        addLog("Next");
    } else if (action == "previous") {
        audioController.previous();
        addLog("Previous");
    } else if (action == "setVolume") {
        int vol = doc["volume"] | audioController.getVolume();
        audioController.setVolume(vol);
        addLog("Volume: " + String(vol));
    } else {
        sendError(req, "Unknown action: " + action);
        return;
    }

    sendJson(req, "{\"success\":true}");
}

void WebServerManager::onPlaylists(AsyncWebServerRequest* req) {
    sendJson(req, playlistManager.allPlaylistsJson());
}

void WebServerManager::onGetPlaylist(AsyncWebServerRequest* req) {
    if (!req->hasParam("name")) {
        sendError(req, "Missing 'name' parameter");
        return;
    }
    String name = req->getParam("name")->value();
    sendJson(req, playlistManager.playlistJson(name));
}

void WebServerManager::onGetSettings(AsyncWebServerRequest* req) {
    sendJson(req, storageManager.loadSettings());
}

void WebServerManager::onPostSettings(AsyncWebServerRequest* req,
                                       uint8_t* data, size_t len) {
    String body((char*)data, len);
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        sendError(req, "Invalid JSON");
        return;
    }
    if (storageManager.saveSettings(body)) {
        sendJson(req, "{\"success\":true}");
        addLog("Settings saved");
    } else {
        sendError(req, "Failed to write settings", 500);
    }
}

void WebServerManager::onLogs(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();
    for (const String& l : _logs) arr.add(l);
    String json;
    serializeJson(doc, json);
    sendJson(req, json);
}

// ---------------------------------------------------------------------------
//  File upload  (multipart – streams directly to SD)
// ---------------------------------------------------------------------------

void WebServerManager::onUploadChunk(AsyncWebServerRequest* req,
                                      const String& filename,
                                      size_t index, uint8_t* data,
                                      size_t len, bool final) {
    if (index == 0) {
        // First chunk: validate extension and open SD file
        String safe = StorageManager::sanitizeFilename(filename);

        if (!StorageManager::isAllowedExtension(safe)) {
            Serial.printf("[Upload] Rejected: %s (bad extension)\n", filename.c_str());
            _uploadHasError = true;
            return;
        }
        if (!storageManager.isMounted()) {
            Serial.println("[Upload] SD not mounted");
            _uploadHasError = true;
            return;
        }

        Serial.printf("[Upload] Start '%s' -> '%s'\n", filename.c_str(), safe.c_str());
        _uploadHasError = false;
        _uploadFile     = storageManager.openSongForWrite(safe);

        if (!_uploadFile) {
            Serial.println("[Upload] Cannot open file for write");
            _uploadHasError = true;
        }
    }

    if (!_uploadHasError && _uploadFile) {
        size_t written = _uploadFile.write(data, len);
        if (written != len) {
            Serial.println("[Upload] Write error – SD full?");
            _uploadHasError = true;
        }
    }

    if (final) {
        if (_uploadFile) {
            _uploadFile.close();
            Serial.printf("[Upload] Done – %u bytes\n", (unsigned)(index + len));
        }
    }
}

void WebServerManager::onUploadComplete(AsyncWebServerRequest* req) {
    if (_uploadHasError) {
        _uploadHasError = false;
        req->send(400, "application/json",
                  "{\"success\":false,\"error\":\"Upload failed – invalid type or SD error\"}");
    } else {
        addLog("Upload OK");
        req->send(200, "application/json",
                  "{\"success\":true,\"message\":\"File uploaded successfully\"}");
    }
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

void WebServerManager::sendJson(AsyncWebServerRequest* req,
                                 const String& json, int code) {
    req->send(code, "application/json", json);
}

void WebServerManager::sendError(AsyncWebServerRequest* req,
                                  const String& msg, int code) {
    String body = "{\"success\":false,\"error\":\"" + msg + "\"}";
    req->send(code, "application/json", body);
}
