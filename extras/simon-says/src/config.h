#pragma once

// ============================================================
//  Hardware pin assignments — change here for different CYD revisions
// ============================================================

// Display SPI (VSPI bus, pins remapped via TFT_eSPI)
#define TFT_MISO_PIN   12
#define TFT_MOSI_PIN   13
#define TFT_SCLK_PIN   14
#define TFT_CS_PIN     15
#define TFT_DC_PIN      2
#define TFT_RST_PIN    -1
#define TFT_BL_PIN     21   // backlight; HIGH = on

// Touch SPI (HSPI bus, separate from display)
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25

// Optional buzzer — connect a passive buzzer between this pin and GND
#define BUZZER_PIN     26

// ============================================================
//  Screen geometry (landscape)
// ============================================================
#define SCREEN_W        320
#define SCREEN_H        240
#define SCREEN_ROTATION   2   // 2 works for CYD boards with swapped GRAM wiring

// ============================================================
//  UI layout constants
// ============================================================
#define STATUS_BAR_H   35                               // pixels tall
#define BTN_GAP         4                               // gap between the four buttons
#define BTN_W         ((SCREEN_W - BTN_GAP) / 2)       // ~158 px
#define BTN_H         ((SCREEN_H - STATUS_BAR_H - BTN_GAP) / 2)  // ~100 px

// ============================================================
//  Touch calibration
//  Enable DEBUG_TOUCH=1 to print raw XPT2046 values on Serial,
//  then touch each corner and note the numbers.
// ============================================================
#define DEBUG_TOUCH     0   // 1 = print raw values; 0 = silent

// Raw 12-bit ADC range from XPT2046.
// These defaults work for many CYD units; adjust if buttons register wrong positions.
#define TOUCH_X_MIN   300
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3800

// Set to 1 if an axis reads backwards (touch drifts to wrong side)
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

// Set to 1 if X and Y raw axes are swapped for your board revision
#define TOUCH_SWAP_XY   0

// ============================================================
//  Game limits
// ============================================================
#define MAX_SEQUENCE   100  // maximum Simon sequence length

// ============================================================
//  Feature flags
// ============================================================
#define BUZZER_ENABLED  0   // set to 1 once a passive buzzer is wired to BUZZER_PIN
