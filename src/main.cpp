#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#define LCD_SCK 6
#define LCD_MISO 2
#define LCD_MOSI 7
#define LCD_CS 23
#define LCD_DC 24
#define LCD_RST -1
#define LCD_BL 25
#define TOUCH_CS 1

static const char *CLIENT_ID = "7b48f9235b05408fa82c84d8b4a32150";
static const char *REDIRECT_URI = "cydspotify://callback";
static const char *SCOPES = "user-read-currently-playing user-read-playback-state user-modify-playback-state";
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC,LCD_CS,LCD_SCK,LCD_MOSI,LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus,LCD_RST,1,true,320,240);
XPT2046_Touchscreen touch(TOUCH_CS);
WebServer server(80);
Preferences prefs;
String verifier, oauthState, accessToken, refreshToken;
String trackName="Spotify player", artistName="Connect Spotify in setup", albumName="", statusText="NOT CONNECTED";
bool isPlaying=false;
int progressMs=0,durationMs=0;
unsigned long lastPoll=0,lastProgress=0;
const uint16_t BG=0x0861,PANEL=0x10A2,GREEN=0x07E0,MUTED=0x8410,WHITE=0xFFFF;

String enc(const String&s){String o;const char*h="0123456789ABCDEF";for(size_t i=0;i<s.length();i++){uint8_t c=s[i];if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')o+=(char)c;else if(c==' ')o+="%20";else{o+='%';o+=h[c>>4];o+=h[c&15];}}return o;}
String randomString(size_t n){const char*c="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";String s;s.reserve(n);for(size_t i=0;i<n;i++)s+=c[esp_random()%66];return s;}
String b64url(const uint8_t*d,size_t n){size_t olen=0;unsigned char out[128];if(mbedtls_base64_encode(out,sizeof(out)-1,&olen,d,n)!=0)return "";out[olen]=0;String s((char*)out);s.replace("+","-");s.replace("/","_");s.replace("=","");return s;}
String challengeFor(const String&v){uint8_t hash[32];mbedtls_md_context_t c;mbedtls_md_init(&c);const mbedtls_md_info_t*i=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);mbedtls_md_setup(&c,i,0);mbedtls_md_starts(&c);mbedtls_md_update(&c,(const unsigned char*)v.c_str(),v.length());mbedtls_md_finish(&c,hash);mbedtls_md_free(&c);return b64url(hash,32);}
String htmlPage(){
 String u="https://accounts.spotify.com/authorize?client_id="+String(CLIENT_ID)+"&response_type=code&redirect_uri="+enc(REDIRECT_URI)+"&scope="+enc(SCOPES)+"&code_challenge_method=S256&code_challenge="+enc(challengeFor(verifier))+"&state="+enc(oauthState);
 String p=R"HTML(<!doctype html><html><meta name="viewport" content="width=device-width,initial-scale=1"><title>CYD Spotify</title><style>*{box-sizing:border-box}body{font:16px system-ui;max-width:540px;margin:24px auto;padding:0 16px;background:#101410;color:#f5f5f5}h2{color:#1ed760}input{width:100%;padding:12px;margin:8px 0 14px;border-radius:8px;border:1px solid #444;background:#202520;color:white}button,.btn{display:inline-block;padding:12px 18px;background:#1ed760;color:#071007;border:0;border-radius:22px;font-weight:700;text-decoration:none}.note{color:#aaa;font-size:14px;line-height:1.5}.card{background:#1b211b;padding:16px;border-radius:14px;margin:14px 0}</style>
<h2>Spotify player · CYD C5</h2><div class="card"><b>1. Wi-Fi</b><form method="POST" action="/wifi"><input name="ssid" placeholder="Wi-Fi name (SSID)" required><input name="password" type="password" placeholder="Wi-Fi password"><button>Save Wi-Fi</button></form></div>
<div class="card"><b>2. Spotify koppelen</b><p class="note">Authorizeer Spotify. Je hebt geen extra pc-programma nodig. Na toestemming kopieer je de volledige callback-URL uit de adresbalk en plak je die hieronder.</p><a class="btn" href="AUTHURL">Spotify koppelen</a><p class="note">Redirect URI moet in Spotify Developer Dashboard staan als <b>cydspotify://callback</b>. Als de browser meldt dat het adres niet geopend kan worden, kopieer dan de volledige URL die begint met cydspotify://callback.</p><form method="POST" action="/code"><input name="callback" placeholder="cydspotify://callback?code=...&state=..." required><button>Voltooien</button></form></div>
<div class="card"><b>Status</b><p>STATUSTEXT</p></div></html>)HTML";
 p.replace("AUTHURL",u);p.replace("STATUSTEXT",statusText);return p;
}
void drawPrev(int x,int y,uint16_t c){gfx->drawFastVLine(x-9,y-8,16,c);gfx->fillTriangle(x-6,y,x+7,y-8,x+7,y+8,c);}
void drawNext(int x,int y,uint16_t c){gfx->drawFastVLine(x+9,y-8,16,c);gfx->fillTriangle(x+6,y,x-7,y-8,x-7,y+8,c);}
void drawPlay(int x,int y,uint16_t c){if(isPlaying){gfx->fillRoundRect(x-7,y-8,5,16,2,c);gfx->fillRoundRect(x+3,y-8,5,16,2,c);}else gfx->fillTriangle(x-6,y-9,x-6,y+9,x+9,y,c);}
void drawScreen(){int sw=gfx->width(),sh=gfx->height();gfx->fillScreen(BG);gfx->fillRect(0,0,sw,34,PANEL);gfx->fillCircle(17,17,7,GREEN);gfx->setTextColor(WHITE);gfx->setTextSize(2);gfx->setCursor(31,9);gfx->print("spotify");gfx->setTextColor(WiFi.status()==WL_CONNECTED?GREEN:MUTED);gfx->setTextSize(1);gfx->setCursor(sw-74,13);gfx->print(WiFi.status()==WL_CONNECTED?"ONLINE":"SETUP AP");
 int art=min(112,sh-106),ax=12,ay=48;gfx->fillRoundRect(ax,ay,art,art,9,0x2104);gfx->fillCircle(ax+art/2,ay+art/2,art/3,GREEN);gfx->drawArc(ax+art/2,ay+art/2,art/4,art/4-2,205,335,BG);gfx->drawArc(ax+art/2,ay+art/2,art/4-5,art/4-7,205,335,BG);
 int ix=ax+art+13,iw=max(80,sw-ix-10);gfx->setTextColor(WHITE);gfx->setTextSize(2);gfx->setCursor(ix,58);String t=trackName;while(t.length()&&t.length()*12>iw)t.remove(t.length()-1);gfx->print(t);gfx->setTextColor(0xBDF7);gfx->setTextSize(1);gfx->setCursor(ix,88);String a=artistName;while(a.length()&&a.length()*6>iw)a.remove(a.length()-1);gfx->print(a);gfx->setCursor(ix,104);String al=albumName;while(al.length()&&al.length()*6>iw)al.remove(al.length()-1);gfx->print(al);gfx->setTextColor(GREEN);gfx->setCursor(ix,128);gfx->print(statusText.substring(0,min((int)statusText.length(),24)));
 int ly=sh-58,bw=sw-24;gfx->drawFastHLine(12,ly,bw,0x39E7);gfx->fillRoundRect(12,ly+9,bw,4,2,0x39E7);if(durationMs>0)gfx->fillRoundRect(12,ly+9,max(3,(long)bw*progressMs/durationMs),4,2,GREEN);int cy=sh-23,cc=sw/2;gfx->fillCircle(cc-57,cy,17,PANEL);gfx->fillCircle(cc,cy,21,GREEN);gfx->fillCircle(cc+57,cy,17,PANEL);drawPrev(cc-57,cy,WHITE);drawPlay(cc,cy,BG);drawNext(cc+57,cy,WHITE);}
