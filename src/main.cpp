#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>

// NM-CYD-C5 starter. Pin assignments are best-effort and may vary by revision.
#define LCD_SCK 6
#define LCD_MISO 2
#define LCD_MOSI 7
#define LCD_CS 23
#define LCD_DC 24
#define LCD_RST -1
#define LCD_BL 25
#define TOUCH_CS 1

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1, true, 320, 240);
XPT2046_Touchscreen touch(TOUCH_CS);
WebServer server(80);
String trackName = "Spotify mini-player";
String artistName = "Not connected";
String albumName = "Spotify integration pending";

String setupPage() {
  return R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><title>CYD Spotify setup</title><style>body{font:16px sans-serif;max-width:520px;margin:2em auto;padding:0 1em;background:#111;color:#eee}input{width:100%;padding:10px;margin:6px 0 14px;box-sizing:border-box}button{padding:12px;background:#1ed760;border:0;border-radius:8px;font-weight:bold}</style><h2>CYD Spotify mini-player</h2><p>Connect this device to your Wi-Fi.</p><form method="POST" action="/save"><label>Wi-Fi name</label><input name="ssid" required><label>Wi-Fi password</label><input name="password" type="password"><button>Connect</button></form><p>Spotify OAuth, album art, and playback controls still need implementing.</p>)HTML";
}
void drawScreen() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillRect(0, 0, 320, 36, 0x18E3);
  gfx->setTextColor(RGB565_WHITE); gfx->setTextSize(2); gfx->setCursor(10, 10); gfx->print("SPOTIFY MINI");
  gfx->drawRoundRect(8, 46, 112, 112, 0x4208); gfx->fillRect(12, 50, 104, 104, 0x2104);
  gfx->setTextSize(1); gfx->setCursor(30, 94); gfx->print("ALBUM ART");
  gfx->setTextSize(2); gfx->setCursor(132, 54); gfx->print(trackName.substring(0, 16));
  gfx->setTextSize(1); gfx->setCursor(132, 82); gfx->print(artistName.substring(0, 27)); gfx->setCursor(132, 98); gfx->print(albumName.substring(0, 27));
  gfx->drawFastHLine(10, 172, 300, 0x4208);
  gfx->drawRoundRect(10, 202, 70, 28, 0x07E0); gfx->drawRoundRect(91, 202, 70, 28, 0xFFFF); gfx->drawRoundRect(172, 202, 65, 28, 0xFFFF); gfx->drawRoundRect(248, 202, 62, 28, 0xF800);
  gfx->setTextSize(1); gfx->setCursor(30, 213); gfx->print("PREV"); gfx->setCursor(112, 213); gfx->print("PLAY"); gfx->setCursor(193, 213); gfx->print("NEXT"); gfx->setCursor(261, 213); gfx->print("SETUP");
  gfx->setCursor(10, 180); gfx->print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Setup AP: 192.168.4.1");
}
void setup() {
  Serial.begin(115200);
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, HIGH);
  gfx->begin(); touch.begin(); touch.setRotation(1);
  WiFi.mode(WIFI_AP_STA); WiFi.softAP("CYD-Spotify-Setup");
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", setupPage()); });
  server.on("/save", HTTP_POST, [](){ String ssid = server.arg("ssid"); String pass = server.arg("password"); server.send(200, "text/html", "Wi-Fi connection started. Check the screen/serial monitor."); WiFi.begin(ssid.c_str(), pass.c_str()); });
  server.on("/prev", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify integration not configured"); });
  server.on("/toggle", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify integration not configured"); });
  server.on("/next", HTTP_POST, [](){ server.send(501, "text/plain", "Spotify integration not configured"); });
  server.begin(); drawScreen();
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
  if (millis() - lastDraw > 1500) { lastDraw = millis(); drawScreen(); }
  delay(5);
}
