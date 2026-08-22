/*******************************************************************************
 * Dual Band Wi-Fi Analyser v2.0
 *
 * Arduino IDE:
 *   Board: ESP32 -> ESP32C5 Dev Module  (Arduino-ESP32 3.3.1+)
 *   Flash size: 16MB
 *   Upload speed: 921600
 *   Partition: Huge APP (3MB no OTA)
 *
 * Display wiring (ILI9341) — same as Readme.txt:
 *   ESP32-C5   ILI9341
 *   3V3     -> VCC
 *   GND     -> GND
 *   GPIO23  -> CS
 *   GPIO25  -> Reset
 *   GPIO24  -> DC/RS
 *   GPIO8   -> SDI/MOSI
 *   GPIO10  -> SCK
 *   GPIO26  -> LED (backlight)
 *
 * BOOT button (GPIO28 on ESP32-C5-DevKitC-1):
 *   Short press  -> next screen
 *   Long press   -> AP list: scroll   |  other screens: reprint CSV on Serial
 *
 * Screens:  1 Spectrum  2 Channel score  3 AP list  4 Site snapshot
 *
 * Beacon/probe survey (not airtime). RSSI is relative (PCB antenna).
 ******************************************************************************/

#if !CONFIG_SOC_WIFI_SUPPORT_5G
#error This sketch requires a dual-band chip (ESP32-C5).
#endif

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>

/*******************************************************************************
 * Pins
 ******************************************************************************/
#define TFT_CS   23
#define TFT_DC   24
#define TFT_RST  25
#define TFT_MOSI 8
#define TFT_SCK  10
#define TFT_BL   26

#define BTN_BOOT 28  // DevKitC-1 BOOT (active low)
#define BTN_ALT  0   // optional extra button (active low, internal pull-up)

#define SCAN_INTERVAL_MS     3000
#define SCAN_DWELL_MS        400
#define SCAN_ACTIVE_MIN_MS   120
#define MAX_APS              64
#define LIST_ROWS            10
#define LONG_PRESS_MS        550
#define HOLD_REPEAT_MS       380
#define BL_PWM               210  // 0–255

/*******************************************************************************
 * Display
 ******************************************************************************/
/* Default C5 bus is CS=23 DC=24 SCK=10 MOSI=8 — matches Readme.txt */
Arduino_DataBus *bus = create_default_Arduino_DataBus();
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 1 /* landscape */, false);

/*******************************************************************************
 * Layout
 ******************************************************************************/
static int16_t W, H;
static const int16_t STATUS_H = 16;
static const int16_t FOOTER_H = 12;
static const int16_t RSSI_SCALE_W = 36;

#define RSSI_CEILING   -30
#define RSSI_SHOW_SSID -70
#define RSSI_FLOOR     -100

/*******************************************************************************
 * Channel legend (2.4 then 5 GHz slots) + colors
 ******************************************************************************/
static const uint8_t channel_legend[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0,
  0, 0, 36, 0, 0, 0, 44, 0, 0, 0, 52, 0, 0, 0,
  60, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 100, 0, 0, 0, 108, 0, 0, 0, 116, 0, 0, 0, 124,
  0, 0, 0, 132, 0, 0, 0, 140, 0, 0, 0, 149, 0, 0,
  0, 157, 0, 0, 0, 165, 0, 0, 0, 173, 0, 0, 0
};

static const uint16_t channel_color[] = {
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_WHITE, RGB565_CYAN, RGB565_WHITE, RGB565_WHITE,
  RGB565_WHITE, RGB565_WHITE, RGB565_WHITE, RGB565_WHITE, RGB565_WHITE, RGB565_WHITE, RGB565_MAGENTA,
  RGB565_WHITE, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_WHITE, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_WHITE, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_DODGERBLUE, RGB565_MAGENTA,
  RGB565_RED, RGB565_ORANGE, RGB565_YELLOW, RGB565_LIME, RGB565_CYAN, RGB565_WHITE
};

static const int LEGEND_N = (int)sizeof(channel_legend);

/*******************************************************************************
 * Scan store
 ******************************************************************************/
static wifi_ap_record_t aps[MAX_APS];
static uint16_t ap_n = 0;
static bool have_scan = false;
static bool scan_stale = false;
static bool scanning = false;
static uint32_t last_scan_ms = 0;
static uint32_t scan_started_ms = 0;

static uint16_t ap24_count = 0, ap50_count = 0;
static uint8_t ap_count_list[sizeof(channel_legend)];
static int32_t peak_rssi[sizeof(channel_legend)];
static int16_t peak_id[sizeof(channel_legend)];
static int score_idx[sizeof(channel_legend)];
static int rec24 = 1, rec50 = 149;
static int rec24_score = 0, rec50_score = 0;
static int rec50_dfs = 0;  // best DFS alternative (0 = none)

enum Page : uint8_t { PAGE_SPECTRUM = 0, PAGE_SCORE, PAGE_LIST, PAGE_SITE, PAGE_COUNT };
static uint8_t page = PAGE_SPECTRUM;
static int list_offset = 0;
static uint16_t sorted_idx[MAX_APS];
static bool dirty = true;
static bool csv_header_done = false;

