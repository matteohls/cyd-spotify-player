#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#define AP_SSID "CYD-TERMINAL"
#define AP_PASS "terminal123"
static httpd_handle_t srv;
static const char page[] =
"<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>CYD Terminal</title><style>"
"*{box-sizing:border-box}body{margin:0;background:#070b08;color:#7dff9a;font:15px ui-monospace,Consolas,monospace}header{padding:16px;background:#101b12;border-bottom:1px solid #285b37}main{max-width:900px;margin:auto;padding:14px}.screen{background:#020503;border:1px solid #285b37;border-radius:7px;padding:14px;min-height:48vh}#out{white-space:pre-wrap;overflow-wrap:anywhere;line-height:1.5}.input{display:flex;gap:8px;margin-top:12px}input[type=text]{flex:1;min-width:0;background:#030704;color:#a4ffb7;border:1px solid #356c43;padding:12px;font:inherit}button{background:#153c20;color:#9cffb0;border:1px solid #438b53;padding:10px 12px;font:inherit;cursor:pointer;margin:3px 3px 3px 0}.bar{margin:10px 0}.upload{border:1px solid #285b37;padding:12px;margin-top:14px}.small{font-size:12px;color:#8eae96;line-height:1.5}.warn{color:#ffd080}</style></head><body><header><b>▣ CYD WEB TERMINAL</b> <span class=small>ESP32-C5 / ESP-IDF</span></header><main><div class=screen><div id=out>CYD WEB TERMINAL [ready]\nType help for commands.\n</div><div class=input><span>&gt;</span><input id=cmd autofocus autocomplete=off><button id=run>RUN</button></div></div><div class=bar><button data-c=help>HELP</button><button data-c=info>INFO</button><button data-c=heap>HEAP</button><button data-c=uptime>UPTIME</button><button data-c=wifi>WIFI</button><button data-c=partitions>PARTITIONS</button><button id=clear>CLEAR</button></div><section class=upload><b>FIRMWARE OTA</b><p class='small warn'>Only upload a compatible ESP32-C5 application .bin. This installs firmware and reboots. It cannot run Linux or arbitrary desktop binaries.</p><input type=file id=file accept='.bin,application/octet-stream'> <button id=flash>UPLOAD & INSTALL</button><p id=state class=small>Ready.</p></section><p class=small>Wi-Fi: CYD-TERMINAL · password: terminal123 · http://192.168.4.1</p></main><script>"
"const out=document.querySelector('#out'),cmd=document.querySelector('#cmd');function print(s){out.textContent+=s+'\\n'}async function run(s){s=s.trim();if(!s)return;if(s==='clear'){out.textContent='';return}print('> '+s);cmd.value='';try{let r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'text/plain'},body:s});print(await r.text())}catch(e){print('Connection lost: '+e)}}document.querySelector('#run').onclick=()=>run(cmd.value);cmd.onkeydown=e=>{if(e.key==='Enter')run(cmd.value)};document.querySelectorAll('[data-c]').forEach(b=>b.onclick=()=>run(b.dataset.c));document.querySelector('#clear').onclick=()=>out.textContent='';"
"document.querySelector('#flash').onclick=async()=>{let f=document.querySelector('#file').files[0],s=document.querySelector('#state');if(!f){s.textContent='Choose a .bin first';return}if(!confirm('Install '+f.name+' and reboot?'))return;s.textContent='Uploading... keep power connected';try{let r=await fetch('/api/update',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f});s.textContent=await r.text()}catch(e){s.textContent='Connection closed during reboot: '+e}};fetch('/api/info').then(r=>r.text()).then(t=>document.title='CYD Terminal · '+t);</script></body></html>";

