#pragma once

// ============================================================
//  WiFi Access Point
// ============================================================
#define AP_SSID      "Jukebox-ESP32"
#define AP_PASSWORD  "12345678"
#define AP_IP_1      192
#define AP_IP_2      168
#define AP_IP_3      4
#define AP_IP_4      1

// ============================================================
//  SD Card – SPI mode
//  Adjust to match your board's SD card wiring.
//  Common pins on Sunton / SC-series ESP32-S3 3.5" boards:
//    MOSI=11, MISO=13, SCLK=12, CS=10
// ============================================================
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCLK_PIN  12
#define SD_SPI_FREQ  25000000   // 25 MHz – lower to 4000000 if SD errors

// ============================================================
//  Audio – I2S (ESP32-S3 has no analog DAC like the original ESP32)
//  Adjust to match your board's I2S amplifier wiring.
//  TODO: Set correct pins once real audio is wired up.
// ============================================================
#define I2S_BCLK_PIN   17
#define I2S_WS_PIN     18
#define I2S_DOUT_PIN    8

// ============================================================
//  Storage Paths
// ============================================================
#define SONGS_FOLDER      "/songs"
#define PLAYLISTS_FOLDER  "/playlists"
#define SETTINGS_FOLDER   "/settings"
#define SETTINGS_FILE     "/settings/settings.json"

// ============================================================
//  Audio Defaults
// ============================================================
#define DEFAULT_VOLUME    18
#define MAX_VOLUME        30
#define MIN_VOLUME        0

// ============================================================
//  System
// ============================================================
#define SERIAL_BAUD       115200
#define MAX_LOG_ENTRIES   50
#define STATUS_POLL_MS    2000
