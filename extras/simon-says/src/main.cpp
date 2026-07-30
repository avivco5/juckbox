#include <Arduino.h>
#include "config.h"
#include "ui.h"
#include "touch.h"
#include "game.h"

static inline bool hitRect(int16_t tx, int16_t ty,
                            int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return (tx >= rx) && (tx < rx + rw) && (ty >= ry) && (ty < ry + rh);
}

static void waitForLift() {
    while (isTouched()) delay(10);
}

void setup() {
    Serial.begin(115200);
    initDisplay();
    initTouch();
    initGame();
    drawStartScreen();
}

void loop() {
    switch (game.state) {

        case STATE_START: {
            if (!isTouched()) break;
            int16_t x, y;
            if (!readTouchPoint(x, y)) break;
            if (hitRect(x, y, SCREEN_W/2 - 75, 148, 150, 52)) {
                waitForLift();
                game.state = STATE_DIFFICULTY;
                drawDifficultyScreen();
            }
            break;
        }

        case STATE_DIFFICULTY: {
            if (!isTouched()) break;
            int16_t x, y;
            if (!readTouchPoint(x, y)) break;

            Difficulty chosen;
            bool valid = true;

            if      (hitRect(x, y, 20, 45,  280, 50)) chosen = DIFF_EASY;
            else if (hitRect(x, y, 20, 103, 280, 50)) chosen = DIFF_NORMAL;
            else if (hitRect(x, y, 20, 161, 280, 50)) chosen = DIFF_HARD;
            else valid = false;

            if (valid) {
                game.difficulty = chosen;
                waitForLift();
                drawGameScreen();
                delay(200);
                startNewGame();
            }
            break;
        }

        case STATE_PLAYING: {
            playSequence();
            break;
        }

        case STATE_PLAYER_TURN: {
            handlePlayerInput();
            break;
        }

        case STATE_GAME_OVER: {
            if (!isTouched()) break;
            int16_t x, y;
            if (!readTouchPoint(x, y)) break;
            if (hitRect(x, y, SCREEN_W/2 - 90, 158, 180, 50)) {
                waitForLift();
                game.state = STATE_DIFFICULTY;
                drawDifficultyScreen();
            }
            break;
        }
    }

    delay(16);
}
