#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// ---- Lifecycle ----
void initDisplay();

// ---- Screens ----
void drawStartScreen();
void drawDifficultyScreen();
void drawGameScreen();
void drawStatusBar();
void drawGameOverScreen();

// ---- In-game drawing ----
void drawButton(uint8_t buttonIndex, bool lit);
void flashButton(uint8_t buttonIndex, bool lit);  // alias kept for clarity in game.cpp
