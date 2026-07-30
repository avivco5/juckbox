# ESP32-S3 Touchscreen Jukebox

Standalone jukebox running on an ESP32-S3 with a 3.5" QSPI touchscreen, a microSD card slot, and a real I2S audio path (ES8311 codec + speaker amp). No WiFi, no phone, no router — everything is controlled directly on the device's touchscreen.

---

## What it does

| Feature | Status |
|---|---|
| Touchscreen UI (Home / Songs / Settings) | ✓ |
| List MP3 / WAV songs from SD card | ✓ |
| Play / Pause / Resume / Next / Previous | ✓ |
| Volume control (0–100, 5% steps) | ✓ |
| Real I2S audio playback (ES8311 codec) | ✓ |
| Elapsed-time / progress display (real song duration) | ✓ |
| Settings persist across power cycles (volume, repeat, shuffle, last song, theme) | ✓ |
| 12 selectable UI color themes, including a user-editable "Custom" theme (Settings → THEME) | ✓ |
| External PLAY/PAUSE and NEXT buttons | ✓ |
| RGB status LED (rainbow while playing, or always-on via Settings) | ✓ |
| Repeat (Off / One / All) and Shuffle | ✓ |
| Pinball + Air Hockey games (GAMES tab — no reflashing needed, music keeps playing) | ✓ |
| WiFi / web remote control | Removed (see [Legacy WiFi/Web Control](#legacy-wifiweb-control)) |
| Playlist support | Removed (see [Legacy WiFi/Web Control](#legacy-wifiweb-control)) |
| Physical buttons (MCP23017) | Future |

---

## Hardware

- ESP32-S3 development board with PSRAM (OPI, 8 MB embedded — N16R8 class module)
- 3.5" QSPI display, 480×320, ST77922-class panel controller
- Capacitive touch panel over I2C (CST9217/TDDI-style controller at address `0x55`)
- ES8311 I2C audio codec driving an I2S DAC path into a speaker amplifier
- MicroSD card (FAT32) wired in **SD_MMC 4-bit mode** (not SPI)

---

## Pin Configuration

Pins are currently **hardcoded per module** (display, touch, SD, audio each define their own pins at the top of their `.cpp` file) rather than centralized in `config.h` — see [Known Limitations](#known-limitations). To rewire the board, edit the file for the subsystem you're changing.

### Display — QSPI ([src/DisplayDriver.cpp](src/DisplayDriver.cpp))

| Signal | GPIO |
|---|---|
| CS    | 10 |
| CLK   | 12 |
| D0    | 11 |
| D1    | 13 |
| D2    | 14 |
| D3    | 9  |
| Backlight | 41 |

### Touch — I2C ([src/TouchController.cpp](src/TouchController.cpp))

| Signal | GPIO |
|---|---|
| SDA | 38 |
| SCL | 39 |
| INT | 47 |
| RST | 48 |

I2C address: `0x55`. This bus is initialized once in `TouchController::begin()` and is reused by the ES8311 codec — `touchController.begin()` must run before `audioController.begin()` (already the case in [src/main.cpp](src/main.cpp)).

### SD Card — SD_MMC 4-bit ([src/SongLibrary.cpp](src/SongLibrary.cpp))

| Signal | GPIO |
|---|---|
| CLK  | 5 |
| CMD  | 4 |
| D0   | 6 |
| D1   | 7 |
| D2   | 2 |
| D3   | 3 |

### Audio — I2S + ES8311 codec ([src/JukeboxAudioOutput.cpp](src/JukeboxAudioOutput.cpp), [src/Es8311Codec.cpp](src/Es8311Codec.cpp))

| Signal | GPIO |
|---|---|
| MCLK | 17 |
| BCLK | 18 |
| WS   | 21 |
| DOUT | 15 |
| PA enable | 1 (active-low) |

ES8311 codec I2C address: `0x18`. Codec is driven at 44.1 kHz / 16-bit stereo by default.

### External buttons ([src/main.cpp](src/main.cpp))

Both spare pins on the board's "Expansion Input Pins" connector (silkscreen IO45/IO46) are now used — IO38/39 are taken by the touch I2C bus, so there's nothing left over for a third button without an I2C GPIO expander (e.g. MCP23017 — see the "Future" row in the feature table above).

| Button | GPIO | Idle state | Wiring |
|---|---|---|---|
| PLAY/PAUSE | 46 | HIGH | Button to GND + **external ~10kΩ pull-up from GPIO46 to 3.3V** (GPIO46 is input-only with no internal pull-up — leaving this resistor out makes the pin float and read as repeated phantom presses) |
| NEXT | 45 | HIGH | Button to GND + **external ~10kΩ pull-up from GPIO45 to 3.3V** — wired the same way as PLAY/PAUSE on this build. **This is not the recommended wiring for this pin** — see the warning below. |

Both buttons use plain `INPUT` (not `INPUT_PULLUP`/`INPUT_PULLDOWN`) since neither pin's internal pull is reliable enough to depend on — the external resistor is mandatory, not optional, in both cases.

```
PLAY/PAUSE — GPIO46 (idle HIGH, pressed LOW)      NEXT — GPIO45 (idle HIGH, pressed LOW)
─────────────────────────────────────────────      ─────────────────────────────────────────────
                 3.3V                                                3.3V
                  │                                                   │
              ┌───┴───┐                                           ┌───┴───┐
              │ 10kΩ  │  external pull-up                         │ 10kΩ  │  external pull-up
              └───┬───┘                                           └───┬───┘
                  ├───────────────► GPIO46 (ESP32-S3, INPUT)          ├───────────────► GPIO45 (ESP32-S3, INPUT)
                  │                                                   │
              ┌───┴───┐                                           ┌───┴───┐
              │button │                                           │button │
              └───┬───┘                                           └───┬───┘
                  │                                                   │
                 GND                                                 GND
```

> **⚠️ Boot-reliability warning:** GPIO45 is sampled by the ESP32-S3's ROM bootloader at power-on to select flash voltage (3.3V vs 1.8V), and is *supposed* to idle LOW to avoid loading that strap — that's why earlier versions of this doc (and the code) had it wired with a **pull-down** instead (button to 3.3V, resistor to GND, idle LOW/press HIGH). This build wires it as pull-up instead (idle HIGH), matching the diagram above, because that's how the hardware was actually built. The firmware's button-reading code has been updated to match (`pressed = digitalRead(...) == LOW` for both buttons now — see `main.cpp`, `PinballMain.cpp`, `AirHockeyMain.cpp`), but **that's a software accommodation, not a fix for the underlying boot-strap risk**: the pin's voltage at boot is a hardware fact the bootloader reads before any of this code runs. If you see occasional failed/garbled boots, rewiring GPIO45 back to pull-down (and reverting the `== LOW` back to `== HIGH` for that pin in the three files above) is the actual fix.

### RGB status LED ([src/StatusLed.cpp](src/StatusLed.cpp))

| Signal | GPIO |
|---|---|
| Onboard RGB LED (WS2812-style, single data line) | 40 |

This pin is **not documented anywhere by the vendor** — it was found by physically probing the board with [src/RgbPinFinder.cpp](src/RgbPinFinder.cpp) (a one-off diagnostic, see its own PlatformIO env below). By default (`LedMode::WhenPlaying`) the LED cycles through a rainbow while a song is playing and turns off otherwise; the Settings page's **LED** toggle switches to `LedMode::Always` so it keeps cycling even when paused/stopped. Persisted as `ledmode=0/1` in `jukebox_settings.cfg`.

> Don't trust GPIO22/23/24/25 as candidates for anything — they don't physically exist on this ESP32-S3 chip (confirmed by hardware errors when tested). Don't assume GPIO48 drives the LED either — it matches the generic Arduino-ESP32 `esp32-s3-devkitc-1` board profile's default NeoPixel pin, but on this board GPIO48 is already the touch controller's reset line, confirmed unrelated to the LED.

---

## SD Card Folder Structure

```
SD card root/
├── songs/
│   ├── 001.mp3
│   ├── 002.wav
│   └── ...
└── jukebox_settings.cfg
```

`/songs` is created automatically on first boot if it doesn't exist. Only `.mp3` and `.wav` files are listed; the on-screen title is the filename without its extension.

`jukebox_settings.cfg` is a small plain-text key=value file (no JSON dependency) written by [src/JukeboxUI.cpp](src/JukeboxUI.cpp) whenever volume, repeat mode, shuffle, the selected song, theme, or LED mode changes, and read back on boot. The last song is only *selected*, never auto-played, on startup.

Song duration shown on the Home page and used for the progress bar is computed once per song during `SongLibrary::scan()` ([src/SongLibrary.cpp](src/SongLibrary.cpp)): exact for WAV (parsed from the `fmt`/`data` RIFF chunks), and an estimate for MP3 (file size ÷ bitrate of the first frame, assuming constant bitrate — will be off for VBR-encoded files since there's no Xing/VBRI header parsing).

---

## Install PlatformIO

1. Install [VS Code](https://code.visualstudio.com/)
2. Open Extensions → search `PlatformIO IDE` → Install
3. Restart VS Code

Or use the CLI:
```bash
pip install platformio
```

---

## Build and Upload

Open the `CODE/` folder in VS Code with PlatformIO, or use the terminal:

```bash
# Compile only (check for errors)
pio run

# Compile and upload firmware to the ESP32-S3
pio run --target upload

# Open Serial Monitor
pio device monitor
```

There is no web UI to upload anymore — `board_build.filesystem = littlefs` in `platformio.ini` and the `data/` folder are leftovers from the previous WiFi-controlled version and are not used by the current firmware (see below).

There are also alternate PlatformIO environments that build completely separate firmware images sharing only [src/DisplayDriver.cpp](src/DisplayDriver.cpp) — `rgb_finder` and `diagtest` are flashed *instead of* the jukebox (re-flash `esp32s3` afterwards to get it back):

```bash
pio run -e rgb_finder -t upload   # one-off: discover the RGB LED's GPIO (see above)
pio run -e diagtest -t upload     # assembly diagnostic: SD/song list + button press states
pio run -e pinball -t upload      # standalone pinball "cartridge" — not needed if you just want the GAMES tab
pio run -e airhockey -t upload    # standalone air hockey "cartridge" — same idea
pio run -e esp32s3 -t upload      # flash the real jukebox firmware again afterwards
```

### Games ([src/PinballGame.h](src/PinballGame.h) / [src/AirHockeyGame.h](src/AirHockeyGame.h))

Each game is playable two ways from the exact same code:

1. **From inside the jukebox** — the **GAMES** tab (see below), no reflashing needed. Music keeps playing in the background (`AudioController` isn't touched), since only the display and these two buttons are repurposed while a game is running.
2. **Standalone firmware** (`pio run -e pinball -t upload` / `-e airhockey -t upload`) — a dedicated "arcade cabinet" image with nothing else on it, for when you don't want the jukebox at all. Re-flash `esp32s3` afterward to get it back.

Both entry points per game (e.g. [src/PinballMain.cpp](src/PinballMain.cpp) for the standalone build, `JukeboxUI`'s `Page::Pinball` for the tab) just drive the same game class — the physics/rendering exist in exactly one place. `JukeboxUI::setButtonA()`/`setButtonB()` dispatch the two physical buttons to whichever game is currently active (`main.cpp` doesn't know or care which game that is).

**Pinball** — PLAY/PAUSE (IO46) = left flipper, NEXT (IO45) = right flipper. 3 balls per game; bumpers are worth 100 points each. When a ball drains past both flippers, the next one serves automatically; after the third, it's Game Over — press either button to restart.

**Air Hockey** — PLAY/PAUSE (IO46) = paddle up, NEXT (IO45) = paddle down, against a simple CPU-controlled opponent paddle. First to 5 points wins; press either button to restart after Game Over.

Both use simple fixed-timestep 2D physics (gravity/collision reflection for pinball, paddle-angle deflection for air hockey), not a physics engine — deliberately kept simple since [DisplayDriver::flush()](src/DisplayDriver.cpp) always pushes the *entire* 480×320 framebuffer (no partial/dirty-rect updates), which caps real-world frame rate at roughly 20–30 FPS regardless of how little changed — plenty for this kind of game, but a reason not to expect smooth 60 FPS on this display.

When played from the **GAMES** tab, an on-screen **EXIT** button (top-right) returns to the jukebox — the physical buttons can't do it themselves since they're busy being game controls while a game is running.

---

## Using the Touchscreen UI

The UI has four tabs along the bottom: **HOME**, **SONGS**, **SETTINGS**, **GAMES**.

- **HOME** — shows the current song, play/pause state, elapsed time, and has previous/play-pause/next buttons plus a volume +/- control.
- **SONGS** — paginated list (4 per page) of everything found in `/songs`. Tap a row to select and start playing it; tap **SCAN** to re-read the SD card after adding files; use `<` / `>` to page through the list.
- **SETTINGS** — a **REPEAT** button (tap to cycle OFF → ONE → ALL), a **SHUFFLE** toggle, and an **LED** toggle (AUTO = rainbow only while a song plays, the original behavior; ALWAYS = rainbow runs continuously even when paused/stopped); a **THEME** row with `<`/`>` arrows flanking it (step through themes one at a time, same as the Theme Menu's edge buttons) — tapping the theme name itself opens the full picker (see below). No volume control here — that's on **HOME** only, to leave more room for these.
- **GAMES** — **PINBALL** and **AIR HOCKEY** (see above). While a game is running, the jukebox's normal UI (top bar, bottom nav, themes) is replaced entirely by the game's own full-screen display; PLAY/PAUSE and NEXT become game controls instead of playback controls until you tap **EXIT**.

Repeat/shuffle only affect what happens automatically when a song finishes on its own (not the manual `<<`/`>>` buttons, which always go sequentially): **Repeat: One** replays the same song forever; **Repeat: All** advances to the next song (sequential, or random if Shuffle is on) and never stops on its own; **Repeat: Off** with Shuffle on still jumps to a random song when one ends; **Repeat: Off** with Shuffle off just stops, like before this feature existed.

Touch input is debounced in [src/main.cpp](src/main.cpp) to work around a touch-controller quirk where some CST9217/TDDI firmware revisions keep reporting the last coordinate after release; repeated taps are re-armed by time and by a "moved to a different control" check.

### Theme System

Tapping the bottom row of the **SETTINGS** page (labeled `THEME: <name>`) opens a scrollable **Choose Theme** screen listing all 12 themes; tapping a row applies it immediately and returns to Settings. The bottom nav bar stays visible on this screen too, so tapping **HOME**/**SONGS**/**SETTINGS** also exits it. Two arrow pills on the left/right edges of the screen step through themes one at a time (wrapping around) and apply instantly, for quickly browsing/previewing — the on-screen list auto-scrolls to keep the current pick visible.

- **Where themes are defined**: [src/Theme.h](src/Theme.h) / [src/Theme.cpp](src/Theme.cpp). Each themed design is six hand-picked colors (`background`, `panel`, `title`, `text`, `selected`, `border`); the rest of the palette (raised-surface tint, muted text, dark button text) is derived once at theme-switch time by `ThemeManager::buildDefinitions()`, so adding a new design theme only means one `makeTheme(...)` call. The exception is **Classic** — the app's original hand-tuned color scheme from before the theme system existed, kept as its own option since all nine of its colors are specified directly via `makeRawTheme(...)` rather than derived. New themes must be appended right before `THEME_COUNT` in `Theme.h`, never inserted earlier — the theme index is what's persisted to disk, so reordering existing entries would silently reassign everyone's saved theme.
- **Adding a static theme (the exercise for trainees)**: add one enum value right before `THEME_COUNT` in `Theme.h`, then one `makeTheme("Name", "Title Text", bg, panel, title, text, selected, border)` call in `ThemeManager::buildDefinitions()` in `Theme.cpp`. That's the entire change — `JukeboxUI.cpp` doesn't need to be touched, since every screen already reads colors through `themeManager.colors()`.
- **Custom theme (Settings → THEME → Custom row → EDIT)**: the one theme whose colors are chosen on-device instead of hardcoded. The editor screen (`JukeboxUI::drawThemeEditor`) shows the 6 color roles, each cycling through a shared 14-color swatch list (`kThemeSwatches` in `Theme.h`) via `<`/`>` — no free-form color picker, to keep the touch UI simple and stable. Selecting Custom applies it immediately (`ThemeManager::setCustomColors`, rebuilt the same way as any other theme) and every change is saved right away as `custom_bg`/`custom_panel`/`custom_title`/`custom_text`/`custom_selected`/`custom_border` (hex) in `jukebox_settings.cfg`, so it's remembered across restarts.
- **Default theme**: `ThemeManager::begin()` in `Theme.cpp` sets `_current = THEME_PINEAPPLE`. Change that line to change the factory default.
- **Persistence**: the selected theme is saved as `theme=<index>` in `jukebox_settings.cfg` alongside volume/repeat/shuffle (see `JukeboxUI::loadSettings()`/`saveSettings()`), so it survives power cycles.
- **Scope**: colors change everywhere via `themeManager.colors()`, plus small decorative accents on the **Home** page only (`JukeboxUI::drawThemeDecorations`) — no per-theme fonts or animations, so every theme stays equally fast and memory-safe. Each of the 10 box-design themes gets a matching 16×16 pixel-art icon (pineapple, star, robot head, rocket, skull, speaker, cassette, tree, gear, monster face — `iconPineapple`/`iconStar`/etc. in `JukeboxUI.cpp`), drawn twice (left of the album-art panel, right of the volume panel) via `Arduino_GFX::drawBitmap()`, recolored per theme by passing `theme.accent` as the draw color. A couple of themes add a tiny extra accent dot (bubble/coin/spark) from the remaining primitive helpers (`drawBubble`/`drawDot`/`drawCoin`). **Classic** and **Custom** intentionally have no icon — there's no fixed "vibe" to draw for either.
- **Where the icons come from**: hand-drawn with a one-off Pillow (Python) script — draw simple vector shapes (circle/polygon/line) at 160×160, downsample to 16×16 with Lanczos resampling, threshold to 1-bit. Not sourced from image files, so there's no asset pipeline to maintain; each icon is a 32-byte `static const uint8_t[]` array (1bpp, MSB-first, byte-packed rows — the exact format `drawBitmap()` expects). All 10 icons together cost ~320 bytes of flash. To change or add an icon, regenerate with the same technique (or hand-edit the hex bytes — each row is 2 bytes for 16 columns) and add a case to the `icon = ...` switch in `drawThemeDecorations()`.
- **Adapting the spec's button mapping**: the original theme-menu spec assumed physical NEXT/BACK/PLAY/SELECT buttons with long-press gestures. This hardware only has two buttons (PLAY/PAUSE on GPIO46, NEXT on GPIO45 — see [Pin Configuration](#pin-configuration)), both wired globally to playback in `main.cpp`. Rather than overload them with menu-navigation meaning, the theme menu is touch-only, reached from Settings as described above.

### Touch reliability — diagnostics only (active "fix" reverted)

The touch controller and the ES8311 audio codec share one I2C bus (GPIO38/39), and touch occasionally stops responding for no obvious reason. A first attempt at fixing this (I2C bus recovery + a periodic preventive rearm every 30s) made things **worse** — `Wire.begin()` isn't cheap, and re-running it on every accepted touch destabilized the I2C driver itself. That attempt was reverted.

What's left, in [src/TouchController.cpp](src/TouchController.cpp), is **passive only**: `readRegister()` logs `[Touch] I2C read failing` / `[Touch] I2C read recovered after N failed attempt(s)` on state transitions (not every poll, so it won't flood the log). If touch stops responding again, check the serial log for these messages — that'll tell us whether it's really an I2C-level failure before trying another fix.

---

## Known Limitations

- `src/config.h` is **not included by any file in the active build** (`platformio.ini`'s `build_src_filter`). All real pin/volume/storage constants are hardcoded in the relevant driver `.cpp` files instead — `config.h` is stale and should not be trusted as the source of truth for pins.
- No authentication or remote control of any kind — the device is controlled only via its own touchscreen.
- `next` / `previous` always wrap around the full song list found by the last `SCAN`; there's no shuffle or playlist concept anymore.
- Long song titles are truncated with `...` on screen; no Hebrew/RTL text rendering is attempted (the on-device UI is Latin/numeric only — Hebrew RTL lived in the removed web UI, see below).
- The RGB LED's color/brightness/animation speed are fixed constants in `StatusLed.cpp`, not exposed in the Settings UI.

---

## Legacy WiFi/Web Control

This repository still contains an earlier version of the jukebox that exposed a WiFi access point and a Hebrew RTL web UI (`/api/...` JSON endpoints, file upload, playlists):

- [src/WebServerManager.h](src/WebServerManager.h) / [.cpp](src/WebServerManager.cpp)
- [src/StorageManager.h](src/StorageManager.h) / [.cpp](src/StorageManager.cpp)
- [src/PlaylistManager.h](src/PlaylistManager.h) / [.cpp](src/PlaylistManager.cpp)
- [data/index.html](data/index.html), [data/app.js](data/app.js), [data/style.css](data/style.css)

**These files are excluded from the build** via `build_src_filter` in `platformio.ini` and are **bit-rotted**: they call `AudioController` methods (`getCurrentSong()`, `getVolume()`, `next()`, `previous()`) that no longer exist on the current `AudioController` (which was rewritten for real I2S/ES8311 playback), and they depend on `ESPAsyncWebServer`/`ArduinoJson`, which are no longer listed in `lib_deps`. They're kept around for reference only — reviving WiFi control would mean re-wiring them against the current `AudioController` API and adding the libraries back.
