#include "SongLibrary.h"

#include <FS.h>
#include <SD_MMC.h>

#include <algorithm>
#include <string.h>

namespace {
constexpr int SdClock = 5;
constexpr int SdCommand = 4;
constexpr int SdData0 = 6;
constexpr int SdData1 = 7;
constexpr int SdData2 = 2;
constexpr int SdData3 = 3;
constexpr const char* SongsFolder = "/songs";

// Reads enough of a WAV file's RIFF chunks to compute exact duration from
// the "fmt " and "data" chunk sizes.
uint32_t estimateWavDurationSeconds(File& file)
{
    uint8_t header[12];
    if (file.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0)
    {
        return 0;
    }

    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataSize = 0;

    while (file.available() >= 8)
    {
        uint8_t chunkId[4];
        uint8_t chunkSizeBytes[4];
        if (file.read(chunkId, 4) != 4 || file.read(chunkSizeBytes, 4) != 4)
        {
            break;
        }
        const uint32_t chunkSize =
            chunkSizeBytes[0] |
            (static_cast<uint32_t>(chunkSizeBytes[1]) << 8) |
            (static_cast<uint32_t>(chunkSizeBytes[2]) << 16) |
            (static_cast<uint32_t>(chunkSizeBytes[3]) << 24);

        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            uint8_t fmt[16];
            if (file.read(fmt, sizeof(fmt)) != sizeof(fmt))
            {
                break;
            }
            channels = fmt[2] | (static_cast<uint16_t>(fmt[3]) << 8);
            sampleRate =
                fmt[4] |
                (static_cast<uint32_t>(fmt[5]) << 8) |
                (static_cast<uint32_t>(fmt[6]) << 16) |
                (static_cast<uint32_t>(fmt[7]) << 24);
            bitsPerSample = fmt[14] | (static_cast<uint16_t>(fmt[15]) << 8);
            const uint32_t remaining = chunkSize - 16 + (chunkSize % 2);
            if (remaining > 0)
            {
                file.seek(file.position() + remaining);
            }
        }
        else if (memcmp(chunkId, "data", 4) == 0)
        {
            dataSize = chunkSize;
            break;
        }
        else
        {
            file.seek(file.position() + chunkSize + (chunkSize % 2));
        }
    }

    if (sampleRate == 0 || channels == 0 || bitsPerSample == 0 || dataSize == 0)
    {
        return 0;
    }

    const uint32_t bytesPerSecond =
        sampleRate * channels * (bitsPerSample / 8);
    return bytesPerSecond > 0 ? dataSize / bytesPerSecond : 0;
}

// Estimates MP3 duration from the bitrate of the first MPEG-1 Layer III
// frame found after any leading ID3v2 tag, assuming constant bitrate.
// Inaccurate for VBR-encoded files (no Xing/VBRI header parsing).
uint32_t estimateMp3DurationSeconds(File& file, uint32_t fileSize)
{
    static const int BitrateTableV1L3[16] = {
        0, 32, 40, 48, 56, 64, 80, 96,
        112, 128, 160, 192, 224, 256, 320, 0};

    uint32_t searchStart = 0;
    uint8_t id3[10];
    if (file.read(id3, sizeof(id3)) == sizeof(id3) &&
        memcmp(id3, "ID3", 3) == 0)
    {
        const uint32_t tagSize =
            (static_cast<uint32_t>(id3[6] & 0x7F) << 21) |
            (static_cast<uint32_t>(id3[7] & 0x7F) << 14) |
            (static_cast<uint32_t>(id3[8] & 0x7F) << 7) |
            (id3[9] & 0x7F);
        searchStart = 10 + tagSize;
    }
    file.seek(searchStart);

    uint8_t window[4] = {0, 0, 0, 0};
    const uint32_t searchLimit =
        std::min(fileSize, searchStart + 65536U);
    for (uint32_t pos = searchStart; pos < searchLimit; ++pos)
    {
        uint8_t b;
        if (file.read(&b, 1) != 1)
        {
            break;
        }
        window[0] = window[1];
        window[1] = window[2];
        window[2] = window[3];
        window[3] = b;

        if (pos < searchStart + 3)
        {
            continue;
        }

        if (window[0] == 0xFF && (window[1] & 0xE0) == 0xE0)
        {
            const uint8_t versionBits = (window[1] >> 3) & 0x03;
            const uint8_t layerBits = (window[1] >> 1) & 0x03;
            const uint8_t bitrateIndex = (window[2] >> 4) & 0x0F;

            if (versionBits == 0x03 && layerBits == 0x01 &&
                bitrateIndex > 0 && bitrateIndex < 15)
            {
                const int bitrateKbps = BitrateTableV1L3[bitrateIndex];
                if (bitrateKbps > 0 && fileSize > searchStart)
                {
                    const uint32_t audioBytes = fileSize - searchStart;
                    return (audioBytes * 8) /
                           (static_cast<uint32_t>(bitrateKbps) * 1000);
                }
            }
        }
    }
    return 0;
}
}

