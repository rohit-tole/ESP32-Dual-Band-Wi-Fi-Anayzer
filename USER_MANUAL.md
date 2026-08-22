# Dual Band Wi-Fi Analyser v2.0 — user manual

ESP32-C5 + ILI9341. It scans 2.4 and 5 GHz and draws what it heard. It does not join your Wi-Fi, and it is not a calibrated meter. RSSI moves if you turn the board or stand in front of the antenna. Treat it as a walk-around tool, then confirm with something proper if the design actually matters.

Based on the [Instructables ESP32-C5 Dual Band WiFi Analyzer](https://www.instructables.com/ESP32-C5-Dual-Band-WiFi-Analyzer/). Wiring and Arduino settings below are the same ones from the original `Readme.txt`.

---

## Parts

You need:

- An **ESP32-C5** board (DevKitC-1 or similar). Dual-band, no 6 GHz.
- An **ILI9341** SPI TFT, 320×240. Touch is not used.
- Jumpers
- USB cable (power + upload)
- Arduino IDE on the PC

Libraries from Library Manager:

- **GFX Library for Arduino** — moononournation
- **U8g2** — olikraus

A power bank is handy if you are walking a floor.

---

## Wiring

Feed the panel from **3V3**, not 5V, unless you are sure the module is 5V-safe *and* the SPI lines stay at 3.3V.

```
ESP32-C5    ILI9341
========    =======
3v3      -> VCC
GND      -> GND
GPIO23   -> CS
GPIO25   -> Reset
GPIO24   -> DC/RS
GPIO8    -> SDI/MOSI
GPIO10   -> SCK
GPIO26   -> LED
```

Leave MISO / SDO unconnected. There is no touch wiring.

```mermaid
flowchart LR
  subgraph ESP["ESP32-C5"]
    V3[3V3]
    GND[GND]
    CS[GPIO23]
    RST[GPIO25]
    DC[GPIO24]
    MOSI[GPIO8]
    SCK[GPIO10]
    BL[GPIO26]
  end
  subgraph TFT["ILI9341"]
    TVCC[VCC]
    TGND[GND]
    TCS[CS]
    TRST[Reset]
    TDC[DC/RS]
    TMOSI[SDI/MOSI]
    TSCK[SCK]
    TLED[LED]
  end
  V3 --> TVCC
  GND --> TGND
  CS --> TCS
  RST --> TRST
  DC --> TDC
  MOSI --> TMOSI
  SCK --> TSCK
  BL --> TLED
```

On a DevKitC-1, **BOOT is GPIO28**. After the firmware is running, a short press changes screen. If you hold BOOT and hit RESET, you are back in download mode — that’s only for flashing.

If you want a spare button, GPIO0 to GND does the same thing as BOOT while the sketch is running.

---

## What the screens do

Short-press BOOT. The bar at the top stays put: page name, how many APs on 2.4 vs 5, how old the scan is, and a suggested pair like `use 1/149`.

**Spectrum** — the old-style graph, two bands. Blob width follows 20 / 40 / 80 MHz so an 80 MHz AP actually looks wide. Numbers under the channels are AP counts. Only a few SSIDs get labeled (the strong ones), otherwise the 5 GHz plot turns into mush. RSSI ticks on the right: −30, −50, −70, −90.

**Score** — this is the planner page. 2.4 GHz only really scores **1 / 6 / 11**, with overlap from the in-between channels counted in. 5 GHz is UNII channels; DFS ones have a `*`. Lower score is cleaner.

**AP list** — one line per BSS: name, channel, width, RSSI, auth, phy (`ax` / `ac` / `n`). Strongest first. Open networks in red. Hidden ones show as `*` plus the last part of the BSSID. Hold BOOT to scroll.

**Site** — counts: open / WPA2 / WPA3, hidden, 40 MHz on 2.4 (that’s a problem), DFS APs, SSIDs that exist on both bands, SSIDs spread across many channels, strongest AP, and the suggested 2.4 / 5 channels.

Long-press BOOT on spectrum / score / site reprints the CSV on serial (115200). Columns: `bssid,ssid,band,channel,bw_mhz,rssi,auth,phy,dfs,unii`

Scans are about every 3 seconds, 400 ms dwell, both bands, hidden included. If a scan comes back empty, the last good picture stays on the screen.

This is beacons and probe responses, not airtime. Fine for “don’t put the new AP on 6”, not a substitute for a spectrum analyzer.

---

## Flash a pre-built binary (no Arduino compile)

Merged images for ESP32-C5 (16MB, Huge APP) live in [`packages/`](packages/):

- [`packages/Dual-Band-Wi-Fi-Analyser-v2.0.bin`](packages/Dual-Band-Wi-Fi-Analyser-v2.0.bin) — current firmware
- [`packages/Dual-Band-Wi-Fi-Analyser-v1.0.bin`](packages/Dual-Band-Wi-Fi-Analyser-v1.0.bin) — v1.0 spectrum UI

Hold **BOOT**, tap **RESET**, release **BOOT**. Then (change `COM5` to your port):

```
pip install esptool
esptool --chip esp32c5 --port COM5 --baud 921600 write-flash 0x0 Dual-Band-Wi-Fi-Analyser-v2.0.bin
```

Flash at **0x0**. Full notes: [`packages/README.md`](packages/README.md). Tap RESET when it is done.

---

## Flashing with Arduino IDE

Same recipe as the original text file: C5 board package 3.3.1+, 16MB flash, 921600, Huge APP, GFX + u8g2.

**Once on the PC**

Install [Arduino IDE](https://www.arduino.cc/en/software). Under File → Preferences, add this URL if ESP32 isn’t there yet:

`https://espressif.github.io/arduino-esp32/package_esp32_index.json`

Boards Manager: search **esp32** (Espressif) and install **3.3.1 or later**. The old note said “33.1 onwards” — that’s this package.

Then Library Manager: **GFX Library for Arduino** (moononournation) and **U8g2**.

**Open the right sketch**

File → Open → [`Dual Band Wi-Fi Analyser v2.0/Dual Band Wi-Fi Analyser v2.0.ino`](Dual%20Band%20Wi-Fi%20Analyser%20v2.0/Dual%20Band%20Wi-Fi%20Analyser%20v2.0.ino)

That’s the current firmware. For the older sketch use [`Dual Band Wi-Fi Analyser v1.0/Dual Band Wi-Fi Analyser v1.0.ino`](Dual%20Band%20Wi-Fi%20Analyser%20v1.0/Dual%20Band%20Wi-Fi%20Analyser%20v1.0.ino). Don’t open another `.ino` from this repo in the same IDE window or Arduino will glue the files together.

**Download mode**

Plug in USB. If the port is missing or upload dies, hold **BOOT**, tap **RESET**, release BOOT. Then pick the COM port under Tools → Port.

**Tools menu**

| Setting | Value |
|---------|--------|
| Board | ESP32 → ESP32C5 Dev Module (or ESP32C5) |
| Flash Size | 16MB |
| Upload Speed | 921600 |
| Partition Scheme | Huge APP (3MB No OTA) |

Verify, then Upload. If the screen stays black after “Hard resetting via RTS”, tap RESET. You should get **Dual Band Wi-Fi Analyser** on the banner, then the spectrum after the first scan.

Serial Monitor at **115200** if you want the CSV.

---

## Using it on site

Power from USB. Backlight is GPIO26 — if that’s dark, the LED pin isn’t hooked up. Give it a few seconds for the first dual-band scan. Walk. Look at Score for the suggested channels, then the AP list if you need names and widths. Don’t obsess over a 3 dB RSSI change; the antenna isn’t calibrated.

---

## If something’s wrong

Blank screen: check CS / DC / RST / MOSI / SCK, 3V3, GND, and GPIO26 → LED.

Upload failed: download mode, right COM port, a cable that actually has data, Serial Monitor closed.

Compile complaining about 5 GHz or the wrong chip: you didn’t select ESP32C5, or the board package is older than 3.3.1.

Sketch too big: Huge APP + 16MB flash. That’s not optional on this build.

Garbage on the LCD: confirm it’s really an ILI9341, not an ST7789 with the same pin header.

BOOT does nothing after boot: DevKit uses GPIO28. Don’t hold BOOT at reset. GPIO0 is the backup button.

No APs: wait for a full scan. 5 GHz dies quickly through walls.

Only scan networks you’re supposed to be looking at. C5 has no 6 GHz. DFS (52–144) can vanish in real life when radar detection kicks in.

---

v1.0 is basically the original Instructables / moononournation project, with some UI tweaks ([`Dual Band Wi-Fi Analyser v1.0`](Dual%20Band%20Wi-Fi%20Analyser%20v1.0/)). This manual is for [`Dual Band Wi-Fi Analyser v2.0`](Dual%20Band%20Wi-Fi%20Analyser%20v2.0/).
