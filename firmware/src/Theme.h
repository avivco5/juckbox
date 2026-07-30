#pragma once

#include <Arduino.h>

// 10 UI themes matching the physical jukebox box designs, plus "Classic"
// (the app's original color scheme, kept as an 11th option). Each themed
// design is defined from the small set of colors an artist would actually
// pick (background/panel/title/text/selected/border); the remaining shades
// used by the UI (raised surface, muted text, accent, contrasting button
// text) are derived once at theme-switch time in Theme.cpp so adding a
// theme only requires six hex colors, not nine. THEME_CLASSIC is the
// exception — it specifies all nine directly to reproduce the original
// look exactly. New themes must be appended before THEME_COUNT, never
// inserted earlier, since the theme index is persisted to disk.
enum ThemeId {
    THEME_PINEAPPLE,
    THEME_RETRO_ARCADE,
    THEME_ROBOT,
    THEME_SPACESHIP,
    THEME_PIRATE,
    THEME_VINTAGE_RADIO,
    THEME_CASSETTE,
    THEME_FOREST,
    THEME_STEAMPUNK,
    THEME_MONSTER,
    THEME_CLASSIC,
    THEME_CUSTOM,
    THEME_COUNT
};

// THEME_CUSTOM's six base colors are picked by the user at runtime (Theme
// Menu -> EDIT) rather than baked in at compile time. Each role cycles
// through this shared swatch list on-device; ThemeManager stores the
// chosen raw color per role and rebuilds THEME_CUSTOM's derived palette
// the same way makeTheme() does for every other theme.
enum CustomColorRole {
    CustomBg,
    CustomPanel,
    CustomTitle,
    CustomText,
    CustomSelected,
    CustomBorder,
    CustomRoleCount,
};

constexpr const char* kCustomRoleLabels[CustomRoleCount] = {
    "BACKGROUND", "PANEL", "TITLE", "TEXT", "SELECTED", "BORDER",
};

constexpr uint32_t kThemeSwatches[] = {
    0xFFFFFF, 0x000000, 0x808080, 0xE63946, 0xF77F00, 0xFFD60A,
    0x2ECC71, 0x2EC4B6, 0x00E5FF, 0x0077B6, 0x023E8A, 0x7B2CBF,
    0xFF5DA2, 0x7B4F2C,
};
constexpr int kThemeSwatchCount = sizeof(kThemeSwatches) / sizeof(kThemeSwatches[0]);

struct ThemeColors {
    uint16_t background;
    uint16_t surface;
    uint16_t surfaceRaised;
    uint16_t border;
    uint16_t primary;
    uint16_t accent;
    uint16_t text;
    uint16_t muted;
    uint16_t darkText;
};

struct ThemeDefinition {
    const char* displayName;
    const char* titleText;
    ThemeColors colors;
};

class ThemeManager {
public:
    void begin();

    void apply(ThemeId theme);
    void next();
    void previous();

    ThemeId current() const { return _current; }
    const ThemeColors& colors() const { return _definitions[_current].colors; }
    const char* titleText() const { return _definitions[_current].titleText; }
    const char* displayName(ThemeId theme) const { return _definitions[theme].displayName; }

    // Rebuilds THEME_CUSTOM's palette from 6 raw 0xRRGGBB colors (same
    // derivation as every other theme) and remembers them for persistence
    // and for the editor screen to resume from.
    void setCustomColors(
        uint32_t background,
        uint32_t panel,
        uint32_t title,
        uint32_t text,
        uint32_t selected,
        uint32_t border);
    uint32_t customColor(CustomColorRole role) const { return _customRaw[role]; }

private:
    void buildDefinitions();

    ThemeId _current = THEME_PINEAPPLE;
    ThemeDefinition _definitions[THEME_COUNT];
    uint32_t _customRaw[CustomRoleCount] = {};
    bool _built = false;
};

extern ThemeManager themeManager;
