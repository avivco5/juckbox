#include "AirHockeyGame.h"

#include <esp_random.h>

AirHockeyGame airHockeyGame;

namespace {
constexpr int Width = 480;
constexpr int Height = 320;
constexpr int WallTop = 30;

constexpr float PaddleHeight = 60.0f;
constexpr float PaddleWidth = 10.0f;
constexpr float PaddleSpeed = 4.5f;
constexpr float CpuSpeed = 3.3f;
constexpr int PlayerX = 24;
constexpr int CpuX = Width - 24 - static_cast<int>(PaddleWidth);

constexpr float PuckRadius = 8.0f;
constexpr float PuckBaseSpeed = 3.2f;
constexpr float PuckSpeedIncrease = 1.06f;
constexpr float PuckServeSpeedIncrease = 1.15f;
constexpr float PuckMaxSpeed = 8.0f;
constexpr int WinScore = 5;
// The game felt too slow at a cold start, so every game (not just
// later rallies) begins as if this many paddle hits had already
// happened - computed from PuckSpeedIncrease rather than a hardcoded
// number so it stays correct if that constant ever changes.
constexpr int InitialSpeedHits = 15;

constexpr uint16_t ColorBg = 0x0844;
constexpr uint16_t ColorWall = 0x39C7;
constexpr uint16_t ColorPuck = 0xFFE0;
constexpr uint16_t ColorPlayerPaddle = 0x07FF;
constexpr uint16_t ColorCpuPaddle = 0xF800;
constexpr uint16_t ColorText = 0xF7BE;
constexpr uint16_t ColorMuted = 0x8410;
constexpr uint16_t ColorWin = 0x07E0;
constexpr uint16_t ColorLose = 0xF800;
}  // namespace

void AirHockeyGame::begin()
{
    _moveUp = false;
    _moveDown = false;
    _moveUpWasHeld = false;
    _moveDownWasHeld = false;
    resetGame();
}

void AirHockeyGame::setMoveUp(bool held)
{
    _moveUpWasHeld = _moveUp;
    _moveUp = held;
    if (_state == GameState::GameOver && held && !_moveUpWasHeld)
    {
        resetGame();
    }
}

void AirHockeyGame::setMoveDown(bool held)
{
    _moveDownWasHeld = _moveDown;
    _moveDown = held;
    if (_state == GameState::GameOver && held && !_moveDownWasHeld)
    {
        resetGame();
    }
}

bool AirHockeyGame::isGameOver() const
{
    return _state == GameState::GameOver;
}

void AirHockeyGame::resetPuck(int direction)
{
    _puckPos = {Width / 2.0f, Height / 2.0f};
    const float angle = static_cast<float>(random(-30, 30)) * 0.0174533f;
    _puckVel.x = cosf(angle) * _serveSpeed * static_cast<float>(direction);
    _puckVel.y = sinf(angle) * _serveSpeed;
}

void AirHockeyGame::resetGame()
{
    _playerScore = 0;
    _cpuScore = 0;
    _state = GameState::Playing;
    _playerY = Height / 2.0f;
    _cpuY = Height / 2.0f;
    _serveSpeed = PuckBaseSpeed;
    for (int i = 0; i < InitialSpeedHits; ++i)
    {
        _serveSpeed = fminf(_serveSpeed * PuckSpeedIncrease, PuckMaxSpeed);
    }
    resetPuck(random(0, 2) == 0 ? 1 : -1);
}

