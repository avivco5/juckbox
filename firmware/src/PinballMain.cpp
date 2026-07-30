// Standalone pinball firmware (pio run -e pinball -t upload) — flash
// this instead of the jukebox when you want to play, then re-flash
// [env:esp32s3] afterward. All game logic lives in PinballGame, shared
// with the jukebox's Games tab; this file is just the entry point.
//
// Controls: the same two physical buttons wired for the jukebox.
//   IO46 (PLAY/PAUSE, idle HIGH / press LOW) -> LEFT flipper
//   IO45 (NEXT,       idle HIGH / press LOW) -> RIGHT flipper
//   (IO45 is wired the same way as IO46 on this board - pull-up,
//   button to GND - not the pull-down scheme its boot-strap role
//   would normally call for. See the comment in main.cpp.)
#include <Arduino.h>
#include <esp_random.h>

#include "DisplayDriver.h"
#include "PinballGame.h"

namespace {
constexpr int LeftButtonPin = 46;
constexpr int RightButtonPin = 45;
}  // namespace

void setup()
{
    Serial.begin(115200);
    const uint32_t start = millis();
    while (!Serial && millis() - start < 1500)
    {
        delay(10);
    }
    Serial.println();
    Serial.println("=== PINBALL ===");

    randomSeed(esp_random());
    pinMode(LeftButtonPin, INPUT);
    pinMode(RightButtonPin, INPUT);

    if (!displayDriver.begin())
    {
        Serial.println("ERROR: display initialization failed");
        while (true)
        {
            delay(1000);
        }
    }

    pinballGame.begin();
}

void loop()
{
    pinballGame.setLeftFlipper(digitalRead(LeftButtonPin) == LOW);
    pinballGame.setRightFlipper(digitalRead(RightButtonPin) == LOW);
    pinballGame.update();
    pinballGame.draw(displayDriver.canvas());
    displayDriver.flush();
    delay(20);
}