/*******************************************************************************
 * Helpers
 ******************************************************************************/
static uint16_t channelIdx(int ch)
{
  if (ch <= 14)  return (uint16_t)(ch - 1);
  if (ch <= 68)  return (uint16_t)(14 + ((ch - 32) / 2));
  if (ch <= 144) return (uint16_t)(41 + ((ch - 96) / 2));
  if (ch <= 177) return (uint16_t)(67 + ((ch - 149) / 2));
  return 82;
}

static bool is24(int ch) { return ch > 0 && ch <= 14; }
static bool isDfs(int ch) { return ch >= 52 && ch <= 144; }

static int bwMHz(wifi_bandwidth_t bw)
{
  switch ((int)bw) {
    case WIFI_BW_HT20: return 20;
    case WIFI_BW_HT40: return 40;
#ifdef WIFI_BW80
    case WIFI_BW80: return 80;
#endif
#ifdef WIFI_BW160
    case WIFI_BW160: return 160;
#endif
#ifdef WIFI_BW80_BW80
    case WIFI_BW80_BW80: return 80;
#endif
    default: return 20;
  }
}

static uint16_t rssiColor(int rssi)
{
  if (rssi >= -50) return RGB565_LIME;
  if (rssi >= -70) return RGB565_YELLOW;
  return RGB565_RED;
}

static int rssiWeight(int rssi)
{
  if (rssi < -90) return 1;
  if (rssi < -80) return 2;
  if (rssi < -70) return 4;
  if (rssi < -60) return 7;
  if (rssi < -50) return 11;
  return 16;
}

static const char *ssidOf(const wifi_ap_record_t *r)
{
  if (r->ssid[0] == 0) return nullptr;
  return (const char *)r->ssid;
}

static const char *authStr(wifi_auth_mode_t a)
{
  switch (a) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA12";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA23";
#ifdef WIFI_AUTH_WAPI_PSK
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
#ifdef WIFI_AUTH_WPA3_ENTERPRISE
    case WIFI_AUTH_WPA3_ENTERPRISE: return "ENT3";
#endif
#ifdef WIFI_AUTH_WPA3_ENT_192
    case WIFI_AUTH_WPA3_ENT_192:    return "ENT3";
#endif
#ifdef WIFI_AUTH_OWE
    case WIFI_AUTH_OWE:             return "OWE";
#endif
#ifdef WIFI_AUTH_WPA3_EXT_PSK
    case WIFI_AUTH_WPA3_EXT_PSK:    return "WPA3";
#endif
#ifdef WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return "WPA23";
#endif
#ifdef WIFI_AUTH_DPP
    case WIFI_AUTH_DPP:             return "DPP";
#endif
    default:                        return "?";
  }
}

static const char *phyStr(const wifi_ap_record_t *r)
{
  if (r->phy_11ax) return "ax";
  if (r->phy_11ac) return "ac";
  if (r->phy_11n)  return "n";
  if (r->phy_11a)  return "a";
  if (r->phy_11g)  return "g";
  if (r->phy_11b)  return "b";
  return "-";
}

static const char *uniiStr(int ch)
{
  if (is24(ch)) return "2.4";
  if (ch >= 36 && ch <= 48) return "U1";
  if (ch >= 52 && ch <= 64) return "U2";
  if (ch >= 100 && ch <= 144) return "U2e";
  if (ch >= 149 && ch <= 177) return "U3";
  return "?";
}

static bool sameBssid(const uint8_t *a, const uint8_t *b)
{
  return memcmp(a, b, 6) == 0;
}

static bool buttonDown()
{
  return digitalRead(BTN_BOOT) == LOW || digitalRead(BTN_ALT) == LOW;
}

static void printFit(int16_t x, int16_t y, int16_t maxw, const char *s)
{
  if (!s) s = "";
  int16_t cw = 6;
  int n = maxw / cw;
  if (n < 1) n = 1;
  char buf[40];
  int len = (int)strlen(s);
  if (len > n) len = n;
  memcpy(buf, s, len);
  buf[len] = 0;
  gfx->setCursor(x, y);
  gfx->print(buf);
}

static void bssidShort(const uint8_t *b, char *out, size_t n)
{
  snprintf(out, n, "%02X%02X%02X", b[3], b[4], b[5]);
}

/*******************************************************************************
 * Occupancy / scoring
 ******************************************************************************/
static const uint8_t ch80_blocks[][4] = {
  {36, 40, 44, 48},
  {52, 56, 60, 64},
  {100, 104, 108, 112},
  {116, 120, 124, 128},
  {132, 136, 140, 144},
  {149, 153, 157, 161}
};

static void addScoreCh(int ch, int w)
{
  if (ch <= 0) return;
  uint16_t idx = channelIdx(ch);
  if (idx >= LEGEND_N) return;
  score_idx[idx] += w;
}

