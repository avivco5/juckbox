#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  Enumerations
// ============================================================

enum GameState : uint8_t {
    STATE_START,        // title / start screen
    STATE_DIFFICULTY,   // difficulty picker
    STATE_PLAYING,      // CPU is showing the sequence
    STATE_PLAYER_TURN,  // waiting for the player to repeat it
    STATE_GAME_OVER     // mistake made; show score
};

enum Difficulty : uint8_t {
    DIFF_EASY   = 0,
    DIFF_NORMAL = 1,
    DIFF_HARD   = 2
};

// ============================================================
//  Game data
// ============================================================

struct GameData {
    GameState  state;
    Difficulty difficulty;

    uint8_t  sequence[MAX_SEQUENCE];  // button indices 0-3
    uint8_t  sequenceLen;             // how many steps are in the current round
    uint8_t  playerIndex;             // which step the player is on

    uint32_t score;                   // rounds successfully completed
    uint32_t highScore;               // NVS-persisted high score
};

extern GameData game;

// ============================================================
//  Game functions
// ============================================================

void initGame();            // call once in setup(); loads high score from NVS
void startNewGame();        // reset counters, pick first step, enter STATE_PLAYING
void generateNextStep();    // append a random button to the sequence
void playSequence();        // flash the current sequence; blocks until done; then enters STATE_PLAYER_TURN
void handlePlayerInput();   // check for one touch; advance or call gameOver()
void gameOver();            // save high score, draw game-over screen, enter STATE_GAME_OVER

// ============================================================
//  Sound placeholders — implement in game.cpp once hardware is ready
// ============================================================

void playToneForButton(uint8_t buttonIndex);
void playErrorTone();
void playSuccessTone();
