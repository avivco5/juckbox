#include "JukeboxUI.h"

#include <Arduino_GFX_Library.h>
#include <FS.h>
#include <SD_MMC.h>
#include <esp_random.h>

#include "AirHockeyGame.h"
#include "AudioController.h"
#include "DisplayDriver.h"
#include "PinballGame.h"
#include "StatusLed.h"
#include "Theme.h"

namespace {
// The SD status dot stays a fixed system color across all themes rather
// than following the palette, since it communicates hardware state, not
// theme decoration.
constexpr uint16_t Success = 0x2E6B;
constexpr int SongsPerPage = 4;
constexpr int ThemesPerPage = 5;
constexpr const char* SettingsPath = "/jukebox_settings.cfg";

int findSwatchIndex(uint32_t colorHex888)
{
    for (int i = 0; i < kThemeSwatchCount; ++i)
    {
        if (kThemeSwatches[i] == colorHex888)
        {
            return i;
        }
    }
    return 0;
}

// Small reusable motifs for the per-theme Home screen decorations. Each
// takes just a canvas, position and color(s) — the 10 decorated themes
// differ only in which of these they call and where, not in bespoke
// per-theme drawing code.
void drawBubble(Arduino_Canvas* gfx, int x, int y, int radius, uint16_t color)
{
    gfx->drawCircle(x, y, radius, color);
}

void drawDot(Arduino_Canvas* gfx, int x, int y, uint16_t color)
{
    gfx->fillCircle(x, y, 2, color);
}

void drawCoin(Arduino_Canvas* gfx, int x, int y, uint16_t ringColor, uint16_t dotColor)
{
    gfx->drawCircle(x, y, 6, ringColor);
    gfx->fillCircle(x, y, 2, dotColor);
}

// 16x16 1bpp icon bitmaps for the Home screen decorations, one per
// box-design theme. Hand-drawn (via a Pillow script, not hand-typed bit
// grids) and thresholded to a clean silhouette, not sourced from image
// files, so there is no asset pipeline to maintain — each is just 32
// bytes of flash and is recolored per theme by drawIcon()'s color arg.
static const uint8_t iconPineapple[] = {0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x03, 0xC0, 0x03, 0xC0, 0x06, 0x60, 0x00, 0x00, 0x07, 0xE0, 0x0F, 0xF0, 0x0F, 0xF0, 0x1F, 0xF8, 0x1F, 0xF8, 0x0F, 0xF0, 0x0F, 0xF0, 0x07, 0xE0, 0x00, 0x00};
static const uint8_t iconStar[] = {0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x03, 0xC0, 0x1F, 0xF8, 0x3F, 0xFC, 0x1F, 0xF8, 0x0F, 0xF0, 0x07, 0xE0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0C, 0x30, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00};
static const uint8_t iconRobot[] = {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x1F, 0xF8, 0x1F, 0xF8, 0x1B, 0xD8, 0x19, 0x98, 0x1F, 0xF8, 0x1F, 0xF8, 0x1C, 0x38, 0x1F, 0xF8, 0x0F, 0xF8, 0x00, 0x00, 0x00, 0x00};
static const uint8_t iconSpaceship[] = {0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x03, 0xC0, 0x02, 0x40, 0x06, 0x60, 0x07, 0xE0, 0x07, 0xE0, 0x07, 0xE0, 0x07, 0xE0, 0x0F, 0xF0, 0x0F, 0xF0, 0x18, 0x18, 0x30, 0x0C, 0x00, 0x00};
static const uint8_t iconSkull[] = {0x00, 0x00, 0x03, 0xC0, 0x07, 0xF0, 0x0F, 0xF0, 0x19, 0x98, 0x19, 0x88, 0x1B, 0xD8, 0x0E, 0x70, 0x0F, 0xF0, 0x07, 0xE0, 0x07, 0xE0, 0x00, 0x00, 0x10, 0x0C, 0x1F, 0xF8, 0x1F, 0xF8, 0x30, 0x0C};
static const uint8_t iconSpeaker[] = {0x00, 0x00, 0x3F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7C, 0x3E, 0x79, 0x9E, 0x7B, 0xDE, 0x7B, 0xDE, 0x79, 0x9E, 0x7C, 0x3E, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFE, 0x00, 0x00};
static const uint8_t iconCassette[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F, 0xFF, 0x63, 0xC7, 0x61, 0x87, 0x61, 0x87, 0x77, 0xEF, 0x7F, 0xFF, 0xFC, 0x3F, 0x7F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t iconTree[] = {0x00, 0x00, 0x01, 0x80, 0x03, 0xC0, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC, 0x03, 0xC0, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00};
static const uint8_t iconGear[] = {0x01, 0x80, 0x01, 0x80, 0x31, 0x8C, 0x37, 0xEC, 0x0F, 0xF0, 0x1F, 0xF8, 0x1C, 0x38, 0xFC, 0x3F, 0xFC, 0x3F, 0x1C, 0x38, 0x1F, 0xF8, 0x0F, 0xF0, 0x37, 0xEC, 0x31, 0x8C, 0x01, 0x80, 0x01, 0x80};
static const uint8_t iconMonster[] = {0x00, 0x00, 0x04, 0x20, 0x0C, 0x30, 0x0F, 0xF0, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFE, 0x31, 0x8E, 0x31, 0x8E, 0x3F, 0xFE, 0x3F, 0xFE, 0x3F, 0xFE, 0x3D, 0xBC, 0x1F, 0xF8, 0x07, 0xE0, 0x00, 0x00};

void drawIcon(Arduino_Canvas* gfx, int x, int y, const uint8_t* bitmap, uint16_t color)
{
    gfx->drawBitmap(x, y, bitmap, 16, 16, color);
}
}

JukeboxUI jukeboxUI;

void JukeboxUI::begin()
{
    randomSeed(esp_random());
    themeManager.begin();
    if (songCount() > 0)
    {
        _songIndex = 0;
    }
    loadSettings();
    audioController.setVolume(_volume);
    _lastProgressTick = millis();
    draw();
}

void JukeboxUI::update()
{
    const uint32_t now = millis();
    const bool actualPlaying = audioController.isPlaying();
    if (_playing != actualPlaying)
    {
        _playing = actualPlaying;
        _dirty = true;
    }

    if (audioController.consumeTrackEnded())
    {
        advanceAfterTrackEnded();
    }

    if (_songIndex >= 0 &&
        now - _lastProgressTick >= 1000)
    {
        _lastProgressTick = now;
        const uint16_t elapsed =
            static_cast<uint16_t>(audioController.elapsedSeconds());
        if (_elapsed != elapsed)
        {
            _elapsed = elapsed;
            _dirty = true;
        }
    }

    if (_page == Page::Pinball)
    {
        pinballGame.update();
        draw();
        return;
    }
    if (_page == Page::AirHockey)
    {
        airHockeyGame.update();
        draw();
        return;
    }

    if (_dirty)
    {
        draw();
    }
}

void JukeboxUI::setButtonA(bool held)
{
    if (_page == Page::Pinball)
    {
        pinballGame.setLeftFlipper(held);
    }
    else if (_page == Page::AirHockey)
    {
        airHockeyGame.setMoveUp(held);
    }
}

void JukeboxUI::setButtonB(bool held)
{
    if (_page == Page::Pinball)
    {
        pinballGame.setRightFlipper(held);
    }
    else if (_page == Page::AirHockey)
    {
        airHockeyGame.setMoveDown(held);
    }
}

void JukeboxUI::openPinball()
{
    pinballGame.begin();
    setPage(Page::Pinball);
}

void JukeboxUI::openAirHockey()
{
    airHockeyGame.begin();
    setPage(Page::AirHockey);
}

void JukeboxUI::exitGame()
{
    pinballGame.setLeftFlipper(false);
    pinballGame.setRightFlipper(false);
    airHockeyGame.setMoveUp(false);
    airHockeyGame.setMoveDown(false);
    setPage(Page::Games);
}

bool JukeboxUI::inRect(
    const TouchPoint& point,
    int x,
    int y,
    int width,
    int height) const
{
    return point.x >= x &&
           point.x < x + width &&
           point.y >= y &&
           point.y < y + height;
}

int JukeboxUI::songCount() const
{
    return static_cast<int>(songLibrary.songs().size());
}

int JukeboxUI::nextSongIndex() const
{
    const int count = songCount();
    if (count <= 1)
    {
        return count == 0 ? -1 : 0;
    }

    if (_shuffle)
    {
        int candidate = _songIndex;
        while (candidate == _songIndex)
        {
            candidate = static_cast<int>(random(count));
        }
        return candidate;
    }

    return (_songIndex + 1) % count;
}

void JukeboxUI::advanceAfterTrackEnded()
{
    if (songCount() == 0)
    {
        return;
    }

    if (_repeatMode == RepeatMode::One)
    {
        selectSong(_songIndex);
        return;
    }

    if (_repeatMode == RepeatMode::All || _shuffle)
    {
        selectSong(nextSongIndex());
    }
}

const char* JukeboxUI::repeatModeLabel() const
{
    switch (_repeatMode)
    {
        case RepeatMode::One:
            return "REPEAT: ONE";
        case RepeatMode::All:
            return "REPEAT: ALL";
        default:
            return "REPEAT: OFF";
    }
}

int JukeboxUI::indexForPath(const String& path) const
{
    const auto& songs = songLibrary.songs();
    for (size_t i = 0; i < songs.size(); ++i)
    {
        if (songs[i].path == path)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void JukeboxUI::loadSettings()
{
    uint32_t customRaw[CustomRoleCount];
    for (int i = 0; i < CustomRoleCount; ++i)
    {
        customRaw[i] = themeManager.customColor(static_cast<CustomColorRole>(i));
    }

    File file = SD_MMC.open(SettingsPath, FILE_READ);
    if (!file)
    {
        return;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        const int eq = line.indexOf('=');
        if (eq < 0)
        {
            continue;
        }
        const String key = line.substring(0, eq);
        const String value = line.substring(eq + 1);

        if (key == "volume")
        {
            _volume = constrain(value.toInt(), 0, 100);
        }
        else if (key == "repeat")
        {
            const int mode = value.toInt();
            if (mode >= 0 && mode <= 2)
            {
                _repeatMode = static_cast<RepeatMode>(mode);
            }
        }
        else if (key == "shuffle")
        {
            _shuffle = value == "1";
        }
        else if (key == "song")
        {
            const int index = indexForPath(value);
            if (index >= 0)
            {
                _songIndex = index;
            }
        }
        else if (key == "theme")
        {
            const int themeIndex = value.toInt();
            if (themeIndex >= 0 && themeIndex < THEME_COUNT)
            {
                themeManager.apply(static_cast<ThemeId>(themeIndex));
            }
        }
        else if (key == "ledmode")
        {
            statusLed.setMode(
                value.toInt() == 1 ? LedMode::Always : LedMode::WhenPlaying);
        }
        else if (key == "custom_bg")
        {
            customRaw[CustomBg] = strtoul(value.c_str(), nullptr, 16);
        }
        else if (key == "custom_panel")
        {
            customRaw[CustomPanel] = strtoul(value.c_str(), nullptr, 16);
        }
        else if (key == "custom_title")
        {
            customRaw[CustomTitle] = strtoul(value.c_str(), nullptr, 16);
        }
        else if (key == "custom_text")
        {
            customRaw[CustomText] = strtoul(value.c_str(), nullptr, 16);
        }
        else if (key == "custom_selected")
        {
            customRaw[CustomSelected] = strtoul(value.c_str(), nullptr, 16);
        }
        else if (key == "custom_border")
        {
            customRaw[CustomBorder] = strtoul(value.c_str(), nullptr, 16);
        }
    }
    file.close();

    themeManager.setCustomColors(
        customRaw[CustomBg],
        customRaw[CustomPanel],
        customRaw[CustomTitle],
        customRaw[CustomText],
        customRaw[CustomSelected],
        customRaw[CustomBorder]);
}

void JukeboxUI::saveSettings()
{
    File file = SD_MMC.open(SettingsPath, FILE_WRITE);
    if (!file)
    {
        return;
    }

    file.printf("volume=%d\n", _volume);
    file.printf("repeat=%d\n", static_cast<int>(_repeatMode));
    file.printf("shuffle=%d\n", _shuffle ? 1 : 0);
    file.printf("theme=%d\n", static_cast<int>(themeManager.current()));
    file.printf(
        "ledmode=%d\n",
        statusLed.mode() == LedMode::Always ? 1 : 0);
    file.printf("custom_bg=%06X\n", themeManager.customColor(CustomBg));
    file.printf("custom_panel=%06X\n", themeManager.customColor(CustomPanel));
    file.printf("custom_title=%06X\n", themeManager.customColor(CustomTitle));
    file.printf("custom_text=%06X\n", themeManager.customColor(CustomText));
    file.printf(
        "custom_selected=%06X\n", themeManager.customColor(CustomSelected));
    file.printf("custom_border=%06X\n", themeManager.customColor(CustomBorder));
    if (_songIndex >= 0 && _songIndex < songCount())
    {
        file.printf("song=%s\n", songLibrary.songs()[_songIndex].path.c_str());
    }
    file.close();
}

void JukeboxUI::adjustVolume(int delta)
{
    _volume = constrain(_volume + delta, 0, 100);
    audioController.setVolume(_volume);
    _dirty = true;
    saveSettings();
}

void JukeboxUI::openThemeEditor()
{
    for (int role = 0; role < CustomRoleCount; ++role)
    {
        _customEditIndex[role] = findSwatchIndex(
            themeManager.customColor(static_cast<CustomColorRole>(role)));
    }
    themeManager.apply(THEME_CUSTOM);
    setPage(Page::ThemeEditor);
}

void JukeboxUI::cycleCustomColor(CustomColorRole role, int direction)
{
    _customEditIndex[role] =
        (_customEditIndex[role] + direction + kThemeSwatchCount) %
        kThemeSwatchCount;

    uint32_t updated[CustomRoleCount];
    for (int i = 0; i < CustomRoleCount; ++i)
    {
        updated[i] = kThemeSwatches[_customEditIndex[i]];
    }
    themeManager.setCustomColors(
        updated[CustomBg],
        updated[CustomPanel],
        updated[CustomTitle],
        updated[CustomText],
        updated[CustomSelected],
        updated[CustomBorder]);
    _dirty = true;
    saveSettings();
}

String JukeboxUI::visibleTitle(
    const String& title,
    size_t maxCharacters) const
{
    if (title.length() <= maxCharacters)
    {
        return title;
    }
    return title.substring(0, maxCharacters - 3) + "...";
}

void JukeboxUI::setPage(Page page)
{
    if (_page != page)
    {
        _page = page;
        _dirty = true;
    }
}

void JukeboxUI::selectSong(int index)
{
    if (songCount() == 0)
    {
        _songIndex = -1;
        _playing = false;
        _dirty = true;
        saveSettings();
        return;
    }

    _songIndex = constrain(index, 0, songCount() - 1);
    _elapsed = 0;
    const LibrarySong& song = songLibrary.songs()[_songIndex];
    _playing = audioController.play(song.path);
    _dirty = true;
    saveSettings();
    Serial.printf(
        "[UI] Selected: %s (%s)\n",
        song.path.c_str(),
        _playing ? "playing" : "audio error");
}

void JukeboxUI::togglePlayPause()
{
    if (_songIndex < 0 && songCount() > 0)
    {
        selectSong(0);
        return;
    }
    if (audioController.isPlaying())
    {
        audioController.pause();
    }
    else if (audioController.isPaused())
    {
        audioController.resume();
    }
    else if (_songIndex >= 0 && _songIndex < songCount())
    {
        audioController.play(songLibrary.songs()[_songIndex].path);
    }
    _playing = audioController.isPlaying();
    _lastProgressTick = millis();
    _dirty = true;
    Serial.printf("[UI] %s\n", _playing ? "Play" : "Pause");
}

void JukeboxUI::nextSong()
{
    if (songCount() > 0)
    {
        const int current = max(0, _songIndex);
        selectSong((current + 1) % songCount());
    }
}

void JukeboxUI::handleTouch(const TouchPoint& point)
{
    Serial.printf("[Touch] x=%u y=%u\n", point.x, point.y);

    if (_page == Page::Pinball || _page == Page::AirHockey)
    {
        if (inRect(point, 422, 4, 54, 22))
        {
            exitGame();
        }
        return;
    }

    if (point.y >= 274)
    {
        if (point.x < 120)
        {
            setPage(Page::Home);
        }
        else if (point.x < 240)
        {
            setPage(Page::Songs);
        }
        else if (point.x < 360)
        {
            setPage(Page::Settings);
        }
        else
        {
            setPage(Page::Games);
        }
        return;
    }

    if (_page == Page::Home)
    {
        if (inRect(point, 262, 138, 76, 76))
        {
            togglePlayPause();
        }
        else if (inRect(point, 201, 146, 60, 60))
        {
            if (songCount() > 0)
            {
                const int current = max(0, _songIndex);
                selectSong((current + songCount() - 1) % songCount());
            }
        }
        else if (inRect(point, 341, 146, 60, 60))
        {
            nextSong();
        }
        else if (inRect(point, 198, 220, 64, 52))
        {
            adjustVolume(-5);
        }
        else if (inRect(point, 398, 220, 64, 52))
        {
            adjustVolume(5);
        }
    }
    else if (_page == Page::Songs)
    {
        if (inRect(point, 374, 44, 82, 24))
        {
            songLibrary.scan();
            if (_songIndex >= songCount())
            {
                _songIndex = songCount() > 0 ? 0 : -1;
                _playing = false;
            }
            _songPage = 0;
            _dirty = true;
            return;
        }

        const int pageCount =
            max(1, (songCount() + SongsPerPage - 1) / SongsPerPage);
        if (inRect(point, 256, 40, 52, 32) && _songPage > 0)
        {
            --_songPage;
            _dirty = true;
            return;
        }
        if (inRect(point, 314, 40, 52, 32) &&
            _songPage + 1 < pageCount)
        {
            ++_songPage;
            _dirty = true;
            return;
        }

        for (int row = 0; row < SongsPerPage; ++row)
        {
            const int index = _songPage * SongsPerPage + row;
            if (index >= songCount())
            {
                break;
            }
            if (inRect(point, 24, 72 + row * 48, 432, 40))
            {
                selectSong(index);
                setPage(Page::Home);
                return;
            }
        }
    }
    else if (_page == Page::Settings)
    {
        if (inRect(point, 24, 110, 138, 28))
        {
            _repeatMode = _repeatMode == RepeatMode::Off
                ? RepeatMode::One
                : (_repeatMode == RepeatMode::One
                       ? RepeatMode::All
                       : RepeatMode::Off);
            _dirty = true;
            saveSettings();
        }
        else if (inRect(point, 170, 110, 138, 28))
        {
            _shuffle = !_shuffle;
            _dirty = true;
            saveSettings();
        }
        else if (inRect(point, 316, 110, 140, 28))
        {
            statusLed.setMode(
                statusLed.mode() == LedMode::WhenPlaying
                    ? LedMode::Always
                    : LedMode::WhenPlaying);
            _dirty = true;
            saveSettings();
        }
        else if (inRect(point, 24, 197, 64, 40))
        {
            themeManager.previous();
            saveSettings();
            _dirty = true;
        }
        else if (inRect(point, 392, 197, 64, 40))
        {
            themeManager.next();
            saveSettings();
            _dirty = true;
        }
        else if (inRect(point, 92, 197, 296, 40))
        {
            _themePage = themeManager.current() / ThemesPerPage;
            setPage(Page::ThemeMenu);
        }
    }
    else if (_page == Page::ThemeMenu)
    {
        if (inRect(point, 0, 40, 40, 234))
        {
            themeManager.previous();
            saveSettings();
            _themePage = static_cast<int>(themeManager.current()) / ThemesPerPage;
            _dirty = true;
            return;
        }
        if (inRect(point, 440, 40, 40, 234))
        {
            themeManager.next();
            saveSettings();
            _themePage = static_cast<int>(themeManager.current()) / ThemesPerPage;
            _dirty = true;
            return;
        }

        const int pageCount =
            max(1, (static_cast<int>(THEME_COUNT) + ThemesPerPage - 1) / ThemesPerPage);
        if (inRect(point, 256, 40, 52, 32) && _themePage > 0)
        {
            --_themePage;
            _dirty = true;
            return;
        }
        if (inRect(point, 314, 40, 52, 32) && _themePage + 1 < pageCount)
        {
            ++_themePage;
            _dirty = true;
            return;
        }

        for (int row = 0; row < ThemesPerPage; ++row)
        {
            const int index = _themePage * ThemesPerPage + row;
            if (index >= THEME_COUNT)
            {
                break;
            }
            const int y = 72 + row * 40;
            if (index == THEME_CUSTOM && inRect(point, 344, y + 4, 68, 26))
            {
                openThemeEditor();
                return;
            }
            if (inRect(point, 40, y, 400, 34))
            {
                themeManager.apply(static_cast<ThemeId>(index));
                saveSettings();
                _dirty = true;
                setPage(Page::Settings);
                return;
            }
        }
    }
    else if (_page == Page::ThemeEditor)
    {
        for (int role = 0; role < CustomRoleCount; ++role)
        {
            const int y = 70 + role * 32;
            if (inRect(point, 112, y, 34, 28))
            {
                cycleCustomColor(static_cast<CustomColorRole>(role), -1);
                return;
            }
            if (inRect(point, 294, y, 34, 28))
            {
                cycleCustomColor(static_cast<CustomColorRole>(role), 1);
                return;
            }
        }
    }
    else if (_page == Page::Games)
    {
        if (inRect(point, 24, 84, 432, 60))
        {
            openPinball();
        }
        else if (inRect(point, 24, 152, 432, 60))
        {
            openAirHockey();
        }
    }
}

void JukeboxUI::drawCenteredText(
    const char* text,
    int centerX,
    int y,
    uint8_t size,
    uint16_t color)
{
    auto* gfx = displayDriver.canvas();
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    gfx->setTextSize(size);
    gfx->getTextBounds(
        text,
        0,
        y,
        &x1,
        &y1,
        &width,
        &height);
    gfx->setTextColor(color);
    gfx->setCursor(centerX - width / 2, y);
    gfx->print(text);
}

void JukeboxUI::drawTopBar()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->fillRect(0, 0, DisplayDriver::Width, 40, theme.surface);
    gfx->drawFastHLine(0, 39, DisplayDriver::Width, theme.border);

    gfx->fillCircle(22, 20, 10, theme.primary);
    gfx->fillCircle(22, 20, 4, theme.background);
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(42, 12);
    gfx->print(themeManager.titleText());

    gfx->fillCircle(
        388,
        20,
        5,
        songLibrary.isMounted() ? Success : 0xF9A6);
    gfx->setTextColor(theme.muted);
    gfx->setTextSize(1);
    gfx->setCursor(400, 17);
    gfx->print(songLibrary.isMounted() ? "SD READY" : "NO SD");
}

void JukeboxUI::drawNavigation()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->fillRect(0, 274, DisplayDriver::Width, 46, theme.surface);
    gfx->drawFastHLine(0, 274, DisplayDriver::Width, theme.border);

    const char* labels[] = {"HOME", "SONGS", "SETTINGS", "GAMES"};
    const Page pages[] = {Page::Home, Page::Songs, Page::Settings, Page::Games};
    for (int i = 0; i < 4; ++i)
    {
        const int x = i * 120;
        const bool active = _page == pages[i] ||
            ((_page == Page::ThemeMenu || _page == Page::ThemeEditor) &&
             pages[i] == Page::Settings);
        if (active)
        {
            gfx->fillRoundRect(x + 8, 281, 104, 31, 10, theme.primary);
        }
        drawCenteredText(
            labels[i],
            x + 60,
            292,
            1,
            active ? theme.darkText : theme.muted);
    }
}

void JukeboxUI::drawRecord(int centerX, int centerY, int radius)
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->fillCircle(centerX, centerY, radius, 0x0042);
    gfx->drawCircle(centerX, centerY, radius - 2, theme.primary);
    gfx->drawCircle(centerX, centerY, radius - 15, theme.border);
    gfx->drawCircle(centerX, centerY, radius - 29, theme.border);
    gfx->fillCircle(centerX, centerY, 24, theme.accent);
    gfx->fillCircle(centerX, centerY, 8, theme.surface);