static void addOccupy5(int primary, int bw, wifi_second_chan_t second, int w)
{
  if (bw >= 80) {
    for (unsigned b = 0; b < sizeof(ch80_blocks) / sizeof(ch80_blocks[0]); b++) {
      bool in = false;
      for (int i = 0; i < 4; i++) if (ch80_blocks[b][i] == primary) in = true;
      if (in) {
        for (int i = 0; i < 4; i++) addScoreCh(ch80_blocks[b][i], w);
        return;
      }
    }
  }
  addScoreCh(primary, w);
  if (bw >= 40) {
    if (second == WIFI_SECOND_CHAN_ABOVE) addScoreCh(primary + 4, w);
    else if (second == WIFI_SECOND_CHAN_BELOW) addScoreCh(primary - 4, w);
    else addScoreCh(primary + 4, w / 2);
  }
}

static void addOccupy24(int primary, int bw, wifi_second_chan_t second, int w)
{
  const int targets[] = {1, 6, 11};
  int extra = (second == WIFI_SECOND_CHAN_ABOVE) ? primary + 4
            : (second == WIFI_SECOND_CHAN_BELOW) ? primary - 4 : 0;
  for (int t : targets) {
    int d = abs(primary - t);
    int d2 = extra ? abs(extra - t) : 99;
    if (d2 < d) d = d2;
    if (d == 0) addScoreCh(t, w * 4);
    else if (d <= 2) addScoreCh(t, w * 2);
    else if (d <= 4) addScoreCh(t, w);
    else if (bw >= 40 && d <= 6) addScoreCh(t, w / 2);
  }
}

static int cmpRssiDesc(const void *a, const void *b)
{
  uint16_t ia = *(const uint16_t *)a;
  uint16_t ib = *(const uint16_t *)b;
  return (int)aps[ib].rssi - (int)aps[ia].rssi;
}

static void recompute()
{
  memset(ap_count_list, 0, sizeof(ap_count_list));
  memset(score_idx, 0, sizeof(score_idx));
  ap24_count = 0;
  ap50_count = 0;
  for (int i = 0; i < LEGEND_N; i++) {
    peak_rssi[i] = RSSI_FLOOR;
    peak_id[i] = -1;
  }

  for (uint16_t i = 0; i < ap_n; i++) {
    int ch = aps[i].primary;
    uint16_t idx = channelIdx(ch);
    if (idx >= LEGEND_N) continue;
    int rssi = aps[i].rssi;
    if (rssi > peak_rssi[idx]) {
      peak_rssi[idx] = rssi;
      peak_id[idx] = (int16_t)i;
    }

    bool dup = false;
    for (uint16_t j = 0; j < i; j++) {
      if (aps[j].primary == ch && sameBssid(aps[j].bssid, aps[i].bssid)) {
        dup = true;
        break;
      }
    }
    if (dup) continue;

    ap_count_list[idx]++;
    int bw = bwMHz(aps[i].bandwidth);
    int w = rssiWeight(rssi);
    if (is24(ch)) {
      ap24_count++;
      addOccupy24(ch, bw, aps[i].second, w);
    } else {
      ap50_count++;
      addOccupy5(ch, bw, aps[i].second, w);
    }
  }

  rec24 = 1;
  rec24_score = score_idx[channelIdx(1)];
  for (int ch : {6, 11}) {
    int s = score_idx[channelIdx(ch)];
    if (s < rec24_score) {
      rec24_score = s;
      rec24 = ch;
    }
  }

  rec50 = 36;
  rec50_score = 100000;
  rec50_dfs = 0;
  int best_dfs = 100000;
  int best_dfs_ch = 0;
  static const int cand5[] = {
    36, 40, 44, 48, 149, 153, 157, 161, 165,
    52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 132, 136, 140
  };
  for (int ch : cand5) {
    int s = score_idx[channelIdx(ch)];
    if (isDfs(ch)) {
      if (s < best_dfs) {
        best_dfs = s;
        best_dfs_ch = ch;
      }
    } else if (s < rec50_score) {
      rec50_score = s;
      rec50 = ch;
    }
  }
  rec50_dfs = (best_dfs_ch && best_dfs_ch != rec50) ? best_dfs_ch : 0;

  for (uint16_t i = 0; i < ap_n; i++) sorted_idx[i] = i;
  if (ap_n) qsort(sorted_idx, ap_n, sizeof(uint16_t), cmpRssiDesc);

  int max_off = (int)ap_n - LIST_ROWS;
  if (max_off < 0) max_off = 0;
  if (list_offset > max_off) list_offset = max_off;
}

/*******************************************************************************
 * Serial CSV
 ******************************************************************************/
