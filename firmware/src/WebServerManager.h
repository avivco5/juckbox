#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <vector>

class WebServerManager {
public:
    bool begin();
    void addLog(const String& msg);

private:
    AsyncWebServer _server{80};
    std::vector<String> _logs;

    // Active upload state (one upload at a time – fine for single-user)
    File _uploadFile;
    bool _uploadHasError = false;

    void setupStatic();
    void setupApiRoutes();
    void setupUploadRoute();

    // Route handlers
    void onStatus   (AsyncWebServerRequest* req);
    void onSongs    (AsyncWebServerRequest* req);
    void onControl  (AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void onPlaylists(AsyncWebServerRequest* req);
    void onGetPlaylist(AsyncWebServerRequest* req);
    void onGetSettings(AsyncWebServerRequest* req);
    void onPostSettings(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void onLogs     (AsyncWebServerRequest* req);

    void onUploadChunk   (AsyncWebServerRequest* req, const String& filename,
                          size_t index, uint8_t* data, size_t len, bool final);
    void onUploadComplete(AsyncWebServerRequest* req);

    void sendJson (AsyncWebServerRequest* req, const String& json, int code = 200);
    void sendError(AsyncWebServerRequest* req, const String& msg,  int code = 400);
};

extern WebServerManager webServer;
