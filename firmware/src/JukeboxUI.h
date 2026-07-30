#pragma once

#include <Arduino.h>

#include "SongLibrary.h"
#include "Theme.h"
#include "TouchController.h"

class JukeboxUI {
public:
    void begin();
    void update();
    void handleTouch(const TouchPoint& point);
    void togglePlayPause();
    void nextSong();
    bool isPlayingGame() const { return _page == Page::Pinball || _page == Page::AirHockey; }
    // Physical PLAY/PAUSE (A) and NEXT (B) buttons, repurposed as game
    // controls while a game is running; dispatched to whichever game
    // is currently active. Named for the buttons, not for what any one
    // game does with them, since that differs per game (flippers vs.
    // paddle up/down).
    void setButtonA(bool held);
    void setButtonB(bool held);

private:
    enum class Page {
        Home,
        Songs,
        Settings,
        ThemeMenu,
        ThemeEditor,
        Games,
        Pinball,
        AirHockey,
    };

    enum class RepeatMode {
        Off,
        One,
        All,
    };

    void draw();
    void drawTopBar();
    void drawNavigation();
    void drawHome();
    void drawSongs();
    void drawSettings();
    void drawThemeMenu();
    void drawThemeEditor();
    void drawThemeDecorations();
    void drawGames();
    void drawPinball();
    void drawAirHockey();
    void drawGameExitButton();
    void drawRecord(int centerX, int centerY, int radius);
    void drawControlButton(
        int centerX,
        int centerY,
        int radius,
        uint16_t color,
        const char* label,
        uint16_t textColor);
    void drawCenteredText(
        const char* text,
        int centerX,
        int y,
        uint8_t size,
        uint16_t color);
    bool inRect(
        const TouchPoint& point,
        int x,
        int y,
        int width,
        int height) const;
    void selectSong(int index);
    void setPage(Page page);
    String visibleTitle(const String& title, size_t maxCharacters) const;
    int songCount() const;
    int nextSongIndex() const;
    void advanceAfterTrackEnded();
    const char* repeatModeLabel() const;
    void adjustVolume(int delta);
    void loadSettings();
    void saveSettings();
    int indexForPath(const String& path) const;
    void openThemeEditor();
    void cycleCustomColor(CustomColorRole role, int direction);
    void openPinball();
    void openAirHockey();
    void exitGame();

    Page _page = Page::Home;
    bool _playing = false;
    int _songIndex = -1;
    int _songPage = 0;
    int _themePage = 0;
    int _customEditIndex[CustomRoleCount] = {};
    int _volume = 68;
    uint16_t _elapsed = 0;
    uint32_t _lastProgressTick = 0;
    bool _dirty = true;
    RepeatMode _repeatMode = RepeatMode::Off;
    bool _shuffle = false;
};

extern JukeboxUI jukeboxUI;
