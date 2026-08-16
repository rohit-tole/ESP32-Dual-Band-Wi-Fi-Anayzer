#!/usr/bin/env python3
"""Render ILI9341 320x240 mockups of ESP32C5_WiFi_Planner screens, then scale 3x."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

W, H = 320, 240
STATUS_H, FOOTER_H, RSSI_W = 16, 12, 36
SCALE = 3
OUT = Path(__file__).resolve().parent

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
CYAN = (0, 255, 255)
LIME = (0, 255, 0)
YELLOW = (255, 255, 0)
RED = (255, 0, 0)
ORANGE = (255, 165, 0)
MAGENTA = (255, 0, 255)
LIGHTGREY = (192, 192, 192)
MEDIUMBLUE = (0, 0, 180)
LIMEGREEN = (50, 205, 50)
DODGER = (30, 144, 255)

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


def fonts():
    candidates = [
        r"C:\Windows\Fonts\consola.ttf",
        r"C:\Windows\Fonts\cour.ttf",
        r"C:\Windows\Fonts\lucon.ttf",
        r"C:\Windows\Fonts\tahoma.ttf",
    ]
    path = next((p for p in candidates if Path(p).exists()), None)

    def f(size):
        if path:
            return ImageFont.truetype(path, size)
        return ImageFont.load_default()

    return f(8), f(9), f(10)


F5, F6, F8 = fonts()


def new_screen():
    im = Image.new("RGB", (W, H), BLACK)
    return im, ImageDraw.Draw(im)


def status(d, page, name, n24, n50, rec24, rec50):
    d.rectangle((0, 0, W, STATUS_H), fill=MEDIUMBLUE)
    d.text((2, 4), f"{page}/4 {name}  2.4:{n24}  5:{n50}  3s  use {rec24}/{rec50}",
           fill=WHITE, font=F5)


def footer(d, list_page=False):
    d.rectangle((0, H - FOOTER_H, W, H), fill=MEDIUMBLUE)
    msg = "BOOT short: next screen   hold: scroll list" if list_page else \
          "BOOT short: next screen   hold: CSV on Serial"
    d.text((2, H - 10), msg, fill=CYAN, font=F5)


def half_ellipse(d, cx, baseline, rx, ry, color):
    if rx < 1:
        rx = 1
    if ry < 1:
        ry = 1
    box = [cx - rx, baseline - ry, cx + rx, baseline + ry]
    d.pieslice(box, 180, 360, fill=color, outline=color)


def rssi_scale(d, baseline, graph_h):
    x = W - RSSI_W
    for rssi in (-30, -50, -70, -90):
        y = baseline - int((rssi - (-100)) * (graph_h - 1) / ((-30) - (-100)))
        c = rssi_color(rssi)
        d.line((x, y, x + 6, y), fill=c)
        d.text((x + 8, y - 4), str(rssi), fill=c, font=F5)


# Sample survey
APS_24 = [
    ("Office", 6, 20, -41),
    ("Cafe-Free", 6, 20, -55),
    ("Home", 1, 20, -62),
    ("IoT-Hub", 1, 20, -78),
    ("Neighbor", 3, 20, -71),
    ("Guest", 11, 20, -70),
    ("Printer", 11, 20, -81),
    ("Shop", 6, 20, -68),
    ("APT-2G", 6, 40, -58),
    ("Cam-01", 1, 20, -84),
    ("Mesh-2G", 11, 20, -66),
    ("Lab", 6, 20, -73),
]
APS_50 = [
    ("Home", 36, 80, -48),
    ("Corp", 149, 80, -52),
    ("Mesh-5", 44, 40, -65),
    ("Office-5", 40, 80, -61),
    ("Guest5", 149, 40, -69),
    ("APT-5", 157, 20, -72),
    ("IoT-5", 36, 20, -77),
    ("Radar-AP", 100, 80, -64),
    ("U3-Back", 161, 20, -80),
]
COUNTS_24 = {1: 4, 3: 1, 6: 5, 11: 3}
COUNTS_50 = {36: 2, 40: 1, 44: 1, 100: 1, 149: 2, 157: 1, 161: 1}


def save(im, name):
    big = im.resize((W * SCALE, H * SCALE), Image.NEAREST)
    path = OUT / name
    big.save(path, "PNG")
    print("wrote", path)
    return path


def draw_spectrum():
    im, d = new_screen()
    status(d, 1, "SPECTRUM", 12, 9, 1, 149)
    footer(d)
    plot_h = H - STATUS_H - FOOTER_H
    graph_h = plot_h // 2 - 16
    g24 = STATUS_H + graph_h
    g50 = g24 + 16 + graph_h
    plot_w = W - RSSI_W
    ch24_w = plot_w // (14 + 2)
    ch50_w = plot_w // (len(LEGEND) - 14 + 4)

    d.rectangle((0, STATUS_H, W, H - FOOTER_H), fill=BLACK)
    rssi_scale(d, g24, graph_h)
    rssi_scale(d, g50, graph_h)

    def plot(ssid, ch, bw, rssi, band24):
        idx = channel_idx(ch)
        height = max(1, min(graph_h, int((rssi - (-100)) * graph_h / 70)))
        if band24:
            baseline, sig_w = g24, ch24_w * (4 if bw >= 40 else 2)
            offset = (idx + 2) * ch24_w
        else:
            baseline = g50
            mul = 16 if bw >= 80 else (8 if bw >= 40 else 4)
            sig_w = ch50_w * mul
            offset = (idx - 14 + 4) * ch50_w
        col = CH_COLORS[idx % len(CH_COLORS)]
        half_ellipse(d, offset, baseline + 1, sig_w, height, col)
        return offset, baseline, height, col, idx

    peaks24 = [("Office", 6, 20, -41), ("Home", 1, 20, -62), ("Mesh-2G", 11, 20, -66)]
    peaks50 = [("Home", 36, 80, -48), ("Corp", 149, 80, -52), ("Radar-AP", 100, 80, -64)]
    for ap in APS_24:
        plot(*ap, True)
    for ap in APS_50:
        plot(*ap, False)
    for ssid, ch, bw, rssi in peaks24:
        off, base, ht, col, _ = plot(ssid, ch, bw, rssi, True)
        y = max(STATUS_H + 8, base - ht - 8)
        d.text((off - 18, y), f"{ssid} {rssi}", fill=col, font=F5)
    for ssid, ch, bw, rssi in peaks50:
        off, base, ht, col, _ = plot(ssid, ch, bw, rssi, False)
        y = max(g24 + 20, base - ht - 8)
        d.text((max(20, off - 16), y), f"{ssid} {rssi}", fill=col, font=F5)

    d.line((0, g24, plot_w, g24), fill=WHITE)
    d.line((0, g50, plot_w, g50), fill=WHITE)
    for idx in range(14):
        ch = LEGEND[idx]
        x = (idx + 2) * ch24_w
        if ch:
            d.text((x - (2 if ch < 10 else 4), g24 + 2), str(ch),
                   fill=LIME if ch == 1 else CH_COLORS[idx], font=F5)
        if COUNTS_24.get(ch):
            d.text((x - 2, g24 + 9), str(COUNTS_24[ch]), fill=LIGHTGREY, font=F5)
    for idx in range(14, len(LEGEND)):
        ch = LEGEND[idx]
        x = (idx - 14 + 4) * ch50_w
        if ch:
            d.text((x - (4 if ch < 100 else 5), g50 + 2), str(ch),
                   fill=LIME if ch == 149 else CH_COLORS[idx % len(CH_COLORS)], font=F5)
        if COUNTS_50.get(ch):
            d.text((x - 2, g50 + 9), str(COUNTS_50[ch]), fill=LIGHTGREY, font=F5)

    d.rectangle((2, g24 + 6, 22, g24 + 15), fill=MEDIUMBLUE)
    d.text((3, g24 + 7), "2.4", fill=WHITE, font=F5)
    d.rectangle((2, g50 + 6, 14, g50 + 15), fill=LIMEGREEN)
    d.text((4, g50 + 7), "5", fill=WHITE, font=F5)
    save(im, "screen_spectrum.png")


def bar(d, x, y, maxw, val, vmax, col):
    d.rectangle((x, y, x + maxw, y + 8), outline=LIGHTGREY)
    w = int(val * maxw / max(1, vmax))
    if w:
        d.rectangle((x, y, x + w, y + 8), fill=col)


def draw_score():
    im, d = new_screen()
    status(d, 2, "SCORE", 12, 9, 1, 149)
    footer(d)
    d.rectangle((0, STATUS_H, W, H - FOOTER_H), fill=BLACK)
    y = STATUS_H + 6
    d.text((4, y), "Recommend  2.4: ch", fill=CYAN, font=F6)
    d.text((118, y), "1", fill=LIME, font=F6)
    d.text((132, y), "   5: ch", fill=CYAN, font=F6)
    d.text((186, y), "149", fill=LIME, font=F6)
    d.text((210, y), "  DFS alt 100", fill=ORANGE, font=F6)
    y += 14
    d.text((4, y), "2.4 GHz  score 1 / 6 / 11  (overlap included)", fill=WHITE, font=F5)
    scores24 = [(1, 18, 4), (6, 72, 5), (11, 28, 3)]
    y += 8
    for ch, s, n in scores24:
        col = LIME if ch == 1 else (RED if ch == 6 else YELLOW)
        d.text((4, y + 1), f"ch{ch:2d}", fill=col, font=F6)
        bar(d, 32, y, 200, s, 72, col)
        d.text((238, y + 1), f"{s}  {n}AP", fill=WHITE, font=F5)
        y += 12
    y += 4
    d.text((4, y), "5 GHz  (U1/U3 non-DFS first; DFS marked *)", fill=WHITE, font=F5)
    y += 10
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
    vmax5 = 22
    col0, col1, row_h = 4, 164, 10
    n = len(rows)
    half = (n + 1) // 2
    for r in range(half):
        for c in range(2):
            k = r + c * half
            if k >= n:
                continue
            ch, s, dfs, rec = rows[k]
            x = col0 if c == 0 else col1
            yy = y + r * row_h
            col = ORANGE if dfs else (LIME if rec else CYAN)
            mark = "*" if dfs else " "
            arrow = ">" if rec else " "
            d.text((x, yy), f"{mark}{ch:3d}{arrow}", fill=col, font=F5)
            bar(d, x + 28, yy + 1, 88, s, vmax5, col)
            d.text((x + 118, yy), str(s), fill=WHITE, font=F5)
    d.text((4, H - FOOTER_H - 10),
           "Lower score = cleaner. Relative RSSI, not a calibrated meter.",
           fill=LIGHTGREY, font=F5)
    save(im, "screen_score.png")


def draw_list():
    im, d = new_screen()
    status(d, 3, "AP LIST", 12, 9, 1, 149)
    footer(d, list_page=True)
    d.rectangle((0, STATUS_H, W, H - FOOTER_H), fill=BLACK)
    d.text((2, STATUS_H + 2), "SSID              CH BW  RSSI AUTH PHY", fill=LIGHTGREY, font=F5)
    rows = [
        ("Office          ", 6, 20, -41, "WPA2", "ax", False, False),
        ("Home            ", 36, 80, -48, "WPA2", "ax", False, False),
        ("Corp            ", 149, 80, -52, "WPA3", "ax", False, False),
        ("Cafe-Free       ", 6, 20, -55, "OPEN", "g", False, True),
        ("APT-2G          ", 6, 40, -58, "WPA2", "n", False, False),
        ("Office-5        ", 40, 80, -61, "WPA2", "ax", False, False),
        ("Home            ", 1, 20, -62, "WPA2", "n", False, False),
        ("Radar-AP        ", 100, 80, -64, "WPA2", "ac", True, False),
        ("Mesh-5          ", 44, 40, -65, "WPA2", "ac", False, False),
        ("Mesh-2G         ", 11, 20, -66, "WPA2", "n", False, False),
    ]
    y = STATUS_H + 14
    for ssid, ch, bw, rssi, auth, phy, dfs, open_ap in rows:
        col = RED if open_ap else rssi_color(rssi)
        star = "*" if dfs else ""
        line = f"{ssid} {ch:3d} {bw:2d} {rssi:4d} {auth:<5} {phy}{star}"
        d.text((2, y), line, fill=col, font=F5)
        y += 10
    d.text((2, H - FOOTER_H - 10), "1-10 / 21   *DFS   red=OPEN", fill=CYAN, font=F5)
    save(im, "screen_ap_list.png")


def draw_site():
    im, d = new_screen()
    status(d, 4, "SITE", 12, 9, 1, 149)
    footer(d)
    d.rectangle((0, STATUS_H, W, H - FOOTER_H), fill=BLACK)
    lines = [
        ("Site snapshot  (beacon survey)", CYAN),
        ("APs  total 21   2.4 GHz 12   5 GHz 9", WHITE),
        ("Security  OPEN 1   WPA2 16   WPA3/mix 2   hidden 2", RED),
        ("PHY  ax 8   ac 4   n 7", WHITE),
        ("2.4 MHz-40 APs: 1   avoid — eats 1/6/11", ORANGE),
        ("DFS APs: 1   (weather radar bands 52-144)", ORANGE),
        ("Dual-band SSIDs: 3  Home", WHITE),
        ("SSID on 3+ channels: 1  Office (6,40,149)", YELLOW),
        ("Strongest  Office  ch6 20MHz  -41 dBm  WPA2 ax", CYAN),
        ("Plan: 2.4 -> ch 1 (score 18)    5 -> ch 149 (score 14)", LIME),
        ("Not airtime/CCA. PCB antenna, uncalibrated RSSI. CSV on Serial @115200.", LIGHTGREY),
    ]
    y = STATUS_H + 4
    for text, col in lines:
        d.text((4, y), text, fill=col, font=F5)
        y += 11 if col != LIGHTGREY else 14
    save(im, "screen_site.png")


def draw_wiring():
    """Simple pin map, not a fake photo of hardware."""
    ww, hh = 640, 360
    im = Image.new("RGB", (ww, hh), (18, 18, 22))
    d = ImageDraw.Draw(im)
    title, body = F8, F6
    try:
        title = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 18)
        body = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 14)
    except OSError:
        pass
    d.text((20, 16), "ESP32-C5  →  ILI9341", fill=WHITE, font=title)
    rows = [
        ("3V3", "VCC", LIME),
        ("GND", "GND", LIGHTGREY),
        ("GPIO23", "CS", CYAN),
        ("GPIO25", "Reset", YELLOW),
        ("GPIO24", "DC/RS", ORANGE),
        ("GPIO8", "SDI/MOSI", DODGER),
        ("GPIO10", "SCK", MAGENTA),
        ("GPIO26", "LED", LIME),
    ]
    y = 56
    d.rounded_rectangle((24, 48, 280, 330), radius=8, outline=MEDIUMBLUE, width=2)
    d.rounded_rectangle((360, 48, 616, 330), radius=8, outline=LIMEGREEN, width=2)
    d.text((90, 58), "ESP32-C5", fill=CYAN, font=body)
    d.text((430, 58), "ILI9341", fill=LIME, font=body)
    y = 92
    for a, b, col in rows:
        d.text((48, y), a, fill=WHITE, font=body)
        d.line((160, y + 8, 500, y + 8), fill=col, width=2)
        d.text((520, y), b, fill=WHITE, font=body)
        y += 28
    d.text((24, hh - 28), "MISO not used. Backlight = GPIO26.", fill=LIGHTGREY, font=F6)
    path = OUT / "wiring.png"
    im.save(path, "PNG")
    print("wrote", path)


if __name__ == "__main__":
    draw_spectrum()
    draw_score()
    draw_list()
    draw_site()
    draw_wiring()
