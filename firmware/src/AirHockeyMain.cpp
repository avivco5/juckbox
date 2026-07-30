// Standalone air hockey firmware (pio run -e airhockey -t upload) —
// flash instead of the jukebox to play, then re-flash [env:esp32s3]
// afterward. All game logic lives in AirHockeyGame, shared with the
// jukebox's Games tab; this file is just the entry point.
//
// Controls: the same two physical buttons wired for the jukebox.
//   IO46 (PLAY/PAUSE, idle HIGH / press LOW) -> paddle up
//   IO45 (NEXT,       idle HIGH / press LOW) -> paddle down
//   (IO45 is wired the same way as IO46 on this board - pull-up,
//   button to GND - not the pull-down scheme its boot-strap role
//   would normally call for. See the comment in main.cpp.)
#include <Arduino.h>
#include <esp_random.h>

#include "AirHockeyGame.h"
#include "DisplayDriver.h"

namespace {
constexpr int UpButtonPin = 46;
constexpr int DownButtonPin = 45;
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
    Serial.println("=== AIR HOCKEY ===");

    randomSeed(esp_random());
    pinMode(UpButtonPin, INPUT);
    pinMode(DownButtonPin, INPUT);

    if (!displayDriver.begin())
    {
        Serial.println("ERROR: display initialization failed");
        while (true)
        {
            delay(1000);
        }
    }

    airHockeyGame.begin();
}

void loop()
{
    airHockeyGame.setMoveUp(digitalRead(UpButtonPin) == LOW);
    airHockeyGame.setMoveDown(digitalRead(DownButtonPin) == LOW);
    airHockeyGame.update();
    airHockeyGame.draw(displayDriver.canvas());
    displayDriver.flush();
    delay(20);
}
