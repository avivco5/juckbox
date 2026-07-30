#include "ui.h"
#include "config.h"
#include "game.h"   // for GameData game (state, score, highScore, sequenceLen)

TFT_eSPI tft = TFT_eSPI();

// ============================================================
//  Button geometry
//  Indices: 0=TopLeft(Green), 1=TopRight(Red),
//           2=BottomLeft(Yellow), 3=BottomRight(Blue)
// ============================================================

static const int16_t BTN_X[4] = {
    0,                    // 0 top-left
    BTN_W + BTN_GAP,      // 1 top-right
    0,                    // 2 bottom-left
    BTN_W + BTN_GAP       // 3 bottom-right
};

static const int16_t BTN_Y[4] = {
    STATUS_BAR_H,                        // 0, 1 top row
    STATUS_BAR_H,
    STATUS_BAR_H + BTN_H + BTN_GAP,     // 2, 3 bottom row
    STATUS_BAR_H + BTN_H + BTN_GAP
};

static const char* BTN_LABEL[4] = {"GREEN", "RED", "YELLOW", "BLUE"};

// Colors computed in initDisplay()
static uint16_t COLOR_LIT[4];
static uint16_t COLOR_DIM[4];

// ============================================================
void initDisplay() {
    tft.init();
    tft.setRotation(SCREEN_ROTATION);
    tft.fillScreen(TFT_BLACK);

    // Backlight on (also controlled by TFT_eSPI if TFT_BL is defined in User_Setup.h,
    // but being explicit here ensures it works on every CYD revision)
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Lit (bright) button colours
    COLOR_LIT[0] = tft.color565(0,   230,   0);   // green
    COLOR_LIT[1] = tft.color565(230,   0,   0);   // red
    COLOR_LIT[2] = tft.color565(230, 230,   0);   // yellow
    COLOR_LIT[3] = tft.color565(0,     0, 230);   // blue

    // Dim (dark) button colours — about 1/5 brightness
    COLOR_DIM[0] = tft.color565(0,    55,   0);
    COLOR_DIM[1] = tft.color565(55,    0,   0);
    COLOR_DIM[2] = tft.color565(55,   55,   0);
    COLOR_DIM[3] = tft.color565(0,     0,  55);
}

// ============================================================
void drawButton(uint8_t idx, bool lit) {
    if (idx > 3) return;

    uint16_t color = lit ? COLOR_LIT[idx] : COLOR_DIM[idx];
    int16_t  x     = BTN_X[idx];
    int16_t  y     = BTN_Y[idx];

    tft.fillRect(x, y, BTN_W, BTN_H, color);

    // Subtle border so buttons are visually separated from the background
    tft.drawRect(x, y, BTN_W, BTN_H, TFT_BLACK);

    tft.setTextColor(lit ? TFT_BLACK : TFT_WHITE, color);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(BTN_LABEL[idx], x + BTN_W / 2, y + BTN_H / 2);
}

void flashButton(uint8_t buttonIndex, bool lit) {
    drawButton(buttonIndex, lit);
}

// ============================================================
void drawGameScreen() {
    tft.fillScreen(TFT_BLACK);
    for (uint8_t i = 0; i < 4; i++) drawButton(i, false);
    drawStatusBar();
}

// ============================================================
void drawStatusBar() {
    tft.fillRect(0, 0, SCREEN_W, STATUS_BAR_H, tft.color565(40, 40, 40));

    // Left: round counter
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Round " + String(game.sequenceLen), 6, STATUS_BAR_H / 2);

    // Centre: game-phase label
    const char* phase = "";
    if (game.state == STATE_PLAYING)     phase = "WATCH";
    if (game.state == STATE_PLAYER_TURN) phase = "YOUR TURN";
    tft.setTextDatum(MC_DATUM);
    tft.drawString(phase, SCREEN_W / 2, STATUS_BAR_H / 2);

    // Right: high score
    tft.setTextDatum(MR_DATUM);
    tft.drawString("Best " + String(game.highScore), SCREEN_W - 6, STATUS_BAR_H / 2);
}

// ============================================================
void drawStartScreen() {
    tft.fillScreen(TFT_BLACK);

    // Title
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(5);
    tft.drawString("SIMON", SCREEN_W / 2, 52);

    // Decorative colour dots
    int16_t cy    = 105;
    int16_t space = 44;
    int16_t cx    = SCREEN_W / 2 - space * 3 / 2;
    tft.fillCircle(cx,           cy, 16, COLOR_LIT[0]);  // green
    tft.fillCircle(cx + space,   cy, 16, COLOR_LIT[1]);  // red
    tft.fillCircle(cx + space*2, cy, 16, COLOR_LIT[2]);  // yellow
    tft.fillCircle(cx + space*3, cy, 16, COLOR_LIT[3]);  // blue

    // START button
    tft.fillRoundRect(SCREEN_W/2 - 75, 148, 150, 52, 10, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("START", SCREEN_W / 2, 174);

    // High score
    tft.setTextColor(tft.color565(160, 160, 160));
    tft.setTextSize(1);
    tft.drawString("High Score: " + String(game.highScore), SCREEN_W / 2, 218);
}

// ============================================================
void drawDifficultyScreen() {
    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("SELECT DIFFICULTY", SCREEN_W / 2, 22);

    // Easy  —  y 45..95
    tft.fillRoundRect(20, 45, 280, 50, 8, tft.color565(0, 80, 0));
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("EASY", SCREEN_W / 2, 70);

    // Normal  —  y 103..153
    tft.fillRoundRect(20, 103, 280, 50, 8, tft.color565(130, 80, 0));
    tft.drawString("NORMAL", SCREEN_W / 2, 128);

    // Hard  —  y 161..211
    tft.fillRoundRect(20, 161, 280, 50, 8, tft.color565(120, 0, 0));
    tft.drawString("HARD", SCREEN_W / 2, 186);
}

// ============================================================
void drawGameOverScreen() {
    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(3);
    tft.drawString("GAME OVER", SCREEN_W / 2, 48);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("Score: " + String(game.score),     SCREEN_W / 2, 100);
    tft.drawString("Best:  " + String(game.highScore), SCREEN_W / 2, 130);

    // PLAY AGAIN button  —  y 158..208
    tft.fillRoundRect(SCREEN_W/2 - 90, 158, 180, 50, 10, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("PLAY AGAIN", SCREEN_W / 2, 183);
}
