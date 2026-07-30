#include "PinballGame.h"

#include <esp_random.h>

PinballGame pinballGame;

namespace {
constexpr int Width = 480;
constexpr int Height = 320;

constexpr uint16_t ColorBg = 0x0844;
constexpr uint16_t ColorWall = 0x39C7;
constexpr uint16_t ColorBall = 0xFFE0;
constexpr uint16_t ColorFlipper = 0x07FF;
constexpr uint16_t ColorFlipperActive = 0xFFFF;
constexpr uint16_t ColorText = 0xF7BE;
constexpr uint16_t ColorMuted = 0x8410;
constexpr uint16_t ColorGameOver = 0xF800;

constexpr uint16_t BumperColors[3] = {0xF81F, 0xFD20, 0x07E0};

constexpr float BallRadius = 7.0f;
constexpr float Gravity = 0.35f;
constexpr float WallRestitution = 0.85f;
constexpr float FlipperRestingRestitution = 0.55f;
constexpr float FlipperActiveKick = 8.5f;
constexpr float BumperPopSpeed = 7.0f;
constexpr float MaxSpeed = 10.0f;
// Fraction of the full rest->active swing covered per update() call -
// about 3 frames to fully swing up, so the flipper visibly sweeps
// through space (and can catch a resting ball) rather than teleporting.
constexpr float FlipperSwingStep = 0.35f;

float approach(float current, float target, float step)
{
    if (current < target)
    {
        return fminf(current + step, target);
    }
    if (current > target)
    {
        return fmaxf(current - step, target);
    }
    return current;
}

constexpr int WallLeft = 8;
constexpr int WallRight = Width - 8;
constexpr int WallTop = 30;
constexpr int DrainY = Height + 12;

// Guide walls closing the outer lanes beside each flipper's pivot
// (150/330) — without these, the outer WallLeft/WallRight (8/472) are
// the only thing bouncing the ball down there, so it can fall straight
// past the flippers on either side instead of only through the
// intended gap between them.
constexpr int InnerWallY = 90;
constexpr int LeftInnerWallX = 140;
constexpr int RightInnerWallX = 340;
}  // namespace

void PinballGame::begin()
{
    _bumpers[0] = {{175, 110}, 18, BumperColors[0], 0};
    _bumpers[1] = {{305, 110}, 18, BumperColors[1], 0};
    _bumpers[2] = {{240, 170}, 20, BumperColors[2], 0};
    _leftFlipper = {{150, 285}, {205, 308}, {205, 250}};
    _rightFlipper = {{330, 285}, {275, 308}, {275, 250}};
    _leftHeld = false;
    _rightHeld = false;
    _leftWasHeld = false;
    _rightWasHeld = false;
    _leftSwing = 0.0f;
    _rightSwing = 0.0f;
    resetGame();
}

void PinballGame::setLeftFlipper(bool held)
{
    _leftWasHeld = _leftHeld;
    _leftHeld = held;
    if (_state == GameState::GameOver && held && !_leftWasHeld)
    {
        resetGame();
    }
}

void PinballGame::setRightFlipper(bool held)
{
    _rightWasHeld = _rightHeld;
    _rightHeld = held;
    if (_state == GameState::GameOver && held && !_rightWasHeld)
    {
        resetGame();
    }
}

bool PinballGame::isGameOver() const
{
    return _state == GameState::GameOver;
}

void PinballGame::resetBall()
{
    _ballPos = {Width / 2.0f + static_cast<float>(random(-40, 40)), 55};
    _ballVel = {static_cast<float>(random(-15, 15)) / 10.0f, 0};
}

void PinballGame::resetGame()
{
    _score = 0;
    _lives = 3;
    _state = GameState::Playing;
    resetBall();
}

PinballGame::Vec2 PinballGame::flipperTip(const Flipper& f, float swing)
{
    return {
        f.restTip.x + (f.activeTip.x - f.restTip.x) * swing,
        f.restTip.y + (f.activeTip.y - f.restTip.y) * swing,
    };
}