static void dumpCsv()
{
  if (!csv_header_done) {
    Serial.println(F("bssid,ssid,band,channel,bw_mhz,rssi,auth,phy,dfs,unii"));
    csv_header_done = true;
  }
  for (uint16_t i = 0; i < ap_n; i++) {
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             aps[i].bssid[0], aps[i].bssid[1], aps[i].bssid[2],
             aps[i].bssid[3], aps[i].bssid[4], aps[i].bssid[5]);
    const char *ss = ssidOf(&aps[i]);
    Serial.printf("%s,\"%s\",%s,%u,%d,%d,%s,%s,%s,%s\n",
                  mac,
                  ss ? ss : "*",
                  is24(aps[i].primary) ? "2.4" : "5",
                  aps[i].primary,
                  bwMHz(aps[i].bandwidth),
                  aps[i].rssi,
                  authStr(aps[i].authmode),
                  phyStr(&aps[i]),
                  isDfs(aps[i].primary) ? "1" : "0",
                  uniiStr(aps[i].primary));
  }
  Serial.printf("# recommend 2.4=ch%d score=%d  5=ch%d score=%d  dfs_alt=%d  n=%u\n",
                rec24, rec24_score, rec50, rec50_score, rec50_dfs, ap_n);
}

/*******************************************************************************
 * Scan
 ******************************************************************************/
static void startScan()
{
  if (scanning) return;
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
  int16_t r = WiFi.scanNetworks(true /* async */, true /* hidden */, false /* active */, SCAN_DWELL_MS);
  if (r == WIFI_SCAN_FAILED) {
    Serial.println(F("scan start failed"));
    return;
  }
  scanning = true;
  scan_started_ms = millis();
  dirty = true;
}

static void pollScan()
{
  if (!scanning) {
    if (millis() - last_scan_ms >= SCAN_INTERVAL_MS) startScan();
    return;
  }

  int16_t st = WiFi.scanComplete();
  if (st == WIFI_SCAN_RUNNING) return;

  scanning = false;
  last_scan_ms = millis();

  if (st > 0) {
    uint16_t n = (st > MAX_APS) ? MAX_APS : (uint16_t)st;
    uint16_t copied = 0;
    for (uint16_t i = 0; i < (uint16_t)st && copied < MAX_APS; i++) {
      wifi_ap_record_t *src = (wifi_ap_record_t *)WiFi.getScanInfoByIndex(i);
      if (!src) continue;
      memcpy(&aps[copied], src, sizeof(wifi_ap_record_t));
      copied++;
    }
    ap_n = copied;
    have_scan = true;
    scan_stale = false;
    WiFi.scanDelete();
    recompute();
    dumpCsv();
    dirty = true;
  } else {
    WiFi.scanDelete();
    if (have_scan) scan_stale = true;
    dirty = true;
  }
}

/*******************************************************************************
 * Buttons
 ******************************************************************************/
static bool btn_last = false;
static uint32_t btn_t0 = 0;
static bool btn_long = false;

static void nextPage()
{
  page = (uint8_t)((page + 1) % PAGE_COUNT);
  dirty = true;
}

static void onLongPress()
{
  if (page == PAGE_LIST) {
    int max_off = (int)ap_n - LIST_ROWS;
    if (max_off < 0) max_off = 0;
    list_offset += LIST_ROWS;
    if (list_offset > max_off) list_offset = 0;
    dirty = true;
  } else {
    dumpCsv();
  }
}

static void handleButtons()
{
  bool down = buttonDown();
  uint32_t now = millis();
  if (down && !btn_last) {
    btn_t0 = now;
    btn_long = false;
  } else if (down && btn_last) {
    if (!btn_long && (now - btn_t0 >= LONG_PRESS_MS)) {
      btn_long = true;
      onLongPress();
      btn_t0 = now;
    } else if (btn_long && page == PAGE_LIST && (now - btn_t0 >= HOLD_REPEAT_MS)) {
      onLongPress();
      btn_t0 = now;
    }
  } else if (!down && btn_last) {
    if (!btn_long) nextPage();
  }
  btn_last = down;
}

/*******************************************************************************
 * Chrome
 ******************************************************************************/
static const char *pageName()
{
  switch (page) {
    case PAGE_SPECTRUM: return "SPECTRUM";
    case PAGE_SCORE:    return "SCORE";
    case PAGE_LIST:     return "AP LIST";
    default:            return "SITE";
  }
}

static void drawStatusBar()
{
  gfx->fillRect(0, 0, W, STATUS_H, RGB565_MEDIUMBLUE);
  gfx->setFont(u8g2_font_5x7_tr);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(2, 11);

  uint32_t age = have_scan ? (millis() - last_scan_ms) / 1000 : 0;
  char line[64];
  snprintf(line, sizeof(line), "%d/4 %s  2.4:%u  5:%u  %s%lus  %s%d/%d",
           page + 1, pageName(),
           ap24_count, ap50_count,
           scan_stale ? "HOLD " : (scanning ? "SCAN " : ""),
           (unsigned long)age,
           scan_stale ? "old " : "use ",
           rec24, rec50);
  gfx->print(line);
}

static void drawFooter()
{
  gfx->fillRect(0, H - FOOTER_H, W, FOOTER_H, RGB565_MEDIUMBLUE);
  gfx->setFont(u8g2_font_04b_03_tr);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(2, H - 3);
  if (page == PAGE_LIST)
    gfx->print(F("BOOT short: next screen   hold: scroll list"));
  else
    gfx->print(F("BOOT short: next screen   hold: CSV on Serial"));
}

/*******************************************************************************
 * Page 1 — Spectrum
 ******************************************************************************/
