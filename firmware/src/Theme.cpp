#include "Theme.h"

ThemeManager themeManager;

namespace {

uint16_t rgb565(uint32_t hex888)
{
    const uint8_t r = static_cast<uint8_t>((hex888 >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex888 >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex888 & 0xFF);
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void unpack565(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = static_cast<uint8_t>((color >> 11) & 0x1F) << 3;
    g = static_cast<uint8_t>((color >> 5) & 0x3F) << 2;
    b = static_cast<uint8_t>(color & 0x1F) << 3;
}

// Blends two 565 colors; weight is how much of "b" to mix in (0-100).
uint16_t blend565(uint16_t a, uint16_t b, uint8_t weightPercentB)
{
    uint8_t ar, ag, ab, br, bg, bb;
    unpack565(a, ar, ag, ab);
    unpack565(b, br, bg, bb);
    const uint8_t r = static_cast<uint8_t>((ar * (100 - weightPercentB) + br * weightPercentB) / 100);
    const uint8_t g = static_cast<uint8_t>((ag * (100 - weightPercentB) + bg * weightPercentB) / 100);
    const uint8_t bl = static_cast<uint8_t>((ab * (100 - weightPercentB) + bb * weightPercentB) / 100);
    return rgb565((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | bl);
}

uint16_t lighten565(uint16_t color, uint8_t amount)
{
    return blend565(color, 0xFFFF, amount);
}

// Picks readable near-black or near-white text for a given background color.
uint16_t contrastText565(uint16_t background)
{
    uint8_t r, g, b;
    unpack565(background, r, g, b);
    const uint32_t luminance = (r * 299 + g * 587 + b * 114) / 1000;
    return luminance > 150 ? rgb565(0x101010) : rgb565(0xFFFFFF);
}

ThemeDefinition makeTheme(
    const char* displayName,
    const char* titleText,
    uint32_t bg,
    uint32_t panel,
    uint32_t title,
    uint32_t text,
    uint32_t selected,
    uint32_t border)
{
    ThemeDefinition def;
    def.displayName = displayName;
    def.titleText = titleText;

    const uint16_t panel565 = rgb565(panel);
    const uint16_t text565 = rgb565(text);
    const uint16_t selected565 = rgb565(selected);

    def.colors.background = rgb565(bg);
    def.colors.surface = panel565;
    def.colors.surfaceRaised = lighten565(panel565, 24);
    def.colors.border = rgb565(border);
    def.colors.primary = selected565;
    def.colors.accent = rgb565(title);
    def.colors.text = text565;
    def.colors.muted = blend565(text565, panel565, 55);
    def.colors.darkText = contrastText565(selected565);
    return def;
}

// Used for the "Classic" theme, which reproduces the app's original
// hand-tuned color scheme exactly (pre-dating the theme system) rather
// than deriving shades from 6 base colors like makeTheme() does.
ThemeDefinition makeRawTheme(
    const char* displayName,
    const char* titleText,
    const ThemeColors& colors)
{
    ThemeDefinition def;
    def.displayName = displayName;
    def.titleText = titleText;
    def.colors = colors;
    return def;
}

}  // namespace

void ThemeManager::buildDefinitions()
{
    _definitions[THEME_PINEAPPLE] = makeTheme(
        "Pineapple Jukebox", "Pineapple Jukebox",
        0xF77F00, 0x023E8A, 0xFFD447, 0xFFFFFF, 0xF7941D, 0x6A4C93);

    _definitions[THEME_RETRO_ARCADE] = makeTheme(
        "Retro Arcade", "Retro Arcade",
        0x050505, 0x1B1035, 0xFF2BD6, 0x00E5FF, 0x2979FF, 0xFFE600);

    _definitions[THEME_ROBOT] = makeTheme(
        "Cute Robot", "Robo Jukebox",
        0xC9D1D9, 0x30363D, 0x00B4D8, 0xFFFFFF, 0xFF9F1C, 0x2EC4B6);

    _definitions[THEME_SPACESHIP] = makeTheme(
        "Spaceship Cockpit", "Music Mission",
        0x050A18, 0x1A1F2E, 0x39FF14, 0xFFFFFF, 0xE63946, 0xFFD60A);

    _definitions[THEME_PIRATE] = makeTheme(
        "Pirate Treasure", "Treasure Tunes",
        0x3E2417, 0x6B3E26, 0xD4AF37, 0xE9D8A6, 0xC1121F, 0xB08D57);

    _definitions[THEME_VINTAGE_RADIO] = makeTheme(
        "Vintage Radio", "Vintage Jukebox",
        0xF5E6C8, 0x7B4F2C, 0xA23E2B, 0x2F1B12, 0xFFB703, 0xB08D57);

    _definitions[THEME_CASSETTE] = makeTheme(
        "Mix Tape", "Mix Tape",
        0x2EC4B6, 0xD9D9D9, 0xFF4D9D, 0x2B2D42, 0xFFD166, 0xFF4D9D);

    _definitions[THEME_FOREST] = makeTheme(
        "Enchanted Forest", "Forest Songs",
        0x0B3D2E, 0x3A7D44, 0xFEE440, 0xFFFFFF, 0xB388EB, 0x8B5E34);

    _definitions[THEME_STEAMPUNK] = makeTheme(
        "Steampunk", "Gearbox Jukebox",
        0x2B1B12, 0x1F1F1F, 0xD4AF37, 0xEAD7B7, 0xB87333, 0xC9A227);

    _definitions[THEME_MONSTER] = makeTheme(
        "Music Monster", "Music Monster",
        0x1ABC9C, 0x7B2CBF, 0xFFD166, 0xFFFFFF, 0xFF5DA2, 0xFF9F1C);

    _definitions[THEME_CLASSIC] = makeRawTheme(
        "Classic", "Jukebox",
        ThemeColors{
            0x0844,  // background
            0x10A8,  // surface
            0x192C,  // surfaceRaised
            0x29B0,  // border
            0x04FA,  // primary
            0x843F,  // accent
            0xF7BE,  // text
            0x8410,  // muted
            0x0844,  // darkText
        });

    // Starting point for THEME_CUSTOM before the user has edited it (or
    // before a saved custom_* setting is loaded) — reuses Pineapple's
    // palette as a known-good default rather than an arbitrary one.
    setCustomColors(0x0077B6, 0x023E8A, 0xFFD447, 0xFFFFFF, 0xF7941D, 0x6A4C93);
}

void ThemeManager::setCustomColors(
    uint32_t background,
    uint32_t panel,
    uint32_t title,
    uint32_t text,
    uint32_t selected,
    uint32_t border)
{
    _customRaw[CustomBg] = background;
    _customRaw[CustomPanel] = panel;
    _customRaw[CustomTitle] = title;
    _customRaw[CustomText] = text;
    _customRaw[CustomSelected] = selected;
    _customRaw[CustomBorder] = border;
    _definitions[THEME_CUSTOM] = makeTheme(
        "Custom", "Custom Jukebox",
        background, panel, title, text, selected, border);
}

void ThemeManager::begin()
{
    if (!_built)
    {
        buildDefinitions();
        _built = true;
    }
    _current = THEME_PINEAPPLE;
}

void ThemeManager::apply(ThemeId theme)
{
    if (theme >= 0 && theme < THEME_COUNT)
    {
        _current = theme;
    }
}

void ThemeManager::next()
{
    _current = static_cast<ThemeId>((_current + 1) % THEME_COUNT);
}

void ThemeManager::previous()
{
    _current = static_cast<ThemeId>((_current + THEME_COUNT - 1) % THEME_COUNT);
}
