#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// One-off diagnostic: the onboard tri-color RGB LED on this board is
// wired to a fixed GPIO that isn't documented in the vendor datasheet.
// Each candidate GPIO gets its own distinct color, so identifying the
// right pin only requires naming the color you saw (no need to
// synchronize with the serial log timing).
//
// GPIO22/23/24/25 don't physically exist on this ESP32-S3 (confirmed by
// "Invalid pin selected" errors when tested), so they're left out.
// GPIO19/20 (native USB D-/D+) and GPIO33-37 (Octal PSRAM bus) are
// deliberately excluded too: toggling them at runtime can drop the
// Serial connection or hang the chip.
namespace {
struct Candidate {
    int pin;
    uint8_t r, g, b;
    const char* colorName;
};

const Candidate Candidates[] = {
    {8, 255, 0, 0, "RED"},
    {16, 0, 255, 0, "GREEN"},
    {40, 0, 0, 255, "BLUE"},
    {42, 255, 255, 0, "YELLOW"},
    {43, 255, 0, 255, "MAGENTA"},
    {44, 0, 255, 255, "CYAN"},
    {45, 255, 90, 0, "ORANGE"},
    {0, 255, 255, 255, "WHITE"},
};
constexpr size_t CandidateCount =
    sizeof(Candidates) / sizeof(Candidates[0]);
constexpr uint32_t HoldMs = 2000;

Adafruit_NeoPixel* strip = nullptr;
size_t currentIndex = 0;
uint32_t lastSwitchAt = 0;
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
    Serial.println("=== RGB LED PIN FINDER (color-coded) ===");
    Serial.println("Watch the onboard RGB LED and report which color lit it up:");
    for (const Candidate& c : Candidates)
    {
        Serial.printf("  GPIO %-3d -> %s\n", c.pin, c.colorName);
    }
    Serial.println();
    lastSwitchAt = millis() - HoldMs;
}

void loop()
{
    const uint32_t now = millis();
    if (now - lastSwitchAt < HoldMs)
    {
        return;
    }

    if (strip != nullptr)
    {
        strip->setPixelColor(0, 0);
        strip->show();
        delete strip;
        strip = nullptr;
    }

    const Candidate& c = Candidates[currentIndex];
    Serial.printf("Testing GPIO %d -> %s\n", c.pin, c.colorName);

    strip = new Adafruit_NeoPixel(1, c.pin, NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setPixelColor(0, strip->Color(c.r, c.g, c.b));
    strip->show();

    currentIndex = (currentIndex + 1) % CandidateCount;
    lastSwitchAt = now;
}
