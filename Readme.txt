Project location: https://www.instructables.com/ESP32-C5-Dual-Band-WiFi-Analyzer/

Board needed : esp32-c5 in Board lib version 33.1 onwards.
Display: ILI9341
Library: 'gfx library' by moononournation, u8g2

Select: Board as :esp32 -> esp32 c5 (after connecting board in boot mode.
	Tools -> Flash size -> 16MB
	upload speed -> 921600
	Partition scheme: Huge App (3mb no ota)

PIN connectivity -
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