    if (_playing)
    {
        gfx->fillCircle(centerX + 44, centerY - 44, 5, theme.primary);
        gfx->drawLine(
            centerX + 44,
            centerY - 44,
            centerX + 56,
            centerY - 56,
            theme.primary);
    }
}

void JukeboxUI::drawControlButton(
    int centerX,
    int centerY,
    int radius,
    uint16_t color,
    const char* label,
    uint16_t textColor)
{
    auto* gfx = displayDriver.canvas();
    gfx->fillCircle(centerX, centerY, radius, color);
    drawCenteredText(
        label,
        centerX,
        centerY - 5,
        1,
        textColor);
}

void JukeboxUI::drawHome()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();

    gfx->fillRoundRect(16, 54, 170, 198, 18, theme.surface);
    gfx->drawRoundRect(16, 54, 170, 198, 18, theme.border);
    drawRecord(101, 139, 66);
    drawCenteredText(
        _songIndex < 0
            ? "NO SONG"
            : (_playing ? "NOW PLAYING" : "PAUSED"),
        101,
        224,
        1,
        _playing ? theme.primary : theme.muted);

    if (_songIndex < 0 || songCount() == 0)
    {
        gfx->setTextColor(theme.text);
        gfx->setTextSize(2);
        gfx->setCursor(204, 66);
        gfx->print("NO SONG SELECTED");
        gfx->setTextColor(theme.muted);
        gfx->setTextSize(1);
        gfx->setCursor(205, 94);
        gfx->print(songLibrary.status());
    }
    else
    {
        const LibrarySong& song = songLibrary.songs()[_songIndex];
        const String title = visibleTitle(song.name, 20);
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(204, 58);
        gfx->print(title);
    gfx->setTextColor(theme.muted);
    gfx->setTextSize(1);
    gfx->setCursor(205, 84);
        gfx->print("MICROSD /songs");
    }

    uint32_t totalSeconds = 0;
    if (_songIndex >= 0 && _songIndex < songCount())
    {
        totalSeconds = songLibrary.songs()[_songIndex].durationSeconds;
    }

    const int progressWidth = 248;
    const uint32_t clampedElapsed =
        static_cast<uint32_t>(_elapsed) > totalSeconds
            ? totalSeconds
            : static_cast<uint32_t>(_elapsed);
    const int filled = totalSeconds > 0
        ? static_cast<int>(
              (static_cast<uint32_t>(progressWidth) * clampedElapsed) /
              totalSeconds)
        : 0;
    gfx->fillRoundRect(204, 111, progressWidth, 8, 4, theme.border);
    gfx->fillRoundRect(204, 111, filled, 8, 4, theme.primary);

    char elapsed[12];
    char duration[12];
    snprintf(
        elapsed,
        sizeof(elapsed),
        "%u:%02u",
        _elapsed / 60,
        _elapsed % 60);
    if (totalSeconds > 0)
    {
        snprintf(
            duration,
            sizeof(duration),
            "%u:%02u",
            static_cast<unsigned>(totalSeconds / 60),
            static_cast<unsigned>(totalSeconds % 60));
    }
    else
    {
        snprintf(duration, sizeof(duration), "--:--");
    }
    gfx->setTextSize(1);
    gfx->setTextColor(theme.muted);
    gfx->setCursor(204, 126);
    gfx->print(elapsed);
    gfx->setCursor(425, 126);
    gfx->print(duration);

    drawControlButton(231, 176, 28, theme.surfaceRaised, "<<", theme.text);
    drawControlButton(
        300,
        176,
        38,
        theme.primary,
        _playing ? "II" : ">",
        theme.darkText);
    drawControlButton(371, 176, 28, theme.surfaceRaised, ">>", theme.text);

    gfx->fillRoundRect(204, 222, 248, 48, 14, theme.surface);
    gfx->drawRoundRect(204, 222, 248, 48, 14, theme.border);
    drawControlButton(230, 246, 22, theme.surfaceRaised, "-", theme.text);
    drawControlButton(426, 246, 22, theme.surfaceRaised, "+", theme.text);

    const int volumeX = 264;
    const int volumeWidth = 128;
    gfx->fillRoundRect(volumeX, 250, volumeWidth, 6, 3, theme.border);
    gfx->fillRoundRect(
        volumeX,
        250,
        volumeWidth * _volume / 100,
        6,
        3,
        theme.accent);
    char volume[8];
    snprintf(volume, sizeof(volume), "%d%%", _volume);
    drawCenteredText(volume, 328, 227, 1, theme.text);

    drawThemeDecorations();
}