bool postForm(const String&url,const String&body,String&response){WiFiClientSecure c;c.setInsecure();HTTPClient h;if(!h.begin(c,url))return false;h.addHeader("Content-Type","application/x-www-form-urlencoded");int code=h.POST(body);response=h.getString();h.end();return code>=200&&code<300;}
bool apiRequest(const String&m,const String&p){if(!accessToken.length())return false;WiFiClientSecure c;c.setInsecure();HTTPClient h;if(!h.begin(c,"https://api.spotify.com/v1"+p))return false;h.addHeader("Authorization","Bearer "+accessToken);int code=m=="POST"?h.POST(""):m=="PUT"?h.PUT(""):h.GET();h.end();if(code==401)statusText="TOKEN EXPIRED";return code>=200&&code<300;}
bool exchangeCode(String cb){int q=cb.indexOf('?');if(q<0)return false;String query=cb.substring(q+1),code="",state="";int p=0;while(p<(int)query.length()){int e=query.indexOf('&',p);if(e<0)e=query.length();String kv=query.substring(p,e);int eq=kv.indexOf('=');if(eq>0){String k=kv.substring(0,eq),v=kv.substring(eq+1);if(k=="code")code=v;if(k=="state")state=v;}p=e+1;}if(!code.length()||state!=oauthState)return false;String resp,body="client_id="+String(CLIENT_ID)+"&grant_type=authorization_code&code="+enc(code)+"&redirect_uri="+enc(REDIRECT_URI)+"&code_verifier="+enc(verifier);if(!postForm("https://accounts.spotify.com/api/token",body,resp)){statusText="TOKEN EXCHANGE FAILED";return false;}JsonDocument d;if(deserializeJson(d,resp))return false;accessToken=d["access_token"]|"";refreshToken=d["refresh_token"]|"";prefs.putString("refresh",refreshToken);prefs.putString("access",accessToken);statusText="SPOTIFY CONNECTED";return accessToken.length();}
bool refreshAuth(){if(!refreshToken.length())return false;String resp,body="client_id="+String(CLIENT_ID)+"&grant_type=refresh_token&refresh_token="+enc(refreshToken);if(!postForm("https://accounts.spotify.com/api/token",body,resp))return false;JsonDocument d;if(deserializeJson(d,resp))return false;accessToken=d["access_token"]|"";String rt=d["refresh_token"]|"";if(rt.length())refreshToken=rt;prefs.putString("access",accessToken);prefs.putString("refresh",refreshToken);return accessToken.length();}
void pollSpotify(){if(!accessToken.length()&&!refreshAuth()){statusText="CONNECT IN SETUP";return;}WiFiClientSecure c;c.setInsecure();HTTPClient h;if(!h.begin(c,"https://api.spotify.com/v1/me/player/currently-playing"))return;h.addHeader("Authorization","Bearer "+accessToken);int code=h.GET();String body=h.getString();h.end();if(code==204){trackName="Nothing playing";artistName="Start Spotify on another device";albumName="";statusText="READY";isPlaying=false;durationMs=progressMs=0;return;}if(code==401){if(refreshAuth())return;statusText="RECONNECT SPOTIFY";return;}if(code!=200){statusText=code==403?"API ACCESS DENIED":"SPOTIFY API ERROR";return;}JsonDocument d;if(deserializeJson(d,body)){statusText="BAD SPOTIFY DATA";return;}JsonObject item=d["item"];trackName=item["name"]|"Unknown track";albumName=item["album"]["name"]|"";artistName="";for(JsonVariant v:item["artists"].as<JsonArray>()){if(artistName.length())artistName+=", ";artistName+=(const char*)(v["name"]|"");}isPlaying=d["is_playing"]|false;progressMs=d["progress_ms"]|0;durationMs=item["duration_ms"]|0;statusText=isPlaying?"NOW PLAYING":"PAUSED";lastProgress=millis();}
void setup(){Serial.begin(115200);pinMode(LCD_BL,OUTPUT);digitalWrite(LCD_BL,HIGH);gfx->begin();touch.begin();touch.setRotation(1);prefs.begin("spotify",false);accessToken=prefs.getString("access","");refreshToken=prefs.getString("refresh","");verifier=randomString(64);oauthState=randomString(24);WiFi.mode(WIFI_AP_STA);WiFi.softAP("CYD-Spotify-Setup");
 server.on("/",HTTP_GET,[]{server.send(200,"text/html",htmlPage());});server.on("/wifi",HTTP_POST,[]{String s=server.arg("ssid"),p=server.arg("password");WiFi.begin(s.c_str(),p.c_str());server.send(200,"text/html","<p>Connecting to Wi-Fi. Setup AP remains available.</p><a href='/'>Back</a>");});server.on("/code",HTTP_POST,[]{bool ok=exchangeCode(server.arg("callback"));server.send(ok?200:400,"text/html",ok?"<p>Spotify connected. Close this page.</p><a href='/'>Back</a>":"<p>Could not exchange code. Check callback URL and redirect URI.</p><a href='/'>Try again</a>");drawScreen();});server.on("/prev",HTTP_POST,[]{bool ok=apiRequest("POST","/me/player/previous");server.send(ok?200:502,"text/plain",ok?"OK":"Spotify command failed");});server.on("/toggle",HTTP_POST,[]{bool ok=apiRequest("PUT",isPlaying?"/me/player/pause":"/me/player/play");server.send(ok?200:502,"text/plain",ok?"OK":"Spotify command failed");});server.on("/next",HTTP_POST,[]{bool ok=apiRequest("POST","/me/player/next");server.send(ok?200:502,"text/plain",ok?"OK":"Spotify command failed");});server.begin();drawScreen();Serial.println("Connect to CYD-Spotify-Setup and open http://192.168.4.1");}
void loop(){server.handleClient();if(WiFi.status()==WL_CONNECTED&&millis()-lastPoll>2500){lastPoll=millis();pollSpotify();drawScreen();}if(touch.touched()){TS_Point p=touch.getPoint();Serial.printf("Touch raw x=%d y=%d\n",p.x,p.y);delay(220);}if(isPlaying&&durationMs>0&&millis()-lastProgress>1000){progressMs=min(durationMs,progressMs+1000);lastProgress=millis();}delay(5);}
