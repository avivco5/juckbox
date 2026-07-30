#pragma once

#include <Arduino_GFX_Library.h>

// Simple fixed-timestep 2D pinball simulation, reused by both the
// standalone "pinball" firmware (PinballMain.cpp) and the jukebox's
// Games tab (JukeboxUI) so the physics/rendering exist in exactly one
// place rather than being duplicated between the two entry points.
class PinballGame {
public:
    void begin();
    void setLeftFlipper(bool held);
    void setRightFlipper(bool held);
    void update();
    void draw(Arduino_Canvas* gfx);
    bool isGameOver() const;

private:
    struct Vec2 {
        float x;
        float y;
    };
    struct Bumper {
        Vec2 pos;
        float radius;
        uint16_t color;
        uint32_t litUntil;
    };
    struct Flipper {
        Vec2 pivot;
        Vec2 restTip;
        Vec2 activeTip;
    };
    enum class GameState {
        Playing,
        GameOver,
    };

    void resetBall();
    void resetGame();
    static Vec2 flipperTip(const Flipper& f, float swing);
    static Vec2 closestOnSegment(Vec2 a, Vec2 b, Vec2 p);
    void reflectOffNormal(float nx, float ny, float restitution, float extraKick);
    void handleFlipperCollision(const Flipper& f, float swing, bool held);
    void drawFlipper(Arduino_Canvas* gfx, const Flipper& f, float swing);

    Vec2 _ballPos;
    Vec2 _ballVel;
    int _lives;
    int _score;
    GameState _state;
    bool _leftHeld;
    bool _rightHeld;
    bool _leftWasHeld;
    bool _rightWasHeld;
    // 0 = fully resting, 1 = fully swung up. Animated toward _leftHeld/
    // _rightHeld each frame rather than snapping instantly, so the
    // flipper's collision segment actually sweeps through the space
    // between rest and active - a ball sitting at rest position gets
    // caught by that sweep instead of being left behind when the
    // flipper "teleports" to its active position.
    float _leftSwing;
    float _rightSwing;
    Bumper _bumpers[3];
    Flipper _leftFlipper;
    Flipper _rightFlipper;
};

extern PinballGame pinballGame;