void JukeboxUI::drawThemeDecorations()
{
    // Confined to the narrow margins left of the album art panel and
    // right of the volume panel (never over a touch target or text) so
    // they're purely cosmetic accents matching each physical box design.
    // Classic and Custom intentionally have none — Classic is the plain
    // pre-theme look, and Custom has no fixed "vibe" to draw a motif for.
    auto* gfx = displayDriver.canvas();
    const ThemeColors& theme = themeManager.colors();
    constexpr int leftX = 0;
    constexpr int rightX = 458;
    constexpr int iconY = 145;

    const uint8_t* icon = nullptr;
    switch (themeManager.current())
    {
        case THEME_PINEAPPLE: icon = iconPineapple; break;
        case THEME_RETRO_ARCADE: icon = iconStar; break;
        case THEME_ROBOT: icon = iconRobot; break;
        case THEME_SPACESHIP: icon = iconSpaceship; break;
        case THEME_PIRATE: icon = iconSkull; break;
        case THEME_VINTAGE_RADIO: icon = iconSpeaker; break;
        case THEME_CASSETTE: icon = iconCassette; break;
        case THEME_FOREST: icon = iconTree; break;
        case THEME_STEAMPUNK: icon = iconGear; break;
        case THEME_MONSTER: icon = iconMonster; break;
        default: break;  // Classic, Custom: no icon
    }
    if (!icon)
    {
        return;
    }

    drawIcon(gfx, leftX, iconY, icon, theme.accent);
    drawIcon(gfx, rightX, iconY, icon, theme.accent);

    // A couple of small accent motifs for themes where they add life
    // without cluttering the icon (bubbles for underwater, sparkle dots
    // for neon/space, a coin for treasure, a firefly for the forest).
    switch (themeManager.current())
    {
        case THEME_PINEAPPLE:
            drawBubble(gfx, leftX + 8, iconY - 20, 3, theme.text);
            drawBubble(gfx, rightX + 8, iconY + 34, 3, theme.text);
            break;
        case THEME_RETRO_ARCADE:
            drawDot(gfx, leftX + 8, iconY - 20, theme.primary);
            drawDot(gfx, rightX + 8, iconY + 34, theme.primary);
            break;
        case THEME_SPACESHIP:
            drawDot(gfx, leftX + 8, iconY - 20, theme.text);
            drawDot(gfx, rightX + 8, iconY + 34, theme.text);
            break;
        case THEME_PIRATE:
            drawCoin(gfx, leftX + 8, iconY - 18, theme.primary, theme.text);
            break;
        case THEME_FOREST:
            drawDot(gfx, rightX + 8, iconY + 32, theme.accent);
            break;
        default:
            break;
    }
}

