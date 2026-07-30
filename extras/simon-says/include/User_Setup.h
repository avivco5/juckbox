// TFT_eSPI driver configuration for ESP32-2432S028 (Cheap Yellow Display / CYD)
// Place this file in the project's include/ folder.
// PlatformIO adds include/ before library paths, so this shadows the library's own
// User_Setup.h and prevents the need to patch the library folder.

#define USER_SETUP_LOADED  // tells TFT_eSPI to skip its own User_Setup_Select.h

// ---- Display driver ----
#define ILI9341_DRIVER

// ---- SPI pins (VSPI bus, remapped for CYD) ----
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // tied to 3.3 V on most CYD boards

// ---- Backlight ----
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// ---- Fonts to compile in ----
#define LOAD_GLCD    // Adafruit GLCD (tiny, always useful)
#define LOAD_FONT2   // small proportional
#define LOAD_FONT4   // medium proportional
#define LOAD_FONT6   // large digits
#define LOAD_FONT7   // 7-segment style
#define LOAD_FONT8   // large digits
#define LOAD_GFXFF   // FreeFont support
#define SMOOTH_FONT  // anti-aliased FreeFont rendering

// ---- SPI clock speeds ----
#define SPI_FREQUENCY        55000000   // display write
#define SPI_READ_FREQUENCY   20000000   // display read
#define SPI_TOUCH_FREQUENCY   2500000   // touch (not used by TFT_eSPI; we drive touch separately)