static void drawRSSIScale(int16_t baseline, int16_t graph_h)
{
  int x = W - RSSI_SCALE_W;
  const int marks[] = {-30, -50, -70, -90};
  gfx->setFont(u8g2_font_04b_03_tr);
  for (int i = 0; i < 4; i++) {
    int rssi = marks[i];
    int y = baseline - map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_h);
    uint16_t c = rssiColor(rssi);
    gfx->drawFastHLine(x, y, 6, c);
    gfx->setTextColor(c);
    gfx->setCursor(x + 8, y + 2);
    gfx->print(rssi);
  }
}

static void drawSpectrum()
{
  int plot_h = H - STATUS_H - FOOTER_H;
  int graph_h = (plot_h / 2) - 16;
  int g24 = STATUS_H + graph_h;
  int g50 = g24 + 16 + graph_h;
  int plot_w = W - RSSI_SCALE_W;
  int ch24_w = plot_w / (14 + 2);
  int ch50_w = plot_w / (LEGEND_N - 14 + 4);

  gfx->fillRect(0, STATUS_H, W, plot_h, RGB565_BLACK);
  drawRSSIScale(g24, graph_h);
  drawRSSIScale(g50, graph_h);

  if (!have_scan) {
    gfx->setFont(u8g2_font_helvR08_tr);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(8, STATUS_H + 40);
    gfx->print(F("Scanning dual-band..."));
    return;
  }

  /* occupancy ellipses */
  for (uint16_t i = 0; i < ap_n; i++) {
    int ch = aps[i].primary;
    uint16_t idx = channelIdx(ch);
    if (idx >= LEGEND_N) continue;
    int rssi = aps[i].rssi;
    if (rssi < RSSI_FLOOR) rssi = RSSI_FLOOR;
    int height = constrain(map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_h), 1, graph_h);
    int bw = bwMHz(aps[i].bandwidth);
    int baseline, sig_w, offset;
    if (idx < 14) {
      baseline = g24;
      sig_w = ch24_w * ((bw >= 40) ? 4 : 2);
      offset = (idx + 2) * ch24_w;
    } else {
      baseline = g50;
      int mul = 4;
      if (bw >= 80) mul = 16;
      else if (bw >= 40) mul = 8;
      sig_w = ch50_w * mul;
      offset = (idx - 14 + 4) * ch50_w;
    }
    gfx->startWrite();
    gfx->writeEllipseHelper(offset, baseline + 1, sig_w, height, 0b0011, channel_color[idx]);
    gfx->endWrite();
  }

  /* top 3 labels per band */
  int shown24 = 0, shown50 = 0;
  for (uint16_t s = 0; s < ap_n; s++) {
    uint16_t i = sorted_idx[s];
    int ch = aps[i].primary;
    uint16_t idx = channelIdx(ch);
    if (peak_id[idx] != (int16_t)i) continue;
    if (aps[i].rssi < RSSI_SHOW_SSID) continue;
    bool band24 = is24(ch);
    if (band24 && shown24 >= 3) continue;
    if (!band24 && shown50 >= 3) continue;
    if (band24) shown24++;
    else shown50++;

    int height = constrain(map(aps[i].rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_h), 1, graph_h);
    int baseline = band24 ? g24 : g50;
    int offset = band24 ? (idx + 2) * ch24_w : (idx - 14 + 4) * ch50_w;
    const char *ss = ssidOf(&aps[i]);
    char lab[28];
    if (ss) snprintf(lab, sizeof(lab), "%s %d", ss, aps[i].rssi);
    else {
      char id[8];
      bssidShort(aps[i].bssid, id, sizeof(id));
      snprintf(lab, sizeof(lab), "*%s %d", id, aps[i].rssi);
    }
    gfx->setFont(u8g2_font_04b_03_tr);
    gfx->setTextColor(channel_color[idx]);
    int y = baseline - height - 2;
    if (y < STATUS_H + 8) y = STATUS_H + 8;
    printFit(offset - 20, y, 90, lab);
  }

  gfx->drawFastHLine(0, g24, plot_w, RGB565_WHITE);
  gfx->drawFastHLine(0, g50, plot_w, RGB565_WHITE);

  gfx->setFont(u8g2_font_04b_03_tr);
  for (int idx = 0; idx < 14; idx++) {
    int ch = channel_legend[idx];
    int x = (idx + 2) * ch24_w;
    if (ch) {
      gfx->setTextColor(ch == rec24 ? RGB565_LIME : channel_color[idx]);
      gfx->setCursor(x - ((ch < 10) ? 2 : 4), g24 + 8);
      gfx->print(ch);
    }
    if (ap_count_list[idx]) {
      gfx->setTextColor(RGB565_LIGHTGREY);
      gfx->setCursor(x - 2, g24 + 15);
      gfx->print(ap_count_list[idx]);
    }
  }
  for (int idx = 14; idx < LEGEND_N; idx++) {
    int ch = channel_legend[idx];
    int x = (idx - 14 + 4) * ch50_w;
    if (ch) {
      gfx->setTextColor(ch == rec50 ? RGB565_LIME : channel_color[idx]);
      gfx->setCursor(x - ((ch < 100) ? 4 : 5), g50 + 8);
      gfx->print(ch);
    }
    if (ap_count_list[idx]) {
      gfx->setTextColor(RGB565_LIGHTGREY);
      gfx->setCursor(x - 2, g50 + 15);
      gfx->print(ap_count_list[idx]);
    }
  }

  gfx->setFont(u8g2_font_5x7_tr);
  gfx->setTextColor(RGB565_WHITE, RGB565_MEDIUMBLUE);
  gfx->setCursor(2, g24 + 14);
  gfx->print(F("2.4"));
  gfx->setTextColor(RGB565_WHITE, RGB565_LIMEGREEN);
  gfx->setCursor(2, g50 + 14);
  gfx->print(F("5"));
}