void JukeboxUI::drawSongs()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(24, 48);
    gfx->print("SONG LIBRARY");

    gfx->fillRoundRect(374, 44, 82, 24, 8, theme.primary);
    drawCenteredText("SCAN", 415, 52, 1, theme.darkText);

    const int pageCount =
        max(1, (songCount() + SongsPerPage - 1) / SongsPerPage);
    if (pageCount > 1)
    {
        gfx->fillRoundRect(256, 40, 52, 32, 8, theme.surfaceRaised);
        gfx->fillRoundRect(314, 40, 52, 32, 8, theme.surfaceRaised);
        drawCenteredText("<", 282, 52, 1, theme.text);
        drawCenteredText(">", 340, 52, 1, theme.text);
    }

    if (songCount() == 0)
    {
        drawCenteredText(
            songLibrary.status().c_str(),
            240,
            130,
            1,
            theme.muted);
        drawCenteredText(
            "PUT MP3/WAV FILES IN /songs",
            240,
            154,
            1,
            theme.text);
        return;
    }

    for (int row = 0; row < SongsPerPage; ++row)
    {
        const int index = _songPage * SongsPerPage + row;
        if (index >= songCount())
        {
            break;
        }

        const LibrarySong& song = songLibrary.songs()[index];
        const int y = 72 + row * 48;
        const bool selected = index == _songIndex;
        gfx->fillRoundRect(
            24,
            y,
            432,
            40,
            12,
            selected ? theme.surfaceRaised : theme.surface);
        gfx->drawRoundRect(
            24,
            y,
            432,
            40,
            12,
            selected ? theme.primary : theme.border);

        gfx->fillCircle(48, y + 20, 12, selected ? theme.primary : theme.border);
        drawCenteredText(
            String(index + 1).c_str(),
            48,
            y + 17,
            1,
            selected ? theme.darkText : theme.text);

        gfx->setTextColor(theme.text);
        gfx->setTextSize(1);
        gfx->setCursor(72, y + 10);
        gfx->print(visibleTitle(song.name, 34));
        gfx->setTextColor(theme.muted);
        gfx->setCursor(72, y + 25);
        gfx->printf(
            "%.1f MB",
            static_cast<double>(song.size) / (1024.0 * 1024.0));

        gfx->setTextColor(selected ? theme.primary : theme.muted);
        gfx->setCursor(421, y + 17);
        gfx->print(selected && _playing ? "PLAY" : ">");
    }
}

