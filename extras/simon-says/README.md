# Simon Says — ESP32-2432S028 (CYD)

A complete Simon Says memory game for the Cheap Yellow Display (ESP32-2432S028).

---

## Hardware target

| Item | Value |
|---|---|
| Board | ESP32-2432S028 ("Cheap Yellow Display") |
| MCU | ESP32 |
| Display | 2.8 inch ILI9341, 320 × 240, landscape |
| Touch | XPT2046 resistive, separate SPI bus |
| Framework | Arduino (PlatformIO) |

---

## Project structure

```
SimonSays/
├── platformio.ini          project / build config
├── include/
│   └── User_Setup.h        TFT_eSPI pin config (keeps library folder clean)
├── src/
│   ├── config.h            all pins, calibration, layout constants
│   ├── main.cpp            setup(), loop(), state machine
│   ├── game.h / game.cpp   sequence logic, scoring, NVS high score, sound stubs
│   ├── ui.h  / ui.cpp      all drawing: screens, buttons, status bar
│   └── touch.h / touch.cpp XPT2046 init, coordinate mapping, button detection
└── README.md
```

---

## Required libraries

PlatformIO fetches these automatically from `lib_deps` in `platformio.ini`:

| Library | Author | Purpose |
|---|---|---|
| TFT_eSPI | Bodmer | ILI9341 display driver |
| XPT2046_Touchscreen | Paul Stoffregen | Resistive touch controller |
| Preferences | ESP32 Arduino core | NVS high-score storage (built-in, no entry needed) |

---

## How to upload with PlatformIO

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
2. Open the `SimonSays/` folder in VS Code (`File → Open Folder`).
3. Connect the CYD via USB.
4. Click **Upload** (→ arrow in the PlatformIO toolbar) or run:
   ```
   pio run --target upload
   ```
5. Open **Serial Monitor** at 115200 baud if you need to read debug output.

---

## Game controls

| Element | Action |
|---|---|
| START button (title screen) | Go to difficulty picker |
| EASY / NORMAL / HARD | Start game at that speed |
| Four large coloured buttons | Repeat the sequence |
| PLAY AGAIN (game-over screen) | Return to difficulty picker |

Difficulty flash speeds:

| Difficulty | Button lit | Gap between steps |
|---|---|---|
| Easy | 700 ms | 400 ms |
| Normal | 450 ms | 250 ms |
| Hard | 250 ms | 150 ms |

---

## Touch calibration

### Quick calibration procedure

1. Open `src/config.h` and set `DEBUG_TOUCH 1`.
2. Upload and open Serial Monitor (115 200 baud).
3. Touch the **four corners** of the screen one at a time; note the raw `x` and `y` values printed.
4. Update the four constants:
   ```c
   #define TOUCH_X_MIN   <left-edge raw value>
   #define TOUCH_X_MAX   <right-edge raw value>
   #define TOUCH_Y_MIN   <top-edge raw value>
   #define TOUCH_Y_MAX   <bottom-edge raw value>
   ```
5. If touching the left side registers as right (or vice versa), set `TOUCH_INVERT_X 1`.
6. If touching the top registers as bottom (or vice versa), set `TOUCH_INVERT_Y 1`.
7. If X and Y raw values are completely swapped for your revision, set `TOUCH_SWAP_XY 1`.
8. Set `DEBUG_TOUCH 0` when done.

---

## Adding a buzzer (optional)

1. Connect a **passive buzzer** between **GPIO 26** and **GND** (GPIO 26 is the DAC/`tone()` capable pin on CYD).
2. Set `BUZZER_ENABLED 1` in `src/config.h`.
3. Uncomment the `tone()` lines inside `playToneForButton()`, `playErrorTone()`, and `playSuccessTone()` in `src/game.cpp`.

---

## Common issues and fixes

### White screen / blank screen

- **Cause**: TFT_eSPI is using wrong pins.
- **Fix**: Confirm `include/User_Setup.h` is present in the `include/` folder and that PlatformIO finds it. Run `pio run -v` and look for the include path; `include/` must appear before the TFT_eSPI library path.
- **Also check**: `ILI9341_DRIVER` is defined and no other driver is defined in `User_Setup.h`.

### Screen mirrored or rotated wrong

- **Fix**: Change `SCREEN_ROTATION` in `src/config.h`.
  - `1` = landscape, USB on the left
  - `3` = landscape, USB on the right
  - `0` = portrait
  - `2` = portrait flipped

### Touch coordinates are completely wrong or reversed

- Enable `DEBUG_TOUCH 1` and check the raw values.
- Adjust `TOUCH_X_MIN/MAX` and `TOUCH_Y_MIN/MAX`.
- If axes are swapped: `TOUCH_SWAP_XY 1`.
- If an axis is mirrored: `TOUCH_INVERT_X 1` or `TOUCH_INVERT_Y 1`.

### Backlight not turning on

- The backlight is driven by GPIO 21 (`TFT_BL_PIN` in `config.h`).
- `initDisplay()` calls `pinMode(TFT_BL_PIN, OUTPUT)` and `digitalWrite(TFT_BL_PIN, HIGH)`.
- If still dark, measure the pin voltage with a multimeter while running; if it reads 3.3 V the issue is the backlight circuit (some boards need an inverted signal — change `HIGH` to `LOW` in `ui.cpp`).

### Wrong board revision — different pin numbers

- All pins are in `src/config.h` (one place to change).
- `include/User_Setup.h` mirrors the display pins for TFT_eSPI — keep them in sync if you change them.

### Compile error: "User_Setup.h: No such file"

- Confirm `include/User_Setup.h` exists in the `SimonSays/include/` folder.
- Confirm `platformio.ini` has `-I include` in `build_flags`.

### Touch works but buttons in wrong quadrant

- The button boundaries are computed from `BTN_W`, `BTN_H`, and `STATUS_BAR_H` in `config.h` from the same values used to draw them, so this almost always means the touch calibration needs adjustment (see above).
