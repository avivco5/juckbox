#include <Arduino.h>

#include "AudioController.h"
#include "DisplayDriver.h"
#include "JukeboxUI.h"
#include "SongLibrary.h"
#include "StatusLed.h"
#include "TouchController.h"

namespace {
uint32_t lastTouchAction = 0;
TouchPoint lastTouchPoint;
bool haveLastTouchPoint = false;

// External PLAY/PAUSE button, wired to the board's "Expansion Input Pins"
// connector (silkscreen IO45/IO46). IO46 is used instead of IO45 because
// IO45 is sampled at boot to select the flash voltage; loading it
// externally risks a boot failure. IO46 is input-only and may have no
// usable internal pull-up, so it needs an EXTERNAL pull-up resistor
// (~10k) from IO46 to 3.3V, with the button between IO46 and GND.
constexpr int PlayPauseButtonPin = 46;
constexpr uint32_t ButtonDebounceMs = 50;
int lastButtonState = HIGH;
uint32_t lastButtonChangeAt = 0;

// External NEXT button, wired to the same "Expansion Input Pins"
// connector's other pin (IO45). On this board it's wired the same way
// as the PLAY/PAUSE button (pull-up to 3.3V, button to GND) rather
// than the pull-down scheme originally documented here, so it reads
// idle = HIGH, press = LOW - the opposite of what the pull-down
// wiring would give.
//
// IMPORTANT: IO45 is sampled at boot to select flash voltage and is
// supposed to idle LOW to avoid loading that strap. Wiring it with a
// pull-up (idle HIGH) as done here is a real boot-reliability risk
// that this software-side inversion does NOT fix - the boot-time
// sample happens before any of this code runs. If boot is flaky,
// rewire IO45 with a pull-down (button to 3.3V, resistor to GND)
// instead of inverting the logic here.
constexpr int NextButtonPin = 45;
int lastNextButtonState = HIGH;
uint32_t lastNextButtonChangeAt = 0;
}

void setup()
{
    Serial.begin(115200);
    const uint32_t start = millis();
    while (!Serial && millis() - start < 2500)
    {
        delay(10);
    }

    Serial.println();
    Serial.println("=== JUKEBOX TOUCH UI ===");
    Serial.printf(
        "PSRAM: %u bytes\n",
        static_cast<unsigned>(ESP.getPsramSize()));

    if (!displayDriver.begin())
    {
        Serial.println("ERROR: display initialization failed");
        while (true)
        {
            delay(1000);
        }
    }

    touchController.begin();
    songLibrary.begin();
    audioController.begin();
    jukeboxUI.begin();
    statusLed.begin();

    pinMode(PlayPauseButtonPin, INPUT);
    pinMode(NextButtonPin, INPUT);

    Serial.println("Jukebox UI ready");
}

void loop()
{
    TouchPoint point;
    const bool touchDown = touchController.read(point);
    const uint32_t now = millis();

    if (touchDown)
    {
        const int movement =
            haveLastTouchPoint
                ? abs(static_cast<int>(point.x) -
                      static_cast<int>(lastTouchPoint.x)) +
                      abs(static_cast<int>(point.y) -
                          static_cast<int>(lastTouchPoint.y))
                : 1000;

        // Some CST9217/TDDI firmware revisions keep returning the previous
        // coordinate after release. Re-arm by time, and immediately accept a
        // clearly different coordinate. This keeps repeated taps functional
        // even when the release flag or INT line is latched.
        const bool cooldownElapsed = now - lastTouchAction >= 450;
        const bool movedToAnotherControl =
            movement >= 28 &&
            now - lastTouchAction >= 180;

        if (!haveLastTouchPoint ||
            cooldownElapsed ||
            movedToAnotherControl)
        {
            lastTouchAction = now;
            lastTouchPoint = point;
            haveLastTouchPoint = true;
            jukeboxUI.handleTouch(point);
            touchController.rearm();
            // The reset removes the current touch event. Give the finger time
            // to leave the glass before accepting the next event.
            lastTouchAction = millis();
        }
    }

    // While a game is running, these two buttons become game controls
    // instead of playback controls - both press AND release edges
    // matter there (e.g. a flipper needs to know when it's let go),
    // unlike the one-shot playback actions below which only fire on
    // press. JukeboxUI dispatches to whichever game is active.
    const int buttonState = digitalRead(PlayPauseButtonPin);
    if (buttonState != lastButtonState &&
        now - lastButtonChangeAt >= ButtonDebounceMs)
    {
        lastButtonChangeAt = now;
        lastButtonState = buttonState;
        const bool pressed = buttonState == LOW;
        if (jukeboxUI.isPlayingGame())
        {
            jukeboxUI.setButtonA(pressed);
        }
        else if (pressed)
        {
            Serial.println("[Button] PLAY/PAUSE pressed");
            jukeboxUI.togglePlayPause();
        }
    }

    const int nextButtonState = digitalRead(NextButtonPin);
    if (nextButtonState != lastNextButtonState &&
        now - lastNextButtonChangeAt >= ButtonDebounceMs)
    {
        lastNextButtonChangeAt = now;
        lastNextButtonState = nextButtonState;
        const bool pressed = nextButtonState == LOW;
        if (jukeboxUI.isPlayingGame())
        {
            jukeboxUI.setButtonB(pressed);
        }
        else if (pressed)
        {
            Serial.println("[Button] NEXT pressed");
            jukeboxUI.nextSong();
        }
    }

    audioController.update();
    jukeboxUI.update();
    statusLed.update();
    delay(12);
}
