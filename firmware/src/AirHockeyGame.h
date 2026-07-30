#pragma once

#include <Arduino_GFX_Library.h>

// Single-player air hockey vs a simple CPU paddle. Same shared-class
// pattern as PinballGame: used both by the standalone "airhockey"
// firmware (AirHockeyMain.cpp) and the jukebox's Games tab.
class AirHockeyGame {
public:
    void begin();
    void setMoveUp(bool held);
    void setMoveDown(bool held);
    void update();
    void draw(Arduino_Canvas* gfx);
    bool isGameOver() const;

private:
    struct Vec2 {
        float x;
        float y;
    };
    enum class GameState {
        Playing,
        GameOver,
    };

    void resetPuck(int direction);
    void resetGame();

    Vec2 _puckPos;
    Vec2 _puckVel;
    // Base speed each new serve starts at - ratchets up after every
    // goal (see update()) so the match gets faster as it's played, on
    // top of the existing per-hit speed-up within a single rally.
    float _serveSpeed;
    float _playerY;
    float _cpuY;
    bool _moveUp;
    bool _moveDown;
    bool _moveUpWasHeld;
    bool _moveDownWasHeld;
    int _playerScore;
    int _cpuScore;
    GameState _state;
};

extern AirHockeyGame airHockeyGame;
