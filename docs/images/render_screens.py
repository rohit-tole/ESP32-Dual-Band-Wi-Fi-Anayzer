#!/usr/bin/env python3
"""Native 960x720 UI panels (same 4:3 as ILI9341). Fonts drawn at this size — no pixel scale-up."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

W, H = 960, 720
STATUS_H, FOOTER_H, RSSI_W = 48, 40, 96
OUT = Path(__file__).resolve().parent

BLACK = (12, 12, 14)
WHITE = (245, 245, 245)
CYAN = (64, 224, 255)
LIME = (80, 220, 80)
YELLOW = (255, 220, 70)
RED = (255, 70, 70)
ORANGE = (255, 160, 50)
MAGENTA = (230, 80, 230)
LIGHTGREY = (170, 170, 175)
MEDIUMBLUE = (28, 55, 150)
LIMEGREEN = (40, 160, 70)
DODGER = (50, 140, 255)
NAVY = (22, 40, 110)

CH_COLORS = [RED, ORANGE, YELLOW, LIME, CYAN, DODGER, MAGENTA] * 12

LEGEND = [
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0,
    0, 0, 36, 0, 0, 0, 44, 0, 0, 0, 52, 0, 0, 0,
    60, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 100, 0, 0, 0, 108, 0, 0, 0, 116, 0, 0, 0, 124,
    0, 0, 0, 132, 0, 0, 0, 140, 0, 0, 0, 149, 0, 0,
    0, 157, 0, 0, 0, 165, 0, 0, 0, 173, 0, 0, 0,
]


def channel_idx(ch):
    if ch <= 14:
        return ch - 1
    if ch <= 68:
        return 14 + ((ch - 32) // 2)
    if ch <= 144:
        return 41 + ((ch - 96) // 2)
    if ch <= 177:
        return 67 + ((ch - 149) // 2)
    return 82


def rssi_color(rssi):
    if rssi >= -50:
        return LIME
    if rssi >= -70:
        return YELLOW
    return RED


def font(size, bold=False):
    paths = [
        r"C:\Windows\Fonts\segoeui.ttf" if not bold else r"C:\Windows\Fonts\segoeuib.ttf",
        r"C:\Windows\Fonts\calibri.ttf",
        r"C:\Windows\Fonts\consola.ttf",
        r"C:\Windows\Fonts\arial.ttf",
    ]
    for p in paths:
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


F14, F16, F18, F20, F22 = font(14), font(16), font(18), font(20), font(22)
FB18 = font(18, True)
FB22 = font(22, True)


def new_screen():
    im = Image.new("RGB", (W, H), BLACK)
    return im, ImageDraw.Draw(im)


def status(d, page, name, n24, n50, rec24, rec50):
    d.rectangle((0, 0, W, STATUS_H), fill=NAVY)
    d.text((14, 12), f"{page}/4  {name}    2.4:{n24}    5:{n50}    3s    use {rec24}/{rec50}",
           fill=WHITE, font=F20)


def footer(d, list_page=False):
    d.rectangle((0, H - FOOTER_H, W, H), fill=NAVY)
    msg = "BOOT short: next screen      hold: scroll list" if list_page else \
          "BOOT short: next screen      hold: CSV on Serial"
    d.text((14, H - 30), msg, fill=CYAN, font=F16)


def half_ellipse(d, cx, baseline, rx, ry, color):
    rx, ry = max(3, rx), max(3, ry)
    d.pieslice([cx - rx, baseline - ry, cx + rx, baseline + ry], 180, 360, fill=color)


def rssi_scale(d, baseline, graph_h):
    x = W - RSSI_W
    for rssi in (-30, -50, -70, -90):
        y = baseline - int((rssi - (-100)) * (graph_h - 1) / 70)
        c = rssi_color(rssi)
        d.line((x, y, x + 16, y), fill=c, width=2)
        d.text((x + 22, y - 10), str(rssi), fill=c, font=F16)


APS_24 = [
    ("Office", 6, 20, -41), ("Cafe-Free", 6, 20, -55), ("Home", 1, 20, -62),
    ("IoT-Hub", 1, 20, -78), ("Neighbor", 3, 20, -71), ("Guest", 11, 20, -70),
    ("Printer", 11, 20, -81), ("Shop", 6, 20, -68), ("APT-2G", 6, 40, -58),
    ("Cam-01", 1, 20, -84), ("Mesh-2G", 11, 20, -66), ("Lab", 6, 20, -73),
]
APS_50 = [
    ("Home", 36, 80, -48), ("Corp", 149, 80, -52), ("Mesh-5", 44, 40, -65),
    ("Office-5", 40, 80, -61), ("Guest5", 149, 40, -69), ("APT-5", 157, 20, -72),
    ("IoT-5", 36, 20, -77), ("Radar-AP", 100, 80, -64), ("U3-Back", 161, 20, -80),
]
COUNTS_24 = {1: 4, 3: 1, 6: 5, 11: 3}
COUNTS_50 = {36: 2, 40: 1, 44: 1, 100: 1, 149: 2, 157: 1, 161: 1}


def save(im, name):
    path = OUT / name
    im.save(path, "PNG", optimize=True)
    print("wrote", path, im.size)


def draw_spectrum():
    im, d = new_screen()
    status(d, 1, "SPECTRUM", 12, 9, 1, 149)
    footer(d)
    plot_h = H - STATUS_H - FOOTER_H
    graph_h = plot_h // 2 - 48
    g24 = STATUS_H + graph_h
    g50 = g24 + 48 + graph_h
    plot_w = W - RSSI_W
    ch24_w = plot_w // 16
    ch50_w = plot_w // (len(LEGEND) - 14 + 4)
    rssi_scale(d, g24, graph_h)
    rssi_scale(d, g50, graph_h)

    def plot(ssid, ch, bw, rssi, band24):
        idx = channel_idx(ch)
        height = max(4, min(graph_h, int((rssi + 100) * graph_h / 70)))
        if band24:
            baseline, sig_w = g24, ch24_w * (4 if bw >= 40 else 2)
            offset = (idx + 2) * ch24_w
        else:
            baseline = g50
            mul = 16 if bw >= 80 else (8 if bw >= 40 else 4)
            sig_w, offset = ch50_w * mul, (idx - 14 + 4) * ch50_w
        col = CH_COLORS[idx % len(CH_COLORS)]
        half_ellipse(d, offset, baseline + 2, sig_w, height, col)
        return offset, baseline, height, col

    peaks24 = [("Office", 6, 20, -41), ("Home", 1, 20, -62), ("Mesh-2G", 11, 20, -66)]
    peaks50 = [("Home", 36, 80, -48), ("Corp", 149, 80, -52), ("Radar-AP", 100, 80, -64)]
    for ap in APS_24:
        plot(*ap, True)
    for ap in APS_50:
        plot(*ap, False)
    for ssid, ch, bw, rssi in peaks24:
        off, base, ht, col = plot(ssid, ch, bw, rssi, True)
        d.text((off - 40, max(STATUS_H + 10, base - ht - 22)), f"{ssid}  {rssi}", fill=col, font=F16)
    for ssid, ch, bw, rssi in peaks50:
        off, base, ht, col = plot(ssid, ch, bw, rssi, False)
        d.text((max(40, off - 36), max(g24 + 40, base - ht - 22)), f"{ssid}  {rssi}", fill=col, font=F16)

    d.line((0, g24, plot_w, g24), fill=WHITE, width=2)
    d.line((0, g50, plot_w, g50), fill=WHITE, width=2)
    for idx in range(14):
        ch = LEGEND[idx]
        x = (idx + 2) * ch24_w
        if ch:
            d.text((x - 6, g24 + 6), str(ch), fill=LIME if ch == 1 else CH_COLORS[idx], font=F16)
        if COUNTS_24.get(ch):
            d.text((x - 4, g24 + 26), str(COUNTS_24[ch]), fill=LIGHTGREY, font=F14)
    for idx in range(14, len(LEGEND)):
        ch = LEGEND[idx]
        x = (idx - 14 + 4) * ch50_w
        if ch:
            d.text((x - 10, g50 + 6), str(ch), fill=LIME if ch == 149 else CH_COLORS[idx % len(CH_COLORS)], font=F14)
        if COUNTS_50.get(ch):
            d.text((x - 4, g50 + 26), str(COUNTS_50[ch]), fill=LIGHTGREY, font=F14)

    d.rounded_rectangle((8, g24 + 18, 58, g24 + 42), radius=4, fill=MEDIUMBLUE)
    d.text((14, g24 + 21), "2.4", fill=WHITE, font=F16)
    d.rounded_rectangle((8, g50 + 18, 40, g50 + 42), radius=4, fill=LIMEGREEN)
    d.text((16, g50 + 21), "5", fill=WHITE, font=F16)
    save(im, "screen_spectrum.png")


def bar(d, x, y, maxw, h, val, vmax, col):
    d.rounded_rectangle((x, y, x + maxw, y + h), radius=3, outline=LIGHTGREY, width=1)
    w = int(val * maxw / max(1, vmax))
    if w >= 4:
        d.rounded_rectangle((x, y, x + w, y + h), radius=3, fill=col)


def draw_score():
    im, d = new_screen()
    status(d, 2, "SCORE", 12, 9, 1, 149)
    footer(d)
    y = STATUS_H + 18
    d.text((16, y), "Recommend  2.4: ch", fill=CYAN, font=F20)
    d.text((268, y), "1", fill=LIME, font=FB22)
    d.text((300, y), "    5: ch", fill=CYAN, font=F20)
    d.text((410, y), "149", fill=LIME, font=FB22)
    d.text((470, y), "    DFS alt 100", fill=ORANGE, font=F20)
    y += 42
    d.text((16, y), "2.4 GHz  score 1 / 6 / 11  (overlap included)", fill=WHITE, font=F18)
    y += 32
    for ch, s, n in ((1, 18, 4), (6, 72, 5), (11, 28, 3)):
        col = LIME if ch == 1 else (RED if ch == 6 else YELLOW)
        d.text((16, y + 2), f"ch {ch}", fill=col, font=F18)
        bar(d, 90, y, 620, 22, s, 72, col)
        d.text((730, y + 2), f"{s}   {n} AP", fill=WHITE, font=F18)
        y += 36
    y += 12
    d.text((16, y), "5 GHz  (U1/U3 non-DFS first; DFS marked *)", fill=WHITE, font=F18)
    y += 30
    rows = [
        (36, 22, False, False), (149, 14, False, True),
        (40, 18, False, False), (153, 4, False, False),
        (44, 9, False, False), (157, 6, False, False),
        (48, 8, False, False), (161, 3, False, False),
        (165, 2, False, False), (52, 0, True, False),
        (100, 16, True, False), (116, 2, True, False),
        (132, 0, True, False), (140, 1, True, False),
        (60, 0, True, False),
    ]
    half = (len(rows) + 1) // 2
    for r in range(half):
        for c in range(2):
            k = r + c * half
            if k >= len(rows):
                continue
            ch, s, dfs, rec = rows[k]
            x = 16 if c == 0 else 490
            yy = y + r * 28
            col = ORANGE if dfs else (LIME if rec else CYAN)
            d.text((x, yy), f"{'*' if dfs else ' '}{ch}{'>' if rec else ''}", fill=col, font=F16)
            bar(d, x + 70, yy + 4, 250, 14, s, 22, col)
            d.text((x + 330, yy), str(s), fill=WHITE, font=F16)
    d.text((16, H - FOOTER_H - 32),
           "Lower score = cleaner. Relative RSSI, not a calibrated meter.",
           fill=LIGHTGREY, font=F16)
    save(im, "screen_score.png")


def draw_list():
    im, d = new_screen()
    status(d, 3, "AP LIST", 12, 9, 1, 149)
    footer(d, list_page=True)
    d.text((16, STATUS_H + 12), "SSID                 CH    BW    RSSI    AUTH    PHY", fill=LIGHTGREY, font=F16)
    rows = [
        ("Office", 6, 20, -41, "WPA2", "ax", False, False),
        ("Home", 36, 80, -48, "WPA2", "ax", False, False),
        ("Corp", 149, 80, -52, "WPA3", "ax", False, False),
        ("Cafe-Free", 6, 20, -55, "OPEN", "g", False, True),
        ("APT-2G", 6, 40, -58, "WPA2", "n", False, False),
        ("Office-5", 40, 80, -61, "WPA2", "ax", False, False),
        ("Home", 1, 20, -62, "WPA2", "n", False, False),
        ("Radar-AP", 100, 80, -64, "WPA2", "ac", True, False),
        ("Mesh-5", 44, 40, -65, "WPA2", "ac", False, False),
        ("Mesh-2G", 11, 20, -66, "WPA2", "n", False, False),
    ]
    y = STATUS_H + 42
    for ssid, ch, bw, rssi, auth, phy, dfs, open_ap in rows:
        col = RED if open_ap else rssi_color(rssi)
        star = "*" if dfs else ""
        line = f"{ssid:<16} {ch:>4}   {bw:>3}   {rssi:>4}    {auth:<5}   {phy}{star}"
        d.text((16, y), line, fill=col, font=F18)
        y += 32
    d.text((16, H - FOOTER_H - 32), "1–10 / 21     *DFS     red = OPEN", fill=CYAN, font=F16)
    save(im, "screen_ap_list.png")


def draw_site():
    im, d = new_screen()
    status(d, 4, "SITE", 12, 9, 1, 149)
    footer(d)
    lines = [
        ("Site snapshot  (beacon survey)", CYAN, FB22),
        ("APs  total 21     2.4 GHz 12     5 GHz 9", WHITE, F20),
        ("Security  OPEN 1     WPA2 16     WPA3/mix 2     hidden 2", RED, F20),
        ("PHY  ax 8     ac 4     n 7", WHITE, F20),
        ("2.4 MHz-40 APs: 1     avoid — eats 1/6/11", ORANGE, F20),
        ("DFS APs: 1     (weather radar bands 52–144)", ORANGE, F20),
        ("Dual-band SSIDs: 3  Home", WHITE, F20),
        ("SSID on 3+ channels: 1  Office (6, 40, 149)", YELLOW, F20),
        ("Strongest  Office  ch6  20 MHz  −41 dBm  WPA2 ax", CYAN, F20),
        ("Plan:  2.4 → ch 1 (score 18)      5 → ch 149 (score 14)", LIME, F20),
        ("Not airtime/CCA. PCB antenna, uncalibrated RSSI. CSV on Serial @115200.", LIGHTGREY, F16),
    ]
    y = STATUS_H + 20
    for text, col, fnt in lines:
        d.text((20, y), text, fill=col, font=fnt)
        y += 38 if fnt != F16 else 42
    save(im, "screen_site.png")


def draw_wiring():
    ww, hh = 1100, 620
    im = Image.new("RGB", (ww, hh), (22, 22, 26))
    d = ImageDraw.Draw(im)
    d.text((32, 24), "ESP32-C5  →  ILI9341", fill=WHITE, font=FB22)
    rows = [
        ("3v3", "VCC", LIME),
        ("GND", "GND", LIGHTGREY),
        ("GPIO23", "CS", CYAN),
        ("GPIO25", "Reset", YELLOW),
        ("GPIO24", "DC/RS", ORANGE),
        ("GPIO8", "SDI/MOSI", DODGER),
        ("GPIO10", "SCK", MAGENTA),
        ("GPIO26", "LED", LIME),
    ]
    d.rounded_rectangle((40, 80, 420, 540), radius=12, outline=MEDIUMBLUE, width=3)
    d.rounded_rectangle((680, 80, 1060, 540), radius=12, outline=LIMEGREEN, width=3)
    d.text((150, 100), "ESP32-C5", fill=CYAN, font=FB22)
    d.text((800, 100), "ILI9341", fill=LIME, font=FB22)
    y = 160
    for a, b, col in rows:
        d.text((70, y), a, fill=WHITE, font=F20)
        d.line((250, y + 12, 900, y + 12), fill=col, width=3)
        d.text((920, y), b, fill=WHITE, font=F20)
        y += 42
    d.text((40, hh - 40), "MISO not used. Backlight = GPIO26.", fill=LIGHTGREY, font=F16)
    im.save(OUT / "wiring.png", "PNG", optimize=True)
    print("wrote wiring.png", im.size)


def draw_ili9341_board():
    """Labeled reference drawing of a typical 2.2/2.8 SPI ILI9341 module."""
    ww, hh = 900, 640
    im = Image.new("RGB", (ww, hh), (236, 236, 232))
    d = ImageDraw.Draw(im)
    d.text((24, 16), "ILI9341 SPI TFT  (typical 2.2\" / 2.8\" module)", fill=(30, 30, 30), font=FB22)
    # PCB
    d.rounded_rectangle((180, 70, 720, 600), radius=10, fill=(20, 90, 45), outline=(10, 50, 25), width=3)
    # glass
    d.rectangle((210, 100, 690, 460), fill=(18, 32, 70), outline=(8, 12, 24), width=2)
    d.text((340, 250), "320 × 240", fill=(180, 200, 255), font=FB22)
    d.text((355, 290), "ILI9341", fill=(140, 170, 220), font=F20)
    pins = ["VCC", "GND", "CS", "RESET", "DC/RS", "SDI/MOSI", "SCK", "LED", "SDO/MISO"]
    x0 = 230
    for i, name in enumerate(pins):
        x = x0 + i * 50
        d.rectangle((x, 480, x + 22, 560), fill=(200, 180, 40), outline=(80, 70, 10))
        d.text((x - 6, 568), name, fill=(255, 255, 220), font=F14)
    d.text((24, hh - 36), "Reference drawing — pin names as used in this project. SDO/MISO is unused.",
           fill=(70, 70, 70), font=F16)
    im.save(OUT / "ili9341-board.png", "PNG", optimize=True)
    print("wrote ili9341-board.png", im.size)


if __name__ == "__main__":
    draw_spectrum()
    draw_score()
    draw_list()
    draw_site()
    draw_wiring()
    # ili9341-board.png is a real LCDwiki product photo, not generated here