// Closest point on segment a-b to point p.
PinballGame::Vec2 PinballGame::closestOnSegment(Vec2 a, Vec2 b, Vec2 p)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lenSq = dx * dx + dy * dy;
    float t = lenSq > 0.0001f
        ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq
        : 0.0f;
    t = constrain(t, 0.0f, 1.0f);
    return {a.x + dx * t, a.y + dy * t};
}

void PinballGame::reflectOffNormal(float nx, float ny, float restitution, float extraKick)
{
    const float dot = _ballVel.x * nx + _ballVel.y * ny;
    _ballVel.x = (_ballVel.x - 2 * dot * nx) * restitution + nx * extraKick;
    _ballVel.y = (_ballVel.y - 2 * dot * ny) * restitution + ny * extraKick;
}

void PinballGame::handleFlipperCollision(const Flipper& f, float swing, bool held)
{
    // swing only positions the geometry (so the collision segment
    // sweeps smoothly instead of teleporting - see the comment on
    // _leftSwing/_rightSwing). Kick strength is tied to "held" instead
    // of swing: a ball touched during the very first frame of a press
    // (swing barely off 0) should still get the full throw, not a
    // fraction of it just because the animation hasn't caught up yet.
    const Vec2 tip = flipperTip(f, swing);
    const Vec2 closest = closestOnSegment(f.pivot, tip, _ballPos);
    const float dx = _ballPos.x - closest.x;
    const float dy = _ballPos.y - closest.y;
    const float dist = sqrtf(dx * dx + dy * dy);
    const float minDist = BallRadius + 6.0f;
    if (dist >= minDist || dist < 0.0001f)
    {
        return;
    }

    const float nx = dx / dist;
    const float ny = dy / dist;
    _ballPos.x = closest.x + nx * minDist;
    _ballPos.y = closest.y + ny * minDist;
    reflectOffNormal(
        nx,
        ny,
        held ? 1.0f : FlipperRestingRestitution,
        held ? FlipperActiveKick : 0.0f);
}

void PinballGame::update()
{
    if (_state != GameState::Playing)
    {
        return;
    }

    _leftSwing = approach(_leftSwing, _leftHeld ? 1.0f : 0.0f, FlipperSwingStep);
    _rightSwing = approach(_rightSwing, _rightHeld ? 1.0f : 0.0f, FlipperSwingStep);

    _ballVel.y += Gravity;

    const float speed = sqrtf(_ballVel.x * _ballVel.x + _ballVel.y * _ballVel.y);
    if (speed > MaxSpeed)
    {
        _ballVel.x = _ballVel.x / speed * MaxSpeed;
        _ballVel.y = _ballVel.y / speed * MaxSpeed;
    }

    _ballPos.x += _ballVel.x;
    _ballPos.y += _ballVel.y;

    if (_ballPos.x - BallRadius < WallLeft)
    {
        _ballPos.x = WallLeft + BallRadius;
        _ballVel.x = -_ballVel.x * WallRestitution;
    }
    else if (_ballPos.x + BallRadius > WallRight)
    {
        _ballPos.x = WallRight - BallRadius;
        _ballVel.x = -_ballVel.x * WallRestitution;
    }
    if (_ballPos.y - BallRadius < WallTop)
    {
        _ballPos.y = WallTop + BallRadius;
        _ballVel.y = -_ballVel.y * WallRestitution;
    }

    if (_ballPos.y > InnerWallY)
    {
        if (_ballPos.x - BallRadius < LeftInnerWallX)
        {
            _ballPos.x = LeftInnerWallX + BallRadius;
            _ballVel.x = -_ballVel.x * WallRestitution;
        }
        else if (_ballPos.x + BallRadius > RightInnerWallX)
        {
            _ballPos.x = RightInnerWallX - BallRadius;
            _ballVel.x = -_ballVel.x * WallRestitution;
        }
    }

    for (Bumper& bumper : _bumpers)
    {
        const float dx = _ballPos.x - bumper.pos.x;
        const float dy = _ballPos.y - bumper.pos.y;
        const float dist = sqrtf(dx * dx + dy * dy);
        const float minDist = BallRadius + bumper.radius;
        if (dist < minDist && dist > 0.0001f)
        {
            const float nx = dx / dist;
            const float ny = dy / dist;
            _ballPos.x = bumper.pos.x + nx * minDist;
            _ballPos.y = bumper.pos.y + ny * minDist;
            _ballVel.x = nx * BumperPopSpeed;
            _ballVel.y = ny * BumperPopSpeed;
            if (millis() > bumper.litUntil)
            {
                _score += 100;
            }
            bumper.litUntil = millis() + 200;
        }
    }

    handleFlipperCollision(_leftFlipper, _leftSwing, _leftHeld);
    handleFlipperCollision(_rightFlipper, _rightSwing, _rightHeld);

    if (_ballPos.y > DrainY)
    {
        --_lives;
        if (_lives <= 0)
        {
            _state = GameState::GameOver;
        }
        else
        {
            resetBall();
        }
    }
}