void JukeboxUI::drawSettings()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(24, 48);
    gfx->print("SETTINGS");

    gfx->fillRoundRect(24, 76, 432, 70, 16, theme.surface);
    gfx->drawRoundRect(24, 76, 432, 70, 16, theme.border);
    gfx->setTextColor(theme.muted);
    gfx->setTextSize(1);
    gfx->setCursor(44, 88);
    gfx->print("PLAYBACK & LED");

    gfx->fillRoundRect(24, 110, 138, 28, 10, theme.surfaceRaised);
    drawCenteredText(repeatModeLabel(), 93, 118, 1, theme.text);

    gfx->fillRoundRect(
        170,
        110,
        138,
        28,
        10,
        _shuffle ? theme.primary : theme.surfaceRaised);
    drawCenteredText(
        _shuffle ? "SHUFFLE: ON" : "SHUFFLE: OFF",
        239,
        118,
        1,
        _shuffle ? theme.darkText : theme.text);

    const bool ledAlways = statusLed.mode() == LedMode::Always;
    gfx->fillRoundRect(
        316,
        110,
        140,
        28,
        10,
        ledAlways ? theme.primary : theme.surfaceRaised);
    drawCenteredText(
        ledAlways ? "LED: ALWAYS" : "LED: AUTO",
        386,
        118,
        1,
        ledAlways ? theme.darkText : theme.text);

    gfx->fillRoundRect(24, 158, 432, 88, 16, theme.surface);
    gfx->drawRoundRect(24, 158, 432, 88, 16, theme.border);
    gfx->setTextColor(theme.muted);
    gfx->setTextSize(1);
    gfx->setCursor(44, 170);
    gfx->print("THEME");

    gfx->fillRoundRect(24, 197, 64, 40, 14, theme.surfaceRaised);
    drawCenteredText("<", 56, 212, 1, theme.text);

    gfx->fillRoundRect(92, 197, 296, 40, 14, theme.surfaceRaised);
    gfx->drawRoundRect(92, 197, 296, 40, 14, theme.primary);
    drawCenteredText(
        themeManager.displayName(themeManager.current()),
        240,
        212,
        1,
        theme.text);

    gfx->fillRoundRect(392, 197, 64, 40, 14, theme.surfaceRaised);
    drawCenteredText(">", 424, 212, 1, theme.text);
}

