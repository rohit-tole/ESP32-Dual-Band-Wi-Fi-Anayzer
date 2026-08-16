/*******************************************************************************
 * ESP32-C5 Wi-Fi Analyzer – Rohit Tole
 ******************************************************************************/
#if CONFIG_SOC_WIFI_SUPPORT_5G

#define SCAN_INTERVAL 3000

#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>

/*******************************************************************************
 * Display
 ******************************************************************************/
#define GFX_BL DF_GFX_BL

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else
Arduino_DataBus *bus = create_default_Arduino_DataBus();
Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 1, false);
#endif

/*******************************************************************************
 * Layout variables
 ******************************************************************************/
int16_t w, h;
int16_t banner_height;
int16_t graph_height;
int16_t graph24_baseline, graph50_baseline;
int16_t graph_baseline;
int16_t channel24_width, channel50_width, signal_width;

/*******************************************************************************
 * RSSI
 ******************************************************************************/
#define RSSI_CEILING     -30
#define RSSI_SHOW_SSID   -70
#define RSSI_FLOOR       -100

#define RSSI_SCALE_WIDTH 36
#define RSSI_TICK_LEN    7

/*******************************************************************************
 * Channel legend + colors
 ******************************************************************************/
uint8_t channel_legend[] = {
  1,2,3,4,5,6,7,8,9,10,11,12,13,0,
  0,0,36,0,0,0,44,0,0,0,52,0,0,0,
  60,0,0,0,68,0,0,0,0,0,0,0,0,0,
  0,100,0,0,0,108,0,0,0,116,0,0,0,124,
  0,0,0,132,0,0,0,140,0,0,0,149,0,0,
  0,157,0,0,0,165,0,0,0,173,0,0,0
};

uint16_t channel_color[] = {
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

uint8_t scan_count = 0;

/*******************************************************************************
 * Helpers
 ******************************************************************************/
uint16_t channelIdx(int ch)
{
  if (ch <= 14)  return ch - 1;
  if (ch <= 68)  return 14 + ((ch - 32) / 2);
  if (ch <= 144) return 41 + ((ch - 96) / 2);
  if (ch <= 177) return 67 + ((ch - 149) / 2);
  return 82;
}

bool matchBssidPrefix(uint8_t *a, uint8_t *b)
{
  for (uint8_t i = 0; i < 5; i++)
    if (a[i] != b[i]) return false;
  return true;
}

uint16_t rssiColor(int rssi)
{
  if (rssi >= -50) return RGB565_LIME;
  if (rssi >= -70) return RGB565_YELLOW;
  return RGB565_RED;
}

/*******************************************************************************
 * RSSI SCALE (RIGHT SIDE)
 ******************************************************************************/
void drawRSSIScale(int16_t baseline, bool is5G)
{
  int x = w - RSSI_SCALE_WIDTH;

  int marks24[] = { -30, -50, -70, -90 };
  int marks50[] = { -35, -55, -75, -90 };

  int *marks = is5G ? marks50 : marks24;

  gfx->setFont(u8g2_font_04b_03_tr);

  for (int i = 0; i < 4; i++)
  {
    int rssi = marks[i];
    int y = baseline - map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_height);
    uint16_t c = rssiColor(rssi);

    gfx->drawFastHLine(x, y, RSSI_TICK_LEN, c);
    gfx->setTextColor(c);
    gfx->setCursor(x + RSSI_TICK_LEN + 3, y + 3);
    gfx->print(rssi);
  }

  // RSSI label (~10 px)
  gfx->setFont(u8g2_font_helvR08_tr);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(x, baseline - graph_height - 6);
  gfx->print("RSSI");
}

/*******************************************************************************
 * SETUP
 ******************************************************************************/
void setup()
{
  Serial.begin(115200);
  WiFi.STA.begin();

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  w = gfx->width();
  h = gfx->height();

  banner_height = 16;

  // ---- TITLE (~10 px, readable) ----
  gfx->fillRect(0, 0, w, banner_height, RGB565_PURPLE);
  gfx->setFont(u8g2_font_helvR08_tr);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(2, 11);
  gfx->print("ESP32 C5 Wi-Fi Analyzer - Rohit Tole");

  graph_height = ((h - banner_height) / 2) - 16;
  graph24_baseline = banner_height + graph_height;
  graph50_baseline = graph24_baseline + 14 + graph_height;

  channel24_width = (w - RSSI_SCALE_WIDTH) / (14 + 2);
  channel50_width = (w - RSSI_SCALE_WIDTH) / (sizeof(channel_legend) - 14 + 4);
}

/*******************************************************************************
 * LOOP (UNCHANGED LOGIC)
 ******************************************************************************/
