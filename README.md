# ESP32 Dual-Band Wi-Fi Analyzer

ESP32-C5 board plus an ILI9341 screen. Walk around, see what’s on 2.4 GHz and 5 GHz, pick a quieter channel. That’s the whole point.

This started from moononournation’s analyzer on [Instructables](https://www.instructables.com/ESP32-C5-Dual-Band-WiFi-Analyzer/). **v2.0** is the sketch I actually use for planning: four screens, channel scores, AP list, a short site summary.

How to wire it, what parts you need, and how to flash it from Arduino IDE: **[USER_MANUAL.md](USER_MANUAL.md)**

## v2.0 in short

Open `ESP32C5_WiFi_Planner/ESP32C5_WiFi_Planner.ino`. After it boots, tap **BOOT** to flip screens (spectrum → score → AP list → site). Hold BOOT on the list to scroll; on the other screens it dumps CSV over serial at 115200.

Arduino settings that actually work on this hardware (same as the old notes):

- Board: ESP32 → ESP32C5, package **3.3.1 or later**
- Flash size **16MB**
- Upload speed **921600**
- Partition: **Huge APP (3MB No OTA)**
- Libraries: GFX Library for Arduino (moononournation) and U8g2

Put the board in download mode first if upload fails: hold BOOT, tap RESET, let go of BOOT.

Wiring is the original pinout: 3V3/GND, then GPIO23 CS, 25 Reset, 24 DC, 8 MOSI, 10 SCK, 26 LED.

## What’s in this folder

- `ESP32C5_WiFi_Planner/` — **v2.0**, use this
- `ESP32C5WiFiAnalyzerUTF8/` — original single-screen analyzer
- `USER_MANUAL.md` — parts, wiring, flashing, how the screens work
- `Readme.txt` — the original hardware notes this repo was built from