void JukeboxUI::drawThemeMenu()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(24, 48);
    gfx->print("CHOOSE THEME");

    // Step one theme at a time (wraps around); tap zones extend the full
    // height of the list below so they're easy to hit, not just the drawn
    // pill. The list is narrowed to 40-440 (see below) to leave room for
    // these wider edge buttons without overlapping it.
    gfx->fillRoundRect(2, 117, 36, 80, 12, theme.surfaceRaised);
    gfx->drawRoundRect(2, 117, 36, 80, 12, theme.border);
    drawCenteredText("<", 20, 147, 2, theme.primary);
    gfx->fillRoundRect(442, 117, 36, 80, 12, theme.surfaceRaised);
    gfx->drawRoundRect(442, 117, 36, 80, 12, theme.border);
    drawCenteredText(">", 460, 147, 2, theme.primary);

    const int pageCount =
        max(1, (static_cast<int>(THEME_COUNT) + ThemesPerPage - 1) / ThemesPerPage);
    if (pageCount > 1)
    {
        gfx->fillRoundRect(256, 40, 52, 32, 8, theme.surfaceRaised);
        gfx->fillRoundRect(314, 40, 52, 32, 8, theme.surfaceRaised);
        drawCenteredText("<", 282, 52, 1, theme.text);
        drawCenteredText(">", 340, 52, 1, theme.text);
    }

    for (int row = 0; row < ThemesPerPage; ++row)
    {
        const int index = _themePage * ThemesPerPage + row;
        if (index >= THEME_COUNT)
        {
            break;
        }

        const int y = 72 + row * 40;
        const bool selected = index == static_cast<int>(themeManager.current());
        gfx->fillRoundRect(
            40,
            y,
            400,
            34,
            10,
            selected ? theme.surfaceRaised : theme.surface);
        gfx->drawRoundRect(
            40,
            y,
            400,
            34,
            10,
            selected ? theme.primary : theme.border);

        gfx->setTextColor(selected ? theme.primary : theme.text);
        gfx->setTextSize(1);
        gfx->setCursor(52, y + 12);
        gfx->print(themeManager.displayName(static_cast<ThemeId>(index)));

        if (index == THEME_CUSTOM)
        {
            gfx->fillRoundRect(344, y + 4, 68, 26, 8, theme.primary);
            drawCenteredText("EDIT", 378, y + 12, 1, theme.darkText);
        }
        else if (selected)
        {
            drawCenteredText("SELECTED", 372, y + 12, 1, theme.primary);
        }
    }
}

