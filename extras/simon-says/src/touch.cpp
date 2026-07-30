#include "touch.h"
#include "config.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// Touch controller uses HSPI so it does not share the bus with the display (VSPI).
static SPIClass           touchSPI(HSPI);
static XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// ============================================================
void initTouch() {
    // HSPI pin order: SCK, MISO, MOSI, SS
    touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    ts.begin(touchSPI);
    // Do NOT call ts.setRotation() here — we apply calibration ourselves.
}

// ============================================================
bool isTouched() {
    return ts.touched();
}

// ============================================================
bool readTouchPoint(int16_t &x, int16_t &y) {
    if (!ts.touched()) return false;

    TS_Point p = ts.getPoint();

#if DEBUG_TOUCH
    Serial.printf("[TOUCH] raw x=%d  y=%d  z=%d\n", p.x, p.y, p.z);
#endif

    int32_t rawX = p.x;
    int32_t rawY = p.y;

    // Optional axis swap (some CYD revisions ship with X/Y transposed)
#if TOUCH_SWAP_XY
    int32_t tmp = rawX; rawX = rawY; rawY = tmp;
#endif

    // Optional axis inversion
#if TOUCH_INVERT_X
    rawX = (TOUCH_X_MIN + TOUCH_X_MAX) - rawX;
#endif
#if TOUCH_INVERT_Y
    rawY = (TOUCH_Y_MIN + TOUCH_Y_MAX) - rawY;
#endif

    // Map ADC range → screen pixels
    x = (int16_t)map(rawX, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
    y = (int16_t)map(rawY, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);

    x = constrain(x, 0, (int16_t)(SCREEN_W - 1));
    y = constrain(y, 0, (int16_t)(SCREEN_H - 1));

    return true;
}

// ============================================================
int8_t readTouchedButton() {
    int16_t x, y;
    if (!readTouchPoint(x, y)) return -1;

    // Touches inside the status bar are ignored
    if (y < STATUS_BAR_H) return -1;

    // Which column and row the touch falls in
    const int16_t colBoundary = BTN_W + BTN_GAP / 2;               // ~160
    const int16_t rowBoundary = STATUS_BAR_H + BTN_H + BTN_GAP / 2; // ~137

    bool leftCol = (x < colBoundary);
    bool topRow  = (y < rowBoundary);

    if ( leftCol &&  topRow) return 0;  // Green
    if (!leftCol &&  topRow) return 1;  // Red
    if ( leftCol && !topRow) return 2;  // Yellow
    return 3;                           // Blue
}