SongLibrary songLibrary;

bool SongLibrary::begin()
{
    Serial.println("[SD] Initializing SDMMC...");

    if (!SD_MMC.setPins(
            SdClock,
            SdCommand,
            SdData0,
            SdData1,
            SdData2,
            SdData3))
    {
        _status = "SD PIN ERROR";
        Serial.println("[SD] setPins failed");
        return false;
    }

    // false = 4-bit mode. Never format the user's card automatically.
    if (!SD_MMC.begin("/sdcard", false, false))
    {
        _status = "CARD MOUNT FAILED";
        Serial.println("[SD] Card mount failed");
        return false;
    }

    if (SD_MMC.cardType() == CARD_NONE)
    {
        _status = "NO CARD";
        Serial.println("[SD] No card detected");
        SD_MMC.end();
        return false;
    }

    _mounted = true;
    Serial.printf(
        "[SD] Card mounted: %llu MB\n",
        SD_MMC.cardSize() / (1024ULL * 1024ULL));

    if (!SD_MMC.exists(SongsFolder))
    {
        if (!SD_MMC.mkdir(SongsFolder))
        {
            _status = "CANNOT CREATE /songs";
            Serial.println("[SD] Cannot create /songs");
            return false;
        }
        Serial.println("[SD] Created /songs");
    }

    return scan();
}

bool SongLibrary::scan()
{
    _songs.clear();

    if (!_mounted)
    {
        _status = "SD NOT MOUNTED";
        return false;
    }

    File directory = SD_MMC.open(SongsFolder);
    if (!directory || !directory.isDirectory())
    {
        _status = "CANNOT OPEN /songs";
        Serial.println("[SD] Cannot open /songs");
        return false;
    }

    File file = directory.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            String filename = file.name();
            const int slash = filename.lastIndexOf('/');
            if (slash >= 0)
            {
                filename = filename.substring(slash + 1);
            }

            if (isAudioFile(filename))
            {
                LibrarySong song;
                song.name = displayName(filename);
                song.path = String(SongsFolder) + "/" + filename;
                song.size = file.size();

                String lowerName = filename;
                lowerName.toLowerCase();
                if (lowerName.endsWith(".wav"))
                {
                    song.durationSeconds = estimateWavDurationSeconds(file);
                }
                else if (lowerName.endsWith(".mp3"))
                {
                    song.durationSeconds = estimateMp3DurationSeconds(
                        file,
                        static_cast<uint32_t>(song.size));
                }

                _songs.push_back(song);
            }
        }

        file.close();
        file = directory.openNextFile();
    }
    directory.close();

    std::sort(
        _songs.begin(),
        _songs.end(),
        [](const LibrarySong& left, const LibrarySong& right) {
            String leftName = left.name;
            String rightName = right.name;
            leftName.toLowerCase();
            rightName.toLowerCase();
            return leftName < rightName;
        });

    _status = _songs.empty()
        ? "NO MP3/WAV IN /songs"
        : String(_songs.size()) + " SONGS FOUND";

    Serial.printf(
        "[SD] Found %u audio file(s) in /songs\n",
        static_cast<unsigned>(_songs.size()));
    for (const LibrarySong& song : _songs)
    {
        Serial.printf(
            "[SD] %s (%u bytes, %u:%02u)\n",
            song.path.c_str(),
            static_cast<unsigned>(song.size),
            static_cast<unsigned>(song.durationSeconds / 60),
            static_cast<unsigned>(song.durationSeconds % 60));
    }

    return true;
}

bool SongLibrary::isAudioFile(const String& filename)
{
    String lower = filename;
    lower.toLowerCase();
    return lower.endsWith(".mp3") ||
           lower.endsWith(".wav");
}

String SongLibrary::displayName(const String& filename)
{
    const int dot = filename.lastIndexOf('.');
    return dot > 0
        ? filename.substring(0, dot)
        : filename;
}