void JukeboxUI::drawThemeEditor()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(24, 48);
    gfx->print("CUSTOM THEME");

    for (int role = 0; role < CustomRoleCount; ++role)
    {
        const int y = 70 + role * 32;
        const uint32_t colorHex = kThemeSwatches[_customEditIndex[role]];
        const uint16_t swatchColor = gfx->color565(
            static_cast<uint8_t>((colorHex >> 16) & 0xFF),
            static_cast<uint8_t>((colorHex >> 8) & 0xFF),
            static_cast<uint8_t>(colorHex & 0xFF));

        gfx->setTextColor(theme.muted);
        gfx->setTextSize(1);
        gfx->setCursor(24, y + 10);
        gfx->print(kCustomRoleLabels[role]);

        gfx->fillRoundRect(112, y, 34, 28, 8, theme.surfaceRaised);
        drawCenteredText("<", 129, y + 8, 1, theme.text);

        gfx->fillRoundRect(150, y, 140, 28, 8, swatchColor);
        gfx->drawRoundRect(150, y, 140, 28, 8, theme.border);

        gfx->fillRoundRect(294, y, 34, 28, 8, theme.surfaceRaised);
        drawCenteredText(">", 311, y + 8, 1, theme.text);

        char hex[8];
        snprintf(hex, sizeof(hex), "#%06lX", static_cast<unsigned long>(colorHex));
        gfx->setTextColor(theme.text);
        gfx->setTextSize(1);
        gfx->setCursor(330, y + 10);
        gfx->print(hex);
    }
}

