#pragma once
#include <Arduino.h>

void    initTouch();
bool    isTouched();

// Reads the current touch point and maps it to screen coordinates.
// Returns true if a touch is active; x and y are in pixels.
bool    readTouchPoint(int16_t &x, int16_t &y);

// Returns 0-3 for the Simon button under the finger, or -1 for no touch / status bar.
int8_t  readTouchedButton();
