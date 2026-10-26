#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>

// NM-CYD-C5 pin assignments are best-effort and may vary by board revision.
#define LCD_SCK 6
#define LCD_MISO 2
#define LCD_MOSI 7
#define LCD_CS 23
#define LCD_DC 24
#define LCD_RST -1
#define LCD_BL 25
#define TOUCH_CS 1

// Landscape orientation for the 320x240 CYD C5 display.
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1, true, 320, 240);
XPT2046_Touchscreen touch(TOUCH_CS);
WebServer server(80);

String trackName = "Waiting for Spotify";
String artistName = "Connect Spotify to begin";
String albumName = "";
bool isPlaying = false;

static const uint16_t BG = 0x0861;
static const uint16_t PANEL = 0x10A2;
static const uint16_t GREEN = 0x07E0;
static const uint16_t MUTED = 0x8410;
static const uint16_t WHITE = 0xFFFF;

String setupPage() {
  return R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><title>CYD Spotify setup</title><style>
  *{box-sizing:border-box}body{font:16px system-ui;max-width:520px;margin:2em auto;padding:0 1em;background:#101410;color:#f5f5f5}
  h2{color:#1ed760}input{width:100%;padding:12px;margin:6px 0 14px;border-radius:8px;border:1px solid #444;background:#202520;color:white}
  button{padding:12px 18px;background:#1ed760;border:0;border-radius:20px;font-weight:bold}
  .note{color:#aaa;font-size:14px;line-height:1.5}</style><h2>CYD Spotify</h2>
  <p>Connect your CYD to Wi-Fi.</p><form method="POST" action="/save"><label>Wi-Fi name (SSID)</label><input name="ssid" required>
  <label>Wi-Fi password</label><input name="password" type="password"><button>Connect Wi-Fi</button></form>
  <p class="note">Spotify account authorization and live playback data are not configured yet. This page only configures Wi-Fi.</p>)HTML";
}

void drawPreviousIcon(int cx, int cy, uint16_t color) {
  gfx->drawFastVLine(cx - 10, cy - 9, 18, color);
  gfx->fillTriangle(cx - 7, cy, cx + 7, cy - 9, cx + 7, cy + 9, color);
}
void drawNextIcon(int cx, int cy, uint16_t color) {
  gfx->drawFastVLine(cx + 10, cy - 9, 18, color);
  gfx->fillTriangle(cx + 7, cy, cx - 7, cy - 9, cx - 7, cy + 9, color);
}
void drawPlayPauseIcon(int cx, int cy, uint16_t color) {
  if (isPlaying) {
    gfx->fillRoundRect(cx - 8, cy - 9, 5, 18, 2, color);
    gfx->fillRoundRect(cx + 3, cy - 9, 5, 18, 2, color);
  } else {
    gfx->fillTriangle(cx - 6, cy - 10, cx - 6, cy + 10, cx + 10, cy, color);
  }
}

void drawScreen() {
  const int16_t sw = gfx->width();
  const int16_t sh = gfx->height();
  gfx->fillScreen(BG);
  // Header uses the live logical width to avoid leaving an unused strip at the right.
  gfx->fillRect(0, 0, sw, 34, PANEL);
  gfx->fillCircle(17, 17, 7, GREEN);
  gfx->setTextColor(WHITE); gfx->setTextSize(2); gfx->setCursor(31, 9); gfx->print("spotify");
  gfx->setTextColor(MUTED); gfx->setTextSize(1); gfx->setCursor(sw - 63, 13);
  gfx->print(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");

  const int artSize = min(112, sh - 106);
  const int artX = 12, artY = 48;
  gfx->fillRoundRect(artX, artY, artSize, artSize, 9, 0x2104);
  gfx->drawRoundRect(artX, artY, artSize, artSize, 9, 0x39E7);
  // Placeholder artwork until Spotify metadata and image download are implemented.
  gfx->fillCircle(artX + artSize / 2, artY + artSize / 2, artSize / 3, GREEN);
  gfx->fillCircle(artX + artSize / 2, artY + artSize / 2, artSize / 4, BG);
  gfx->drawArc(artX + artSize / 2, artY + artSize / 2, artSize / 4, artSize / 4 - 2, 205, 335, GREEN);
  gfx->drawArc(artX + artSize / 2, artY + artSize / 2, artSize / 4 - 5, artSize / 4 - 7, 205, 335, GREEN);

  const int infoX = artX + artSize + 13;
  const int infoW = max(80, sw - infoX - 10);
  gfx->setTextColor(WHITE); gfx->setTextSize(2); gfx->setCursor(infoX, 59);
  String t = trackName;
  while (t.length() > 0 && t.length() * 12 > infoW) t.remove(t.length() - 1);
  gfx->print(t);
  gfx->setTextColor(0xBDF7); gfx->setTextSize(1); gfx->setCursor(infoX, 88);
  String a = artistName;
  while (a.length() > 0 && a.length() * 6 > infoW) a.remove(a.length() - 1);
  gfx->print(a);
  if (albumName.length()) {
    gfx->setCursor(infoX, 104);
    String al = albumName;
    while (al.length() > 0 && al.length() * 6 > infoW) al.remove(al.length() - 1);
    gfx->print(al);
  }
  gfx->setTextColor(GREEN); gfx->setCursor(infoX, 127);
  gfx->print("SPOTIFY CONNECT PENDING");

  const int lineY = sh - 58;
  gfx->drawFastHLine(12, lineY, sw - 24, 0x39E7);
  // Progress bar placeholder.
  gfx->fillRoundRect(12, lineY + 9, sw - 24, 4, 2, 0x39E7);
  gfx->fillRoundRect(12, lineY + 9, 3, 4, 2, GREEN);

  const int cy = sh - 23;
  const int center = sw / 2;
  gfx->fillCircle(center - 57, cy, 17, PANEL);
  gfx->fillCircle(center, cy, 21, GREEN);
  gfx->fillCircle(center + 57, cy, 17, PANEL);
  drawPreviousIcon(center - 57, cy, WHITE);
  drawPlayPauseIcon(center, cy, BG);
  drawNextIcon(center + 57, cy, WHITE);
  // Small settings glyph at far right, outside the main transport controls.
  gfx->drawCircle(sw - 17, 52, 7, MUTED);
  gfx->drawCircle(sw - 17, 52, 2, MUTED);
}

void setup() {
  Serial.begin(115200);
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, HIGH);
  gfx->begin();
  touch.begin(); touch.setRotation(1);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("CYD-Spotify-Setup");
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", setupPage()); });
  server.on("/save", HTTP_POST, [](){
    String ssid = server.arg("ssid");
    String pass = server.arg("password");
    WiFi.begin(ssid.c_str(), pass.c_str());
    server.send(200, "text/html", "<p>Wi-Fi connection started. Reconnect to your home Wi-Fi, then check the CYD screen.</p><a href='/'>Back</a>");
  });
  // These endpoints remain explicit placeholders until Spotify OAuth is configured.
  server.on("/prev", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify OAuth not configured yet"); });
  server.on("/toggle", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify OAuth not configured yet"); });
  server.on("/next", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify OAuth not configured yet"); });
  server.begin();
  drawScreen();
  Serial.println("Setup Wi-Fi: CYD-Spotify-Setup; open http://192.168.4.1");
}

void loop() {
  server.handleClient();
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    Serial.printf("Touch raw x=%d y=%d\n", p.x, p.y);
    delay(250);
  }
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 1500) {
    lastDraw = millis();
    drawScreen();
  }
  delay(5);
}
