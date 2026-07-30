#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <SD_MMC.h>

#include "DisplayDriver.h"

// ── Pin assignments (same as main firmware) ────────────────────────────────
namespace {
constexpr int PlayPausePin = 46;   // idle HIGH, press LOW  (ext pull-up to 3.3V)
constexpr int NextPin      = 45;   // idle LOW,  press HIGH (ext pull-DOWN to GND)
constexpr int RgbLedPin    = 40;   // onboard WS2812

// SD_MMC 4-bit
constexpr int SD_CLK = 5, SD_CMD = 4;
constexpr int SD_D0 = 6, SD_D1 = 7, SD_D2 = 2, SD_D3 = 3;

// ── Colors (RGB565) ────────────────────────────────────────────────────────
constexpr uint16_t C_BG     = 0x0844;   // dark navy
constexpr uint16_t C_PANEL  = 0x10A8;   // raised surface
constexpr uint16_t C_TITLE  = 0x07FF;   // cyan
constexpr uint16_t C_OK     = 0x2E6B;   // green
constexpr uint16_t C_FAIL   = 0xF800;   // red
constexpr uint16_t C_TEXT   = 0xF7BE;   // warm white
constexpr uint16_t C_MUTED  = 0x8410;   // gray
constexpr uint16_t C_HILITE = 0xFFE0;   // yellow (button pressed)
constexpr uint16_t C_DIV    = 0x29B0;   // divider line

// ── Layout constants ───────────────────────────────────────────────────────
constexpr int W = 480, H = 320;
constexpr int TITLE_H = 36;
constexpr int SD_X = 0,   SD_W = 240;
constexpr int BTN_X = 240, BTN_W = 240;
constexpr int BODY_Y = TITLE_H + 2;
constexpr int BODY_H = H - BODY_Y;

// ── Runtime state ──────────────────────────────────────────────────────────
bool       sdOk           = false;
bool       songsFolderOk  = false;
String     songs[64];
int        songCount      = 0;

bool       pressed_PP     = false;
bool       pressed_NEXT   = false;
uint32_t   pressedAt_PP   = 0;
uint32_t   pressedAt_NEXT = 0;
int        last_PP        = HIGH;
int        last_NEXT      = LOW;
uint32_t   last_PP_t      = 0;
uint32_t   last_NEXT_t    = 0;

Adafruit_NeoPixel pixel(1, RgbLedPin, NEO_GRB + NEO_KHZ800);

// ── Helper: draw centered text in a rect ───────────────────────────────────
void drawTextAt(Arduino_Canvas* c, int x, int y, const char* txt,
                uint16_t fg, uint8_t size = 2)
{
    c->setTextColor(fg);
    c->setTextSize(size);
    c->setCursor(x, y);
    c->print(txt);
}
} // namespace