static esp_err_t root(httpd_req_t *r){httpd_resp_set_type(r,"text/html; charset=utf-8");return httpd_resp_send(r,page,HTTPD_RESP_USE_STRLEN);}
static esp_err_t info(httpd_req_t *r){char b[160];const esp_app_desc_t*d=esp_app_get_description();snprintf(b,sizeof(b),"%s | %s | heap %u",d->version,esp_get_idf_version(),(unsigned)esp_get_free_heap_size());return httpd_resp_sendstr(r,b);}
static esp_err_t command(httpd_req_t *r){
 char c[128]={0},o[1200]={0};int n=r->content_len<127?r->content_len:127;int got=httpd_req_recv(r,c,n);if(got<=0){httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"No command");return ESP_FAIL;}c[got]=0;while(got>0&&(c[got-1]=='\n'||c[got-1]=='\r'||c[got-1]==' '))c[--got]=0;
 const esp_app_desc_t*d=esp_app_get_description();uint64_t ms=esp_timer_get_time()/1000;
 if(!strcmp(c,"help"))strcpy(o,"Commands: help, info, uptime, heap, wifi, partitions, version, echo TEXT, clear, reboot\nThis is a firmware diagnostic terminal, not a Linux shell. Upload compatible ESP32-C5 app .bin files using OTA.");
 else if(!strcmp(c,"info")){esp_chip_info_t i;esp_chip_info(&i);snprintf(o,sizeof(o),"Chip: ESP32-C5\nCores: %d\nRevision: %d\nFirmware: %s\nESP-IDF: %s",i.cores,i.revision,d->version,esp_get_idf_version());}
 else if(!strcmp(c,"version"))snprintf(o,sizeof(o),"CYD Web Terminal %s\nESP-IDF %s\nTarget ESP32-C5",d->version,esp_get_idf_version());
 else if(!strcmp(c,"uptime"))snprintf(o,sizeof(o),"Uptime: %" PRIu64 " ms (%.1f minutes)",ms,ms/60000.0);
 else if(!strcmp(c,"heap"))snprintf(o,sizeof(o),"Free heap: %u bytes\nMinimum free heap: %u bytes\nLargest block: %u bytes",(unsigned)esp_get_free_heap_size(),(unsigned)esp_get_minimum_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
 else if(!strcmp(c,"wifi"))snprintf(o,sizeof(o),"AP SSID: %s\nAP IP: 192.168.4.1\nPassword: %s",AP_SSID,AP_PASS);
 else if(!strcmp(c,"partitions")){const esp_partition_t*a=esp_ota_get_running_partition(),*b=esp_ota_get_next_update_partition(NULL);snprintf(o,sizeof(o),"Running: %s offset=0x%lx size=%lu\nOTA target: %s offset=0x%lx size=%lu",a?a->label:"?",a?(unsigned long)a->address:0,a?(unsigned long)a->size:0,b?b->label:"?",b?(unsigned long)b->address:0,b?(unsigned long)b->size:0);}
 else if(!strncmp(c,"echo ",5))snprintf(o,sizeof(o),"%s",c+5);
 else if(!strcmp(c,"clear"))strcpy(o,"Use CLEAR in the browser.");
 else if(!strcmp(c,"reboot")){httpd_resp_sendstr(r,"Rebooting...");vTaskDelay(pdMS_TO_TICKS(500));esp_restart();return ESP_OK;}
 else snprintf(o,sizeof(o),"Unknown command: %s\nType help.",c);
 httpd_resp_set_type(r,"text/plain; charset=utf-8");return httpd_resp_sendstr(r,o);
}
static esp_err_t update(httpd_req_t*r){const esp_partition_t*p=esp_ota_get_next_update_partition(NULL);if(!p){httpd_resp_send_err(r,500,"No OTA partition");return ESP_FAIL;}esp_ota_handle_t h;esp_err_t e=esp_ota_begin(p,OTA_WITH_SEQUENTIAL_WRITES,&h);if(e!=ESP_OK){httpd_resp_send_err(r,500,"OTA begin failed");return ESP_FAIL;}char buf[2048];int left=r->content_len;while(left>0){int want=left<(int)sizeof(buf)?left:sizeof(buf);int n=httpd_req_recv(r,buf,want);if(n<=0){esp_ota_abort(h);httpd_resp_send_err(r,500,"Upload interrupted; old firmware retained");return ESP_FAIL;}e=esp_ota_write(h,buf,n);if(e!=ESP_OK){esp_ota_abort(h);httpd_resp_send_err(r,500,"Flash write failed");return ESP_FAIL;}left-=n;}e=esp_ota_end(h);if(e!=ESP_OK){httpd_resp_send_err(r,400,"Invalid firmware image; old firmware retained");return ESP_FAIL;}e=esp_ota_set_boot_partition(p);if(e!=ESP_OK){httpd_resp_send_err(r,500,"Could not select firmware");return ESP_FAIL;}httpd_resp_sendstr(r,"Firmware installed; rebooting.");vTaskDelay(pdMS_TO_TICKS(700));esp_restart();return ESP_OK;}
static void start_server(void){httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.max_uri_handlers=4;c.stack_size=8192;if(httpd_start(&srv,&c)!=ESP_OK)return;httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=root},b={.uri="/api/info",.method=HTTP_GET,.handler=info},d={.uri="/api/cmd",.method=HTTP_POST,.handler=command},e={.uri="/api/update",.method=HTTP_POST,.handler=update};httpd_register_uri_handler(srv,&a);httpd_register_uri_handler(srv,&b);httpd_register_uri_handler(srv,&d);httpd_register_uri_handler(srv,&e);}
void app_main(void){esp_err_t e=nvs_flash_init();if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());ESP_ERROR_CHECK(nvs_flash_init());}ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_ap();wifi_init_config_t w=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&w));wifi_config_t ap={.ap={.ssid=AP_SSID,.ssid_len=sizeof(AP_SSID)-1,.channel=1,.password=AP_PASS,.max_connection=4,.authmode=WIFI_AUTH_WPA2_PSK}};ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));ESP_ERROR_CHECK(esp_wifi_start());start_server();ESP_LOGI("CYD-TERM","Join %s password %s, open http://192.168.4.1",AP_SSID,AP_PASS);}