void PinballGame::drawFlipper(Arduino_Canvas* gfx, const Flipper& f, float swing)
{
    const Vec2 tip = flipperTip(f, swing);
    const uint16_t color = swing > 0.5f ? ColorFlipperActive : ColorFlipper;
    gfx->drawLine(f.pivot.x, f.pivot.y, tip.x, tip.y, color);
    gfx->drawLine(f.pivot.x, f.pivot.y - 1, tip.x, tip.y - 1, color);
    gfx->drawLine(f.pivot.x, f.pivot.y + 1, tip.x, tip.y + 1, color);
    gfx->fillCircle(f.pivot.x, f.pivot.y, 4, color);
    gfx->fillCircle(tip.x, tip.y, 3, color);
}

void PinballGame::draw(Arduino_Canvas* gfx)
{
    gfx->fillScreen(ColorBg);

    gfx->drawFastHLine(0, WallTop, Width, ColorWall);
    gfx->drawFastVLine(WallLeft, WallTop, Height - WallTop, ColorWall);
    gfx->drawFastVLine(WallRight, WallTop, Height - WallTop, ColorWall);
    gfx->drawFastVLine(LeftInnerWallX, InnerWallY, Height - InnerWallY, ColorWall);
    gfx->drawFastVLine(RightInnerWallX, InnerWallY, Height - InnerWallY, ColorWall);

    for (const Bumper& bumper : _bumpers)
    {
        const bool lit = millis() < bumper.litUntil;
        gfx->fillCircle(bumper.pos.x, bumper.pos.y, bumper.radius, lit ? 0xFFFF : bumper.color);
        gfx->drawCircle(bumper.pos.x, bumper.pos.y, bumper.radius, ColorWall);
    }

    drawFlipper(gfx, _leftFlipper, _leftSwing);
    drawFlipper(gfx, _rightFlipper, _rightSwing);

    if (_state == GameState::Playing)
    {
        gfx->fillCircle(_ballPos.x, _ballPos.y, BallRadius, ColorBall);
    }

    gfx->setTextColor(ColorText);
    gfx->setTextSize(2);
    gfx->setCursor(8, 4);
    gfx->printf("SCORE %04d", _score);

    gfx->setTextColor(ColorMuted);
    gfx->setTextSize(1);
    gfx->setCursor(Width - 70, 10);
    gfx->printf("BALLS %d", _lives);

    if (_state == GameState::GameOver)
    {
        gfx->setTextColor(ColorGameOver);
        gfx->setTextSize(3);
        gfx->setCursor(Width / 2 - 96, Height / 2 - 30);
        gfx->print("GAME OVER");
        gfx->setTextColor(ColorText);
        gfx->setTextSize(1);
        gfx->setCursor(Width / 2 - 70, Height / 2 + 10);
        gfx->print("PRESS A BUTTON TO RESTART");
    }
}