// ── SD scan ───────────────────────────────────────────────────────────────
static void scanSdCard()
{
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
    sdOk = SD_MMC.begin("/sd", false /*4-bit*/);
    songCount = 0;

    if (!sdOk) {
        Serial.println("[Diag] SD_MMC mount FAILED");
        return;
    }
    Serial.println("[Diag] SD_MMC mounted OK");

    songsFolderOk = SD_MMC.exists("/songs");
    if (!songsFolderOk) {
        Serial.println("[Diag] /songs folder NOT found");
        return;
    }
    Serial.println("[Diag] /songs folder found");

    File dir = SD_MMC.open("/songs");
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory() && songCount < 64) {
            String name = entry.name();
            name.toLowerCase();
            if (name.endsWith(".mp3") || name.endsWith(".wav")) {
                songs[songCount++] = entry.name();
                Serial.printf("[Diag]   %s\n", entry.name());
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("[Diag] %d song(s) found\n", songCount);
}

// ── Full redraw ────────────────────────────────────────────────────────────
static void redrawAll()
{
    Arduino_Canvas* c = displayDriver.canvas();
    c->fillScreen(C_BG);

    // Title bar
    c->fillRect(0, 0, W, TITLE_H, C_PANEL);
    drawTextAt(c, 8, 8, "JUKEBOX DIAGNOSTIC", C_TITLE, 2);

    // Divider between SD panel and button panel
    c->fillRect(BTN_X - 1, BODY_Y, 2, BODY_H, C_DIV);

    // ── SD panel (left) ──────────────────────────────────────────────────
    int y = BODY_Y + 6;
    drawTextAt(c, SD_X + 6, y, "SD CARD", C_TITLE, 2);
    y += 22;

    c->setTextSize(1);
    c->setTextColor(C_MUTED);
    c->setCursor(SD_X + 6, y);
    c->print("Status: ");
    c->setTextColor(sdOk ? C_OK : C_FAIL);
    c->print(sdOk ? "OK" : "FAIL - check soldering");
    y += 14;

    if (sdOk) {
        c->setTextColor(C_MUTED);
        c->setCursor(SD_X + 6, y);
        c->print("/songs folder: ");
        c->setTextColor(songsFolderOk ? C_OK : C_FAIL);
        c->print(songsFolderOk ? "FOUND" : "NOT FOUND");
        y += 14;

        if (songsFolderOk) {
            c->setTextColor(C_TEXT);
            c->setCursor(SD_X + 6, y);
            c->printf("Files: %d", songCount);
            y += 14;

            int shown = min(songCount, 12);
            for (int i = 0; i < shown; i++) {
                c->setTextColor(C_MUTED);
                c->setCursor(SD_X + 10, y);
                c->print("> ");
                c->setTextColor(C_TEXT);
                String name = songs[i];
                if (name.length() > 26) name = name.substring(0, 23) + "...";
                c->print(name);
                y += 13;
            }
            if (songCount > 12) {
                c->setTextColor(C_MUTED);
                c->setCursor(SD_X + 10, y);
                c->printf("  ...and %d more", songCount - 12);
            }
        }
    }

    // ── Button panel (right) ─────────────────────────────────────────────
    y = BODY_Y + 6;
    drawTextAt(c, BTN_X + 6, y, "BUTTONS", C_TITLE, 2);
    y += 28;

    // IO46 PLAY/PAUSE
    c->setTextSize(1);
    c->setTextColor(C_MUTED);
    c->setCursor(BTN_X + 6, y);
    c->print("IO46  PLAY / PAUSE");
    y += 14;

    uint16_t pp_bg  = pressed_PP ? C_OK   : C_PANEL;
    uint16_t pp_fg  = pressed_PP ? 0x0000 : C_MUTED;
    const char* pp_lbl = pressed_PP ? "  ** PRESSED **  " : "     [ IDLE ]    ";
    c->fillRoundRect(BTN_X + 10, y, BTN_W - 20, 28, 4, pp_bg);
    c->setTextColor(pp_fg);
    c->setTextSize(1);
    c->setCursor(BTN_X + 18, y + 10);
    c->print(pp_lbl);
    y += 40;

    c->setTextColor(C_MUTED);
    c->setTextSize(1);
    c->setCursor(BTN_X + 6, y);
    c->print("IO45  NEXT");
    y += 14;

    uint16_t nx_bg  = pressed_NEXT ? 0x001F : C_PANEL;   // blue when pressed
    uint16_t nx_fg  = pressed_NEXT ? 0xFFFF : C_MUTED;
    const char* nx_lbl = pressed_NEXT ? "  ** PRESSED **  " : "     [ IDLE ]    ";
    c->fillRoundRect(BTN_X + 10, y, BTN_W - 20, 28, 4, nx_bg);
    c->setTextColor(nx_fg);
    c->setTextSize(1);
    c->setCursor(BTN_X + 18, y + 10);
    c->print(nx_lbl);
    y += 44;

    // LED hint
    c->setTextColor(C_MUTED);
    c->setTextSize(1);
    c->setCursor(BTN_X + 6, y);
    c->print("LED: ");
    c->setTextColor(C_OK);
    c->print("GREEN");
    c->setTextColor(C_MUTED);
    c->print("=PP  ");
    c->setTextColor(0x001F);
    c->print("BLUE");
    c->setTextColor(C_MUTED);
    c->print("=NEXT");

    displayDriver.flush();
}

// ── Button-panel-only redraw (called every loop, avoids full flicker) ──────
static void redrawButtonPanel()
{
    Arduino_Canvas* c = displayDriver.canvas();

    // Clear button area
    c->fillRect(BTN_X + 1, BODY_Y, BTN_W - 1, BODY_H, C_BG);

    int y = BODY_Y + 6;
    drawTextAt(c, BTN_X + 6, y, "BUTTONS", C_TITLE, 2);
    y += 28;

    c->setTextSize(1);
    c->setTextColor(C_MUTED);
    c->setCursor(BTN_X + 6, y);
    c->print("IO46  PLAY / PAUSE");
    y += 14;

    uint16_t pp_bg  = pressed_PP ? C_OK   : C_PANEL;
    uint16_t pp_fg  = pressed_PP ? 0x0000 : C_MUTED;
    const char* pp_lbl = pressed_PP ? "  ** PRESSED **  " : "     [ IDLE ]    ";
    c->fillRoundRect(BTN_X + 10, y, BTN_W - 20, 28, 4, pp_bg);
    c->setTextColor(pp_fg);
    c->setCursor(BTN_X + 18, y + 10);
    c->print(pp_lbl);
    y += 40;

    c->setTextColor(C_MUTED);
    c->setCursor(BTN_X + 6, y);
    c->print("IO45  NEXT");
    y += 14;

    uint16_t nx_bg  = pressed_NEXT ? 0x001F : C_PANEL;
    uint16_t nx_fg  = pressed_NEXT ? 0xFFFF : C_MUTED;
    const char* nx_lbl = pressed_NEXT ? "  ** PRESSED **  " : "     [ IDLE ]    ";
    c->fillRoundRect(BTN_X + 10, y, BTN_W - 20, 28, 4, nx_bg);
    c->setTextColor(nx_fg);
    c->setCursor(BTN_X + 18, y + 10);
    c->print(nx_lbl);
    y += 44;

    c->setTextColor(C_MUTED);
    c->setCursor(BTN_X + 6, y);
    c->print("LED: ");
    c->setTextColor(C_OK);
    c->print("GREEN");
    c->setTextColor(C_MUTED);
    c->print("=PP  ");
    c->setTextColor(0x001F);
    c->print("BLUE");
    c->setTextColor(C_MUTED);
    c->print("=NEXT");

    displayDriver.flush();
}

// ── Button polling ─────────────────────────────────────────────────────────
static bool pollButtons()
{
    const uint32_t now = millis();
    bool changed = false;

    // PLAY/PAUSE — active LOW
    const int pp = digitalRead(PlayPausePin);
    if (pp != last_PP && now - last_PP_t >= 50) {
        last_PP_t = now;
        last_PP   = pp;
        if (pp == LOW) {
            pressed_PP   = true;
            pressedAt_PP = now;
            Serial.println("[Diag] IO46 PLAY/PAUSE pressed");
        }
        changed = true;
    }
    if (pressed_PP && now - pressedAt_PP >= 800) {
        pressed_PP = false;
        changed    = true;
    }

    // NEXT — active HIGH
    const int nx = digitalRead(NextPin);
    if (nx != last_NEXT && now - last_NEXT_t >= 50) {
        last_NEXT_t = now;
        last_NEXT   = nx;
        if (nx == HIGH) {
            pressed_NEXT   = true;
            pressedAt_NEXT = now;
            Serial.println("[Diag] IO45 NEXT pressed");
        }
        changed = true;
    }
    if (pressed_NEXT && now - pressedAt_NEXT >= 800) {
        pressed_NEXT = false;
        changed      = true;
    }

    return changed;
}

// ── LED update ─────────────────────────────────────────────────────────────
static void updateLed()
{
    if (pressed_PP) {
        pixel.setPixelColor(0, pixel.Color(0, 200, 0));   // green
    } else if (pressed_NEXT) {
        pixel.setPixelColor(0, pixel.Color(0, 0, 200));   // blue
    } else {
        pixel.setPixelColor(0, 0);
    }
    pixel.show();
}

// ══ Arduino entry points ═══════════════════════════════════════════════════

void setup()
{
    Serial.begin(115200);
    const uint32_t start = millis();
    while (!Serial && millis() - start < 2000) { delay(10); }

    Serial.println();
    Serial.println("=== JUKEBOX DIAGNOSTIC ===");

    displayDriver.begin();

    pixel.begin();
    pixel.setBrightness(60);
    pixel.clear();
    pixel.show();

    pinMode(PlayPausePin, INPUT);
    pinMode(NextPin,      INPUT);

    scanSdCard();
    redrawAll();

    Serial.println("[Diag] Ready. Press buttons to test.");
}

void loop()
{
    const bool changed = pollButtons();
    updateLed();
    if (changed) {
        redrawButtonPanel();
    }
    delay(20);
}
