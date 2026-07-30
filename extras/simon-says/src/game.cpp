#include "game.h"
#include "config.h"
#include "ui.h"
#include "touch.h"
#include <Preferences.h>

GameData game;

static Preferences prefs;

// Flash timing per difficulty: {on_ms, off_ms}
static const uint16_t FLASH_ON_MS[]  = {700, 450, 250};
static const uint16_t FLASH_OFF_MS[] = {400, 250, 150};

// ============================================================
void initGame() {
    prefs.begin("simon", false);
    game.highScore  = prefs.getUInt("hi", 0);
    game.state      = STATE_START;
    game.difficulty = DIFF_NORMAL;

    randomSeed(esp_random());
}

// ============================================================
void startNewGame() {
    game.sequenceLen  = 0;
    game.playerIndex  = 0;
    game.score        = 0;

    generateNextStep();
    game.state = STATE_PLAYING;
}

// ============================================================
void generateNextStep() {
    if (game.sequenceLen < MAX_SEQUENCE) {
        game.sequence[game.sequenceLen++] = (uint8_t)random(4);
    }
}

// ============================================================
void playSequence() {
    const uint16_t onMs  = FLASH_ON_MS[game.difficulty];
    const uint16_t offMs = FLASH_OFF_MS[game.difficulty];

    // Show the round number and "WATCH" label before starting
    drawStatusBar();
    delay(600);

    for (uint8_t i = 0; i < game.sequenceLen; i++) {
        flashButton(game.sequence[i], true);
        playToneForButton(game.sequence[i]);
        delay(onMs);
        flashButton(game.sequence[i], false);
        delay(offMs);
    }

    delay(400);
    game.playerIndex = 0;
    game.state       = STATE_PLAYER_TURN;
    drawStatusBar();  // switches label to "YOUR TURN"
}

// ============================================================
void handlePlayerInput() {
    int8_t btn = readTouchedButton();
    if (btn < 0) return;

    // Visual and audio feedback immediately on press
    flashButton((uint8_t)btn, true);
    playToneForButton((uint8_t)btn);
    delay(150);
    flashButton((uint8_t)btn, false);

    // Wait for finger lift so the same press is not counted twice
    while (isTouched()) delay(10);

    if ((uint8_t)btn == game.sequence[game.playerIndex]) {
        game.playerIndex++;

        if (game.playerIndex == game.sequenceLen) {
            // Player completed this round
            game.score = game.sequenceLen;
            playSuccessTone();
            delay(500);

            generateNextStep();
            game.state = STATE_PLAYING;
            drawStatusBar();
        }
    } else {
        // Wrong button pressed
        playErrorTone();
        delay(200);
        gameOver();
    }
}

// ============================================================
void gameOver() {
    // Score = the last successfully completed sequence length.
    // If the player failed on round 1 it stays 0 (set in startNewGame).
    if (game.highScore < game.score) {
        game.highScore = game.score;
        prefs.putUInt("hi", game.highScore);
    }

    game.state = STATE_GAME_OVER;
    drawGameOverScreen();
}

// ============================================================
//  Sound — placeholders
//  Connect a passive buzzer between BUZZER_PIN and GND, then set
//  BUZZER_ENABLED=1 in config.h and uncomment the tone() calls.
// ============================================================

void playToneForButton(uint8_t buttonIndex) {
#if BUZZER_ENABLED
    static const uint16_t freqs[] = {330, 440, 550, 660};
    // tone(BUZZER_PIN, freqs[buttonIndex], 150);
#else
    (void)buttonIndex;
#endif
}

void playErrorTone() {
#if BUZZER_ENABLED
    // tone(BUZZER_PIN, 180, 500);
#endif
}

void playSuccessTone() {
#if BUZZER_ENABLED
    // tone(BUZZER_PIN, 880, 120);
    // delay(160);
    // tone(BUZZER_PIN, 1100, 180);
#endif
}