void AirHockeyGame::update()
{
    if (_state != GameState::Playing)
    {
        return;
    }

    if (_moveUp)
    {
        _playerY -= PaddleSpeed;
    }
    if (_moveDown)
    {
        _playerY += PaddleSpeed;
    }
    _playerY = constrain(_playerY, WallTop + PaddleHeight / 2.0f, static_cast<float>(Height) - PaddleHeight / 2.0f);

    if (_cpuY < _puckPos.y - 4.0f)
    {
        _cpuY += CpuSpeed;
    }
    else if (_cpuY > _puckPos.y + 4.0f)
    {
        _cpuY -= CpuSpeed;
    }
    _cpuY = constrain(_cpuY, WallTop + PaddleHeight / 2.0f, static_cast<float>(Height) - PaddleHeight / 2.0f);

    _puckPos.x += _puckVel.x;
    _puckPos.y += _puckVel.y;

    if (_puckPos.y - PuckRadius < WallTop)
    {
        _puckPos.y = WallTop + PuckRadius;
        _puckVel.y = -_puckVel.y;
    }
    else if (_puckPos.y + PuckRadius > Height)
    {
        _puckPos.y = Height - PuckRadius;
        _puckVel.y = -_puckVel.y;
    }

    if (_puckVel.x < 0 &&
        _puckPos.x - PuckRadius < PlayerX + PaddleWidth &&
        _puckPos.x - PuckRadius > PlayerX &&
        fabsf(_puckPos.y - _playerY) < PaddleHeight / 2.0f + PuckRadius)
    {
        _puckPos.x = PlayerX + PaddleWidth + PuckRadius;
        const float offset = constrain((_puckPos.y - _playerY) / (PaddleHeight / 2.0f), -1.0f, 1.0f);
        float speed = sqrtf(_puckVel.x * _puckVel.x + _puckVel.y * _puckVel.y) * PuckSpeedIncrease;
        speed = fminf(speed, PuckMaxSpeed);
        // Normalize direction before scaling by speed, so the puck's
        // resulting velocity magnitude always equals the (increased)
        // speed exactly - fixed 0.85/0.8 factors on raw components
        // didn't preserve magnitude, so a center-paddle hit was
        // quietly *slower* than before the hit despite the "increase".
        const float dirX = 1.0f;
        const float dirY = offset * 1.2f;
        const float dirLen = sqrtf(dirX * dirX + dirY * dirY);
        _puckVel.x = (dirX / dirLen) * speed;
        _puckVel.y = (dirY / dirLen) * speed;
    }
    else if (_puckVel.x > 0 &&
        _puckPos.x + PuckRadius > CpuX &&
        _puckPos.x + PuckRadius < CpuX + PaddleWidth &&
        fabsf(_puckPos.y - _cpuY) < PaddleHeight / 2.0f + PuckRadius)
    {
        _puckPos.x = CpuX - PuckRadius;
        const float offset = constrain((_puckPos.y - _cpuY) / (PaddleHeight / 2.0f), -1.0f, 1.0f);
        float speed = sqrtf(_puckVel.x * _puckVel.x + _puckVel.y * _puckVel.y) * PuckSpeedIncrease;
        speed = fminf(speed, PuckMaxSpeed);
        const float dirX = -1.0f;
        const float dirY = offset * 1.2f;
        const float dirLen = sqrtf(dirX * dirX + dirY * dirY);
        _puckVel.x = (dirX / dirLen) * speed;
        _puckVel.y = (dirY / dirLen) * speed;
    }

    if (_puckPos.x < 0)
    {
        ++_cpuScore;
        _serveSpeed = fminf(_serveSpeed * PuckServeSpeedIncrease, PuckMaxSpeed);
        if (_cpuScore >= WinScore)
        {
            _state = GameState::GameOver;
        }
        else
        {
            resetPuck(1);
        }
    }
    else if (_puckPos.x > Width)
    {
        ++_playerScore;
        _serveSpeed = fminf(_serveSpeed * PuckServeSpeedIncrease, PuckMaxSpeed);
        if (_playerScore >= WinScore)
        {
            _state = GameState::GameOver;
        }
        else
        {
            resetPuck(-1);
        }
    }
}

void AirHockeyGame::draw(Arduino_Canvas* gfx)
{
    gfx->fillScreen(ColorBg);
    gfx->drawFastHLine(0, WallTop, Width, ColorWall);
    for (int y = WallTop + 6; y < Height; y += 16)
    {
        gfx->drawFastVLine(Width / 2, y, 8, ColorWall);
    }

    gfx->fillRoundRect(PlayerX, static_cast<int>(_playerY - PaddleHeight / 2.0f), static_cast<int>(PaddleWidth), static_cast<int>(PaddleHeight), 4, ColorPlayerPaddle);
    gfx->fillRoundRect(CpuX, static_cast<int>(_cpuY - PaddleHeight / 2.0f), static_cast<int>(PaddleWidth), static_cast<int>(PaddleHeight), 4, ColorCpuPaddle);

    if (_state == GameState::Playing)
    {
        gfx->fillCircle(_puckPos.x, _puckPos.y, PuckRadius, ColorPuck);
    }

    gfx->setTextColor(ColorText);
    gfx->setTextSize(2);
    gfx->setCursor(Width / 2 - 60, 4);
    gfx->printf("%d", _playerScore);
    gfx->setCursor(Width / 2 + 40, 4);
    gfx->printf("%d", _cpuScore);

    gfx->setTextColor(ColorMuted);
    gfx->setTextSize(1);
    gfx->setCursor(8, 10);
    gfx->print("YOU");
    gfx->setCursor(Width - 34, 10);
    gfx->print("CPU");

    if (_state == GameState::GameOver)
    {
        const bool won = _playerScore > _cpuScore;
        gfx->setTextColor(won ? ColorWin : ColorLose);
        gfx->setTextSize(3);
        gfx->setCursor(Width / 2 - (won ? 78 : 90), Height / 2 - 30);
        gfx->print(won ? "YOU WIN" : "YOU LOSE");
        gfx->setTextColor(ColorText);
        gfx->setTextSize(1);
        gfx->setCursor(Width / 2 - 70, Height / 2 + 10);
        gfx->print("PRESS A BUTTON TO RESTART");
    }
}