/*******************************************************************************
 * Page 2 — Channel score
 ******************************************************************************/
static void drawBar(int16_t x, int16_t y, int16_t maxw, int val, int vmax, uint16_t col)
{
  if (vmax < 1) vmax = 1;
  int w = (int)((long)val * maxw / vmax);
  if (w < 0) w = 0;
  if (w > maxw) w = maxw;
  gfx->drawRect(x, y, maxw, 8, RGB565_LIGHTGREY);
  if (w) gfx->fillRect(x, y, w, 8, col);
}

static void drawScore()
{
  gfx->fillRect(0, STATUS_H, W, H - STATUS_H - FOOTER_H, RGB565_BLACK);
  gfx->setFont(u8g2_font_5x7_tr);

  int y = STATUS_H + 12;
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(4, y);
  gfx->print(F("Recommend  2.4: ch"));
  gfx->setTextColor(RGB565_LIME);
  gfx->print(rec24);
  gfx->setTextColor(RGB565_CYAN);
  gfx->print(F("   5: ch"));
  gfx->setTextColor(RGB565_LIME);
  gfx->print(rec50);
  if (rec50_dfs) {
    gfx->setTextColor(RGB565_ORANGE);
    gfx->print(F("  DFS alt "));
    gfx->print(rec50_dfs);
  }

  y += 14;
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(4, y);
  gfx->print(F("2.4 GHz  score 1 / 6 / 11  (overlap included)"));

  int vmax24 = rec24_score;
  for (int ch : {1, 6, 11}) {
    int s = score_idx[channelIdx(ch)];
    if (s > vmax24) vmax24 = s;
  }
  if (vmax24 < 8) vmax24 = 8;

  y += 6;
  for (int ch : {1, 6, 11}) {
    int s = score_idx[channelIdx(ch)];
    uint16_t col = (ch == rec24) ? RGB565_LIME : ((s > rec24_score * 2) ? RGB565_RED : RGB565_YELLOW);
    gfx->setFont(u8g2_font_5x7_tr);
    gfx->setTextColor(col);
    gfx->setCursor(4, y + 8);
    gfx->printf("ch%2d", ch);
    drawBar(32, y, 200, s, vmax24, col);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(238, y + 8);
    gfx->printf("%d  %uAP", s, ap_count_list[channelIdx(ch)]);
    y += 12;
  }

  y += 6;
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(4, y);
  gfx->print(F("5 GHz  (U1/U3 non-DFS first; DFS marked *)"));
  y += 4;

  /* busiest 5 GHz channels + empty recommended */
  int order[32];
  int on = 0;
  static const int show5[] = {
    36, 40, 44, 48, 149, 153, 157, 161, 165, 52, 60, 100, 116, 132, 140
  };
  for (int ch : show5) {
    if (on < 32) order[on++] = ch;
  }
  int vmax5 = 1;
  for (int i = 0; i < on; i++) {
    int s = score_idx[channelIdx(order[i])];
    if (s > vmax5) vmax5 = s;
  }

  gfx->setFont(u8g2_font_04b_03_tr);
  int col0 = 4;
  int col1 = 164;
  int row_h = 10;
  int rows = (on + 1) / 2;
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < 2; c++) {
      int k = r + c * rows;
      if (k >= on) continue;
      int ch = order[k];
      int x = (c == 0) ? col0 : col1;
      int yy = y + r * row_h;
      int s = score_idx[channelIdx(ch)];
      uint16_t col = isDfs(ch) ? RGB565_ORANGE : ((ch == rec50) ? RGB565_LIME : RGB565_CYAN);
      gfx->setTextColor(col);
      gfx->setCursor(x, yy + 8);
      gfx->printf("%s%3d%s", isDfs(ch) ? "*" : " ", ch, (ch == rec50) ? ">" : " ");
      drawBar(x + 28, yy + 1, 88, s, vmax5, col);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setCursor(x + 118, yy + 8);
      gfx->print(s);
    }
  }

  y = H - FOOTER_H - 10;
  gfx->setFont(u8g2_font_04b_03_tr);
  gfx->setTextColor(RGB565_LIGHTGREY);
  gfx->setCursor(4, y);
  gfx->print(F("Lower score = cleaner. Relative RSSI, not a calibrated meter."));
}