void JukeboxUI::drawGames()
{
    const ThemeColors& theme = themeManager.colors();
    auto* gfx = displayDriver.canvas();
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(24, 48);
    gfx->print("GAMES");

    gfx->fillRoundRect(24, 84, 432, 60, 16, theme.surface);
    gfx->drawRoundRect(24, 84, 432, 60, 16, theme.primary);
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(44, 102);
    gfx->print("PINBALL");
    drawCenteredText(">", 428, 106, 2, theme.primary);

    gfx->fillRoundRect(24, 152, 432, 60, 16, theme.surface);
    gfx->drawRoundRect(24, 152, 432, 60, 16, theme.primary);
    gfx->setTextColor(theme.text);
    gfx->setTextSize(2);
    gfx->setCursor(44, 170);
    gfx->print("AIR HOCKEY");
    drawCenteredText(">", 428, 174, 2, theme.primary);

    gfx->setTextColor(theme.muted);
    gfx->setTextSize(1);
    gfx->setCursor(24, 232);
    gfx->print("BUTTONS BECOME GAME CONTROLS - EXIT ON SCREEN TO RETURN");
    gfx->setCursor(24, 248);
    gfx->print("MUSIC KEEPS PLAYING IN THE BACKGROUND");
}

void JukeboxUI::drawPinball()
{
    auto* gfx = displayDriver.canvas();
    pinballGame.draw(gfx);
    drawGameExitButton();
}

void JukeboxUI::drawAirHockey()
{
    auto* gfx = displayDriver.canvas();
    airHockeyGame.draw(gfx);
    drawGameExitButton();
}

void JukeboxUI::drawGameExitButton()
{
    // The physical buttons are busy being game controls while a game
    // is running, so they can't back out of it - this is the only way.
    auto* gfx = displayDriver.canvas();
    gfx->fillRoundRect(422, 4, 54, 22, 6, 0x39C7);
    gfx->drawRoundRect(422, 4, 54, 22, 6, 0xFFFF);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(1);
    gfx->setCursor(432, 10);
    gfx->print("EXIT");
}

void JukeboxUI::draw()
{
    // Games own the full screen (their own dark playfield, no jukebox
    // chrome) and redraw every frame for animation, unlike every other
    // page here which only redraws when something actually changed.
    if (_page == Page::Pinball || _page == Page::AirHockey)
    {
        if (_page == Page::Pinball)
        {
            drawPinball();
        }
        else
        {
            drawAirHockey();
        }
        displayDriver.flush();
        _dirty = false;
        return;
    }

    auto* gfx = displayDriver.canvas();
    gfx->fillScreen(themeManager.colors().background);
    drawTopBar();

    switch (_page)
    {
        case Page::Home:
            drawHome();
            break;
        case Page::Songs:
            drawSongs();
            break;
        case Page::Settings:
            drawSettings();
            break;
        case Page::ThemeMenu:
            drawThemeMenu();
            break;
        case Page::ThemeEditor:
            drawThemeEditor();
            break;
        case Page::Games:
            drawGames();
            break;
        case Page::Pinball:
        case Page::AirHockey:
            break;  // handled above
    }

    drawNavigation();
    displayDriver.flush();
    _dirty = false;
}