void loop()
{
  uint16_t ap24_count = 0, ap50_count = 0;
  uint8_t ap_count_list[sizeof(channel_legend)];
  int32_t peak_list[sizeof(channel_legend)];
  int16_t peak_id_list[sizeof(channel_legend)];

  for (int i = 0; i < sizeof(channel_legend); i++)
  {
    ap_count_list[i] = 0;
    peak_list[i] = RSSI_FLOOR;
    peak_id_list[i] = -1;
  }

  WiFi.setBandMode((scan_count < 1) ? WIFI_BAND_MODE_2G_ONLY : WIFI_BAND_MODE_AUTO);
  int n = WiFi.scanNetworks(false, true, false, 300);

  gfx->fillRect(0, banner_height, w, h - banner_height, RGB565_BLACK);

  drawRSSIScale(graph24_baseline, false);
  drawRSSIScale(graph50_baseline, true);

  /* ---------- ANALYZE ---------- */
  for (int i = 0; i < n; i++)
  {
    int ch = WiFi.channel(i);
    int idx = channelIdx(ch);
    int rssi = WiFi.RSSI(i);

    if (WiFi.SSID(i).length() && rssi > peak_list[idx])
    {
      peak_list[idx] = rssi;
      peak_id_list[idx] = i;
    }

    bool dup = false;
    for (int j = 0; j < i; j++)
      if (WiFi.channel(j) == ch &&
          matchBssidPrefix(WiFi.BSSID(j), WiFi.BSSID(i)))
        dup = true;

    if (!dup)
    {
      ap_count_list[idx]++;
      (ch <= 14) ? ap24_count++ : ap50_count++;
    }
  }

  /* ---------- DRAW SIGNALS + SSID ---------- */
  for (int i = 0; i < n; i++)
  {
    int ch = WiFi.channel(i);
    int idx = channelIdx(ch);
    int rssi = WiFi.RSSI(i);

    int height = constrain(
      map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_height),
      1, graph_height
    );

    if (idx < 14)
    {
      graph_baseline = graph24_baseline;
      signal_width = channel24_width * 2;
    }
    else
    {
      graph_baseline = graph50_baseline;
      signal_width = channel50_width * 4;
    }

    int offset = (idx < 14)
      ? (idx + 2) * channel24_width
      : (idx - 14 + 4) * channel50_width;

    gfx->startWrite();
    gfx->writeEllipseHelper(
      offset, graph_baseline + 1,
      signal_width, height,
      0b0011, channel_color[idx]
    );
    gfx->endWrite();

    if (i == peak_id_list[idx] && rssi >= RSSI_SHOW_SSID)
    {
      gfx->setTextColor(channel_color[idx]);
      gfx->setCursor(offset - signal_width, graph_baseline - height - 2);
      gfx->print(WiFi.SSID(i));
      gfx->printf("(%d)", rssi);
    }
  }

  /* ---------- BASELINES ---------- */
  gfx->drawFastHLine(0, graph24_baseline, w - RSSI_SCALE_WIDTH, RGB565_WHITE);
  gfx->drawFastHLine(0, graph50_baseline, w - RSSI_SCALE_WIDTH, RGB565_WHITE);

  /* ---------- CHANNEL NUMBERS ---------- */
  gfx->setFont(u8g2_font_04b_03_tr);

  for (int idx = 0; idx < 14; idx++)
  {
    int ch = channel_legend[idx];
    int x = (idx + 2) * channel24_width;
    if (ch)
    {
      gfx->setTextColor(channel_color[idx]);
      gfx->setCursor(x - ((ch < 10) ? 2 : 4), graph24_baseline + 8);
      gfx->print(ch);
    }
  }

  for (int idx = 14; idx < sizeof(channel_legend); idx++)
  {
    int ch = channel_legend[idx];
    int x = (idx - 14 + 4) * channel50_width;
    if (ch)
    {
      gfx->setTextColor(channel_color[idx]);
      gfx->setCursor(x - ((ch < 100) ? 4 : 5), graph50_baseline + 8);
      gfx->print(ch);
    }
  }

  /* ---------- BAND LABELS (~10 px) ---------- */
  gfx->setFont(u8g2_font_helvR08_tr);

  gfx->setTextColor(RGB565_WHITE, RGB565_MEDIUMBLUE);
  gfx->setCursor(2, graph24_baseline + 11);
  gfx->print("2.4");

  gfx->setTextColor(RGB565_WHITE, RGB565_LIMEGREEN);
  gfx->setCursor(2, graph50_baseline + 11);
  gfx->print("5");

  delay((scan_count < 2) ? 0 : SCAN_INTERVAL);
  scan_count++;
}

#endif
