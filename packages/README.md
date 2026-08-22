# Pre-built firmware (ESP32-C5)

Use these if you do not want to compile from source. Board: **ESP32C5 Dev Module**, flash **16MB**, partition **Huge APP (3MB No OTA)**. Built with Arduino-ESP32 **3.3.11**.

| File | Version |
|------|---------|
| [Dual-Band-Wi-Fi-Analyser-v1.0.bin](Dual-Band-Wi-Fi-Analyser-v1.0.bin) | v1.0 (original-style spectrum) |
| [Dual-Band-Wi-Fi-Analyser-v2.0.bin](Dual-Band-Wi-Fi-Analyser-v2.0.bin) | v2.0 (spectrum, score, AP list, site) |

These are **merged** images (bootloader + partitions + app). Flash each file at address **0x0**.

## Put the board in download mode

1. Plug in USB.
2. Hold **BOOT**.
3. Tap **RESET**, then release **BOOT**.
4. Note the COM port in Device Manager (Windows) or `ls /dev/ttyUSB*` / `ls /dev/cu.*` (Linux/macOS).

## Flash with esptool (command line)

Install [esptool](https://docs.espressif.com/projects/esptool/) if you do not have it:

```
pip install esptool
```

Replace `COM5` with your port. On Linux/macOS use `/dev/ttyACM0` or similar.

**v2.0 (recommended):**

```
esptool --chip esp32c5 --port COM5 --baud 921600 write-flash 0x0 Dual-Band-Wi-Fi-Analyser-v2.0.bin
```

**v1.0:**

```
esptool --chip esp32c5 --port COM5 --baud 921600 write-flash 0x0 Dual-Band-Wi-Fi-Analyser-v1.0.bin
```

If `write-flash` is not recognised, your esptool is older — use `write_flash` instead.

After it finishes, tap **RESET**. You should see Dual Band Wi-Fi Analyser on the ILI9341.

## Flash with Espressif Flash Download Tool (Windows)

1. Download [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools).
2. ChipType: **ESP32-C5**.
3. Load the `.bin`, set address **0x0**, tick the row.
4. SPI speed 80 MHz, SPI mode DIO or QIO, flash size 16 MB.
5. Select the COM port, start download (board in download mode).

## Flash from a browser

You can also use [ESP Web Flasher / esptool-js](https://espressif.github.io/esptool-js/): pick ESP32-C5, add the `.bin` at offset `0x0`, connect the serial port, flash.

Do not flash a C3/S3/C6 binary onto a C5, or a 4 MB image onto this 16 MB Huge-APP build.
