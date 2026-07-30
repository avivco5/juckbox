#include "StorageManager.h"
#include "config.h"

StorageManager storageManager;

bool StorageManager::begin() {
    Serial.println("[Storage] Initializing SD card...");

    // Use SPI2 (FSPI on ESP32-S3) with custom pins so we don't conflict
    // with any display bus that may share the default SPI pins.
    _spi = new SPIClass(FSPI);   // FSPI = SPI2 on ESP32-S3
    _spi->begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, *_spi, SD_SPI_FREQ)) {
        Serial.println("[Storage] SD.begin() FAILED – check wiring / card");
        _mounted = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Storage] No SD card detected");
        _mounted = false;
        return false;
    }

    const char* typeStr =
        (cardType == CARD_MMC)  ? "MMC"  :
        (cardType == CARD_SD)   ? "SD"   :
        (cardType == CARD_SDHC) ? "SDHC" : "UNKNOWN";

    Serial.printf("[Storage] Card type: %s, size: %llu MB\n",
                  typeStr, SD.cardSize() / (1024ULL * 1024ULL));

    _mounted = true;
    createFolderStructure();
    return true;
}

bool StorageManager::createFolderStructure() {
    bool ok = true;
    const char* dirs[] = { SONGS_FOLDER, PLAYLISTS_FOLDER, SETTINGS_FOLDER };
    for (const char* d : dirs) {
        if (!SD.exists(d)) {
            if (SD.mkdir(d)) {
                Serial.printf("[Storage] Created: %s\n", d);
            } else {
                Serial.printf("[Storage] Failed to create: %s\n", d);
                ok = false;
            }
        }
    }
    return ok;
}

std::vector<SongInfo> StorageManager::listSongs() {
    std::vector<SongInfo> songs;
    if (!_mounted) return songs;

    File dir = SD.open(SONGS_FOLDER);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[Storage] Cannot open /songs");
        return songs;
    }

    int id = 1;
    File entry = dir.openNextFile();
    while (entry) {
        String name = entry.name();
        // Strip any leading directory prefix some library versions add
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);

        if (!entry.isDirectory() && isAllowedExtension(name)) {
            SongInfo s;
            s.id   = id++;
            s.name = name;
            s.path = String(SONGS_FOLDER) + "/" + name;
            s.size = entry.size();
            songs.push_back(s);
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    Serial.printf("[Storage] Found %u songs\n", (unsigned)songs.size());
    return songs;
}

File StorageManager::openSongForWrite(const String& filename) {
    String path = String(SONGS_FOLDER) + "/" + filename;
    return SD.open(path, FILE_WRITE);
}

bool StorageManager::songExists(const String& filename) {
    return SD.exists(String(SONGS_FOLDER) + "/" + filename);
}

// ---- Settings ----------------------------------------------------------

String StorageManager::loadSettings() {
    if (!_mounted || !SD.exists(SETTINGS_FILE)) return "{}";
    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) return "{}";
    String s = f.readString();
    f.close();
    return s;
}

bool StorageManager::saveSettings(const String& json) {
    if (!_mounted) return false;
    File f = SD.open(SETTINGS_FILE, FILE_WRITE);
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

// ---- Playlists ---------------------------------------------------------

std::vector<String> StorageManager::listPlaylists() {
    std::vector<String> result;
    if (!_mounted) return result;

    File dir = SD.open(PLAYLISTS_FOLDER);
    if (!dir || !dir.isDirectory()) return result;

    File entry = dir.openNextFile();
    while (entry) {
        String name = entry.name();
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        if (!entry.isDirectory() && name.endsWith(".json")) {
            result.push_back(name);
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return result;
}

String StorageManager::loadPlaylist(const String& filename) {
    String path = String(PLAYLISTS_FOLDER) + "/" + filename;
    if (!SD.exists(path)) return "{}";
    File f = SD.open(path, FILE_READ);
    if (!f) return "{}";
    String s = f.readString();
    f.close();
    return s;
}

bool StorageManager::savePlaylist(const String& filename, const String& json) {
    if (!_mounted) return false;
    String path = String(PLAYLISTS_FOLDER) + "/" + filename;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

// ---- Static helpers ----------------------------------------------------

String StorageManager::sanitizeFilename(const String& raw) {
    String out;
    for (char c : raw) {
        if (isAlphaNumeric(c) || c == '.' || c == '-' || c == '_') {
            out += c;
        } else if (c == ' ') {
            out += '_';
        }
    }
    // Cap length – keep the extension
    if (out.length() > 60) {
        int dot = out.lastIndexOf('.');
        String ext = (dot >= 0) ? out.substring(dot) : "";
        out = out.substring(0, 56) + ext;
    }
    return out.length() > 0 ? out : "unnamed";
}

bool StorageManager::isAllowedExtension(const String& filename) {
    String lower = filename;
    lower.toLowerCase();
    return lower.endsWith(".mp3") || lower.endsWith(".wav");
}