/*******************************************************************************
 * Page 3 — AP list
 ******************************************************************************/
static void drawList()
{
  gfx->fillRect(0, STATUS_H, W, H - STATUS_H - FOOTER_H, RGB565_BLACK);
  gfx->setFont(u8g2_font_04b_03_tr);
  gfx->setTextColor(RGB565_LIGHTGREY);
  gfx->setCursor(2, STATUS_H + 8);
  gfx->print(F("SSID              CH BW  RSSI AUTH PHY"));

  if (!have_scan || ap_n == 0) {
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(4, STATUS_H + 28);
    gfx->print(F("No APs yet"));
    return;
  }

  int y = STATUS_H + 18;
  int end = list_offset + LIST_ROWS;
  if (end > (int)ap_n) end = ap_n;
  for (int n = list_offset; n < end; n++) {
    const wifi_ap_record_t *r = &aps[sorted_idx[n]];
    uint16_t col = rssiColor(r->rssi);
    if (r->authmode == WIFI_AUTH_OPEN) col = RGB565_RED;
    gfx->setTextColor(col);

    char ssbuf[17];
    const char *ss = ssidOf(r);
    if (ss) {
      snprintf(ssbuf, sizeof(ssbuf), "%-16s", ss);
    } else {
      char id[8];
      bssidShort(r->bssid, id, sizeof(id));
      snprintf(ssbuf, sizeof(ssbuf), "*%-15s", id);
    }
    ssbuf[16] = 0;

    char line[48];
    snprintf(line, sizeof(line), "%s %3u %2d %4d %-5s %s%s",
             ssbuf, r->primary, bwMHz(r->bandwidth), r->rssi,
             authStr(r->authmode), phyStr(r),
             isDfs(r->primary) ? "*" : "");
    gfx->setCursor(2, y);
    gfx->print(line);
    y += 10;
  }

  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(2, H - FOOTER_H - 2);
  gfx->printf("%d-%d / %u   *DFS   red=OPEN", list_offset + 1, end, ap_n);
}

/*******************************************************************************
 * Page 4 — Site snapshot
 ******************************************************************************/
