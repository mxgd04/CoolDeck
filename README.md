# ESP32-S3 Touch Stream Deck

A DIY Stream Deck built with an **ESP32-S3 (N16R8)** and a **4.0" ST7796 SPI TFT
display with XPT2046 resistive touch (SKU: MSP4021, 320x480)**.

Tap the on-screen buttons to mute your mic, mute Discord, control Spotify,
adjust system volume, take a screenshot, open a browser/terminal, and turn
off your monitor — all from a small touchscreen connected to your PC.

![Stream Deck demo](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange)

---

## Hardware

- **Board:** ESP32-S3-DevKitC-1, N16R8 (16MB Flash / 8MB PSRAM, Octal PSRAM/Flash)
- **Display:** 4.0" SPI TFT, driver **ST7796**, resolution **320x480**, with
  **XPT2046** resistive touch (SKU MSP4021)
- **Graphics library:** [LovyanGFX](https://github.com/lovyan03/LovyanGFX)

---

## Pinout

The display panel and the touch controller use **two independent SPI buses**
on the ESP32-S3. This avoids the bus-arbitration issues that occur when the
display and touch controller share the same SPI peripheral on the S3.

| Display pin | ESP32-S3 GPIO | Function                  |
|-------------|---------------|----------------------------|
| VCC         | 3.3V          | Power (3.3V only, **not** 5V) |
| GND         | GND           | Ground                     |
| SCK         | GPIO 12       | Display SPI clock (SPI2)   |
| MOSI / SDI  | GPIO 11       | Display SPI data out (SPI2)|
| MISO / SDO  | GPIO 13       | Display SPI data in (SPI2) |
| CS          | GPIO 10       | Display chip select        |
| DC / RS     | GPIO 9        | Data/Command select         |
| RESET       | GPIO 14       | Display reset               |
| LED         | GPIO 21       | Backlight (PWM)             |

| Touch pin   | ESP32-S3 GPIO | Function                  |
|-------------|---------------|----------------------------|
| T_CLK       | GPIO 40       | Touch SPI clock (SPI3)     |
| T_DIN       | GPIO 41       | Touch SPI data out (SPI3)  |
| T_DO        | GPIO 42       | Touch SPI data in (SPI3)   |
| T_CS        | GPIO 15       | Touch chip select           |
| T_IRQ       | GPIO 16       | Touch interrupt (optional, polled in code) |

> Note: the touch controller is wired to **SPI3**, separate from the
> display's **SPI2**, even though both are physically the XPT2046 sharing
> the same panel. This was required to get reliable touch readings on the
> ESP32-S3.

---

## Touch Calibration

The touch controller is pre-calibrated for this specific panel with the
following raw ADC values (set in `main.cpp`):

```cpp
cfg.x_min = 438;
cfg.x_max = 3628;
cfg.y_min = 3785;
cfg.y_max = 513;
```

If you use a different panel of the same model, or touch coordinates don't
line up with the buttons, you'll need to recalibrate. A simple way is to
print raw `getTouch()` values while tapping the four corners of the screen
and adjust these four values accordingly.

---

## Firmware setup (PlatformIO)

1. Open this folder in VS Code with the **PlatformIO** extension.
2. `platformio.ini` is already configured for `esp32-s3-devkitc-1` with:
   - `board_build.arduino.memory_type = qio_opi` (required for N16R8 PSRAM)
   - `board_build.partitions = default_16MB.csv`
   - USB CDC enabled for Serial Monitor over the native USB port
3. Build and upload (`PlatformIO: Upload`).
4. Open the Serial Monitor at **115200** baud.

### Dependencies
- [`lovyan03/LovyanGFX`](https://github.com/lovyan03/LovyanGFX) (declared in `platformio.ini`)

---

## Stream Deck layout

The deck has **multiple pages** of 12 buttons (3x4 grid, landscape 480x320).
**Swipe left/right** anywhere on the screen to change page; the dots in the
top-right corner show the current page. Buttons fire when you **lift your
finger**, so a swipe that starts on a button doesn't trigger it.

### Page 1

| | Col 1 | Col 2 | Col 3 |
|---|---|---|---|
| **Row 1** | Mic mute | Discord mute | Camera on/off |
| **Row 2** | Spotify previous | Spotify play/pause | Spotify next |
| **Row 3** | Volume down | Volume up | Screenshot |
| **Row 4** | Open browser | Open terminal | Turn off monitor |

### Page 2

| | Col 1 | Col 2 | Col 3 |
|---|---|---|---|
| **Row 1** | System mute | File explorer | Task manager |
| **Row 2** | Show desktop | Lock PC | Windows settings |
| **Row 3** | *(empty)* | *(empty)* | *(empty)* |
| **Row 4** | *(empty)* | *(empty)* | *(empty)* |

### Info page (leftmost page)

The deck boots on Page 1; **swipe right** from there to reach the **PC monitor page**, showing CPU usage and
temperature, GPU usage and temperature, RAM, VRAM and network speed. The values
are sent by `streamdeck.py` every second as
`STAT:cpu=23,ctemp=45,ram=61,...`. If no data arrives for 4 s, the screen shows
"Sin datos del PC".

- **CPU / RAM / network:** `psutil`
- **GPU / VRAM:** `nvidia-smi` (NVIDIA cards only)
- **CPU temperature:** Windows doesn't expose it to normal programs, so keep
  [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)
  open **as administrator** (`winget install LibreHardwareMonitor.LibreHardwareMonitor`)
  and enable **Options > Remote Web Server > Run** (port 8085). The bridge reads
  `http://localhost:8085/data.json`. Enable **Run on Windows Startup** so it's
  always available. Without it the screen shows `--`.

### Adding buttons or pages

1. In `main.cpp`, edit the `PAGES` array: each entry is
   `{ label, sublabel, color, icon, "CMD:NAME", isToggle }`. Use `EMPTY` for
   unused slots, or add a new block of 12 entries for another page.
2. In `streamdeck.py`, add a function and register `"CMD:NAME"` in `COMMANDS`.

Each button press sends a text command over USB Serial (e.g.
`CMD:MIC_TOGGLE`, `CMD:SPOTIFY_PLAY`, `CMD:VOL_UP`...).

---

## PC bridge (Windows)

The `streamdeck.py` script listens on the ESP32's serial port and executes the
corresponding actions on a Windows PC:

- **Mic mute** via `pycaw` (system default microphone)
- **Discord mute** by briefly focusing the Discord window and sending its
  default `Ctrl+Shift+M` shortcut, then restoring the previous window
- **Spotify controls** via global media keys
- **System volume** via global media keys
- **Screenshot** via `Win+Shift+S`
- **Open browser / terminal / turn off monitor** via standard Windows APIs

### Requirements
```bash
pip install pyserial pycaw comtypes pywin32 psutil
```

Run it with:
```bash
python streamdeck.py
```

It auto-detects the ESP32's serial port (CP210x/CH9102/USB CDC).

If not, you can select the port manually when the program asks for it.

---