static void drawSite()
{
  gfx->fillRect(0, STATUS_H, W, H - STATUS_H - FOOTER_H, RGB565_BLACK);

  int open_n = 0, wpa2 = 0, wpa3 = 0, hidden = 0, mhz40_24 = 0, dfs_n = 0;
  int ax = 0, ac = 0, nphy = 0;
  int strongest_i = -1;

  for (uint16_t i = 0; i < ap_n; i++) {
    if (aps[i].ssid[0] == 0) hidden++;
    if (aps[i].authmode == WIFI_AUTH_OPEN) open_n++;
    if (aps[i].authmode == WIFI_AUTH_WPA2_PSK || aps[i].authmode == WIFI_AUTH_WPA_WPA2_PSK)
      wpa2++;
    if (aps[i].authmode == WIFI_AUTH_WPA3_PSK || aps[i].authmode == WIFI_AUTH_WPA2_WPA3_PSK)
      wpa3++;
    if (is24(aps[i].primary) && bwMHz(aps[i].bandwidth) >= 40) mhz40_24++;
    if (isDfs(aps[i].primary)) dfs_n++;
    if (aps[i].phy_11ax) ax++;
    else if (aps[i].phy_11ac) ac++;
    else if (aps[i].phy_11n) nphy++;
    if (strongest_i < 0 || aps[i].rssi > aps[strongest_i].rssi) strongest_i = i;
  }

  int dual = 0, many_ch = 0;
  char many_ex[48] = "";
  char dual_ex[32] = "";
  for (uint16_t i = 0; i < ap_n; i++) {
    const char *ss = ssidOf(&aps[i]);
    if (!ss) continue;
    bool seen24 = is24(aps[i].primary);
    bool seen5 = !seen24;
    int chset[8];
    int cn = 1;
    chset[0] = aps[i].primary;
    for (uint16_t j = 0; j < ap_n; j++) {
      if (j == i) continue;
      const char *ss2 = ssidOf(&aps[j]);
      if (!ss2 || strcmp(ss, ss2) != 0) continue;
      if (is24(aps[j].primary)) seen24 = true;
      else seen5 = true;
      bool have = false;
      for (int k = 0; k < cn; k++) if (chset[k] == aps[j].primary) have = true;
      if (!have && cn < 8) chset[cn++] = aps[j].primary;
    }
    /* count each SSID once: only when this is the first occurrence */
    bool first_occ = true;
    for (uint16_t j = 0; j < i; j++) {
      const char *ss2 = ssidOf(&aps[j]);
      if (ss2 && strcmp(ss, ss2) == 0) {
        first_occ = false;
        break;
      }
    }
    if (!first_occ) continue;
    if (seen24 && seen5) {
      dual++;
      if (dual_ex[0] == 0) snprintf(dual_ex, sizeof(dual_ex), "%s", ss);
    }
    if (cn >= 3) {
      many_ch++;
      if (many_ex[0] == 0) {
        snprintf(many_ex, sizeof(many_ex), "%s (", ss);
        for (int k = 0; k < cn && k < 4; k++) {
          char t[8];
          snprintf(t, sizeof(t), "%s%d", k ? "," : "", chset[k]);
          strncat(many_ex, t, sizeof(many_ex) - strlen(many_ex) - 2);
        }
        strncat(many_ex, ")", sizeof(many_ex) - strlen(many_ex) - 1);
      }
    }
  }

  gfx->setFont(u8g2_font_5x7_tr);
  int y = STATUS_H + 12;
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(4, y);
  gfx->print(F("Site snapshot  (beacon survey)"));

  auto line = [&](const char *l, uint16_t c = RGB565_WHITE) {
    y += 11;
    gfx->setTextColor(c);
    gfx->setCursor(4, y);
    gfx->print(l);
  };

  char buf[72];
  snprintf(buf, sizeof(buf), "APs  total %u   2.4 GHz %u   5 GHz %u", ap_n, ap24_count, ap50_count);
  line(buf);
  snprintf(buf, sizeof(buf), "Security  OPEN %d   WPA2 %d   WPA3/mix %d   hidden %d",
           open_n, wpa2, wpa3, hidden);
  line(buf, open_n ? RGB565_RED : RGB565_WHITE);
  snprintf(buf, sizeof(buf), "PHY  ax %d   ac %d   n %d", ax, ac, nphy);
  line(buf);
  snprintf(buf, sizeof(buf), "2.4 MHz-40 APs: %d   %s", mhz40_24, mhz40_24 ? "avoid — eats 1/6/11" : "ok");
  line(buf, mhz40_24 ? RGB565_ORANGE : RGB565_LIME);
  snprintf(buf, sizeof(buf), "DFS APs: %d   (weather radar bands 52-144)", dfs_n);
  line(buf, RGB565_ORANGE);
  snprintf(buf, sizeof(buf), "Dual-band SSIDs: %d  %s", dual, dual_ex);
  line(buf);
  snprintf(buf, sizeof(buf), "SSID on 3+ channels: %d  %s", many_ch, many_ex);
  line(buf, many_ch ? RGB565_YELLOW : RGB565_WHITE);

  if (strongest_i >= 0) {
    const wifi_ap_record_t *r = &aps[strongest_i];
    const char *ss = ssidOf(r);
    snprintf(buf, sizeof(buf), "Strongest  %s  ch%u %dMHz  %d dBm  %s %s",
             ss ? ss : "*hidden", r->primary, bwMHz(r->bandwidth), r->rssi,
             authStr(r->authmode), phyStr(r));
    line(buf, RGB565_CYAN);
  }

  snprintf(buf, sizeof(buf), "Plan: 2.4 -> ch %d (score %d)    5 -> ch %d (score %d)",
           rec24, rec24_score, rec50, rec50_score);
  line(buf, RGB565_LIME);

  y += 14;
  gfx->setFont(u8g2_font_04b_03_tr);
  gfx->setTextColor(RGB565_LIGHTGREY);
  gfx->setCursor(4, y);
  gfx->print(F("Not airtime/CCA. PCB antenna, uncalibrated RSSI. CSV on Serial @115200."));
}

/*******************************************************************************
 * Draw dispatcher
 ******************************************************************************/
static void redraw()
{
  drawStatusBar();
  switch (page) {
    case PAGE_SPECTRUM: drawSpectrum(); break;
    case PAGE_SCORE:    drawScore(); break;
    case PAGE_LIST:     drawList(); break;
    default:            drawSite(); break;
  }
  drawFooter();
  dirty = false;
}

/*******************************************************************************
 * Setup / loop
 ******************************************************************************/
void setup()
{
  Serial.begin(115200);
  pinMode(BTN_BOOT, INPUT_PULLUP);
  pinMode(BTN_ALT, INPUT_PULLUP);

  pinMode(TFT_BL, OUTPUT);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, BL_PWM);
#else
  analogWrite(TFT_BL, BL_PWM);
#endif

  if (!gfx->begin()) Serial.println(F("Display init failed"));
  gfx->fillScreen(RGB565_BLACK);
  W = gfx->width();
  H = gfx->height();

  gfx->setFont(u8g2_font_helvR08_tr);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(8, 28);
  gfx->print(F("Dual Band Wi-Fi Analyser"));
  gfx->setFont(u8g2_font_5x7_tr);
  gfx->setCursor(8, 48);
  gfx->print(F("BOOT: cycle screens"));

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.STA.begin();
  WiFi.setScanTimeout(20000);
  WiFi.setScanActiveMinTime(SCAN_ACTIVE_MIN_MS);
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);

  last_scan_ms = millis() - SCAN_INTERVAL_MS;
  dirty = true;
  Serial.println(F("Dual Band Wi-Fi Analyser v2.0 ready"));
}

void loop()
{
  handleButtons();
  pollScan();
  if (dirty) redraw();
  else if (scanning && (millis() % 500) < 20) {
    drawStatusBar();  /* blink SCAN in the bar without full redraw */
  }
}
