#include "ConsoleApp.h"
#include "ConsoleLogic.h"
#include "DeviceModel.h"
#include "LiveNodes.h"
#include "MusicEngine.h"
#include "RadioDisplay.h"
#include <Arduino.h>
#include <ESP_8_BIT_GFX.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <type_traits>
#include <time.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h>

#if ARDUINOJSON_VERSION_MAJOR != 6
#error "This sketch targets ArduinoJson 6.x. Select 6.21.5 in Library Manager."
#endif

namespace {
constexpr uint8_t BLACK=0x00, RED=0xE0, YELLOW=0xFC, GREEN=0x1C,
                  CYAN=0x1F, BLUE=0x03, MAGENTA=0xE3, WHITE=0xFF;
constexpr uint8_t palette[7] = {RED,YELLOW,GREEN,CYAN,BLUE,MAGENTA,WHITE};
constexpr int MSGEQ7_RESET=27, MSGEQ7_STROBE=26, MSGEQ7_OUT=34;
constexpr uint8_t PAGE_COUNT=8, PROBE_COUNT=9;
enum Page : uint8_t { NOW_PLAYING, QUEUE, NODES, HEALTH, SYSTEMS, CLOCK, MUSIC, TEST };
ConsoleConfig cfg;
ESP_8_BIT_GFX videoOut(true,8);

// Draw into an inset logical viewport. Text is reflowed, NOT reduced to an
// unreadable resampled bitmap. Every primitive is clipped at the safe boundary.
class SafeView : public Adafruit_GFX {
 public:
  explicit SafeView(ESP_8_BIT_GFX& out) : Adafruit_GFX(216,200), out_(out) {}
  void setBounds(int left,int right,int top,int bottom) {
    x_=left; y_=top; _width=256-left-right; _height=240-top-bottom;
  }
  void clear() { out_.fillScreen(BLACK); }
  void drawPixel(int16_t x,int16_t y,uint16_t color) override {
    if (x>=0 && y>=0 && x<_width && y<_height) out_.drawPixel(x_+x,y_+y,color);
  }
  void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c) override {
    if (y<0 || y>=_height || w<=0) return;
    if (x<0) { w+=x; x=0; }
    if (x+w>_width) w=_width-x;
    if (w>0) out_.drawFastHLine(x_+x,y_+y,w,c);
  }
  void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c) override {
    if (x<0 || x>=_width || h<=0) return;
    if (y<0) { h+=y; y=0; }
    if (y+h>_height) h=_height-y;
    if (h>0) out_.drawFastVLine(x_+x,y_+y,h,c);
  }
  void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c) override {
    if (x<0) { w+=x; x=0; } if (y<0) { h+=y; y=0; }
    if (x+w>_width) w=_width-x;
    if (y+h>_height) h=_height-y;
    if (w>0 && h>0) out_.fillRect(x_+x,y_+y,w,h,c);
  }
 private:
  ESP_8_BIT_GFX& out_;
  int x_=20,y_=20;
};
SafeView view(videoOut);
crt::Navigation nav;

std::atomic<uint8_t> requestedPage{NOW_PLAYING};
uint32_t pageEntered=0, overlayStarted=0;
bool overlayActive=false, netStarted=false;

// Fixed-size, trivially-copyable snapshots are sent through FreeRTOS queues.
// The network task never mutates Strings or framebuffer state being rendered.
struct CacheInfo {
  bool have=false, ok=false;
  uint32_t sampled=0, attempted=0;
  int http=0;
  char error[48]="NOT YET READ";
};
struct HouseData {
  CacheInfo info;
  char transport[12]="unknown",title[160]="",artist[96]="",album[96]="";
  char file[256]="",defaultFolder[32]="?";
  float elapsed=0,duration=0;
  int volume=-1,queueLength=0,songPosition=-1,songId=-1;
  bool repeat=false,random=false;
  // Live radio is a different source, not a zero-duration MP3. Only keep
  // bounded presentation fields; never retain a complete media-stream URL.
  bool isRadio=false,radioSaved=false;
  char radioStation[120]="",radioStatus[20]="",radioHost[76]="";
  char radioError[96]="",radioFormat[28]="";
  int radioBitrate=-1,radioRetryAttempt=0;
  float radioRetryIn=-1.0f;
};
struct NodesData {
  CacheInfo info, namesInfo;
  bool snapReachable=false;
  int present=0,audible=0,controllers=0,passive=0;
  crt::NodeViews list;
};
struct QueueItem {
  // The queue screen needs title/artist/position, not a second full pathname.
  char title[128]="", artist[96]="";
  int pos=-1,id=-1;
};
constexpr int QUEUE_ITEMS=5;
struct QueueData {
  CacheInfo info;
  QueueItem item[QUEUE_ITEMS];
  int count=0,startPosition=0,queueLength=0;
  bool random=false;
};
struct HealthData {
  CacheInfo info, sessionInfo;
  char version[20]="", status[20]="", mpdError[80]="", snapError[80]="";
  bool mpd=false,snap=false,ready=false;
  bool autoPaused=false,pendingStop=false,policyEnabled=false;
  char pauseReason[80]="", lastAction[80]="", defaultFolder[32]="";
};
struct ProbeData {
  bool checked=false,ok=false,checking=false;
  uint8_t misses=0;
  uint32_t sampled=0,attempted=0;
};
struct SystemsData { ProbeData probe[PROBE_COUNT]; };
static_assert(std::is_trivially_copyable<HouseData>::value,"Queue data must be POD");
static_assert(std::is_trivially_copyable<NodesData>::value,"Queue data must be POD");
static_assert(std::is_trivially_copyable<QueueData>::value,"Queue data must be POD");
static_assert(std::is_trivially_copyable<HealthData>::value,"Queue data must be POD");
HouseData house;
QueueData trackQueueData;
NodesData nodes;
HealthData health;
SystemsData systems;
float displayedElapsed=0;
QueueHandle_t stateQueue=nullptr,trackQueue=nullptr,nodesQueue=nullptr,healthQueue=nullptr,probeQueue=nullptr;
TaskHandle_t netTaskHandle=nullptr;

struct ProbeTarget { const char* label; const char* host; uint16_t port; };
ProbeTarget targets[PROBE_COUNT];

template<size_t N> void copyText(char (&dst)[N],const char* src) {
  snprintf(dst,N,"%s",src ? src : "");
}
void fail(CacheInfo& info,const char* why,int http=0) {
  info.ok=false; info.http=http; info.attempted=millis(); copyText(info.error,why);
}
void success(CacheInfo& info) {
  info.have=info.ok=true; info.http=200;
  info.sampled=info.attempted=millis(); info.error[0]=0;
}

// A bounded buffered reader avoids slow byte-at-a-time socket reads and an
// unbounded parse. It runs ONLY in the network task, never in the display loop.
class JsonReader {
 public:
  explicit JsonReader(WiFiClient& client) : client_(client) {
    started_=lastByte_=millis();
  }
  int read() {
    if (pos_<used_) return buffer_[pos_++];
    while (true) {
      uint32_t now=millis();
      if (crt::age(now,started_)>=6000 || crt::age(now,lastByte_)>=2500) {
        timedOut=true; return -1;
      }
      if (bytes_>=131072) { tooLarge=true; return -1; }
      int n=client_.available();
      if (n>0) {
        if (n>int(sizeof(buffer_))) n=sizeof(buffer_);
        int got=client_.read(buffer_,n);
        if (got>0) {
          used_=got; pos_=1; bytes_+=got; lastByte_=now;
          return buffer_[0];
        }
      } else if (!client_.connected()) return -1;
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  size_t readBytes(char* dst,size_t size) {
    size_t n=0; int c;
    while (n<size && (c=read())>=0) dst[n++]=char(c);
    return n;
  }
  bool timedOut=false,tooLarge=false;
 private:
  WiFiClient& client_;
  uint8_t buffer_[256];
  int pos_=0,used_=0;
  uint32_t started_=0,lastByte_=0,bytes_=0;
};

bool getJson(const char* path,DynamicJsonDocument& doc,
             const JsonDocument& filter,CacheInfo& info) {
  if (WiFi.status()!=WL_CONNECTED) { fail(info,"WIFI OFFLINE"); return false; }
  if (doc.capacity()==0) { fail(info,"JSON BUFFER ALLOCATION FAILED"); return false; }
  WiFiClient client;
  HTTPClient http;
  char url[140];
  snprintf(url,sizeof(url),"http://%s:%u%s",cfg.pi,cfg.housePort,path);
  http.useHTTP10(true);   // Python server sends Content-Length, not chunked data.
  http.setReuse(false);
  http.setConnectTimeout(650);
  http.setTimeout(2500);
  if (!http.begin(client,url)) { fail(info,"HTTP INITIALIZATION FAILED"); return false; }
  http.addHeader("Accept","application/json");
  http.addHeader("Accept-Encoding","identity");
  int result=http.GET();
  if (result!=200) {
    char why[48];
    snprintf(why,sizeof(why),result>0 ? "HTTP %d" : "CONNECTION ERROR %d",result);
    fail(info,why,result); http.end(); return false;
  }
  if (http.getSize()>131072) {
    fail(info,"RESPONSE TOO LARGE",result); http.end(); return false;
  }
  JsonReader reader(client);
  DeserializationError error=deserializeJson(doc,reader,
      DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(12));
  http.end();
  if (error) {
    char why[48];
    if (reader.timedOut) snprintf(why,sizeof(why),"JSON READ TIMEOUT");
    else if (reader.tooLarge) snprintf(why,sizeof(why),"RESPONSE TOO LARGE");
    else snprintf(why,sizeof(why),"JSON %s",error.c_str());
    fail(info,why,result); return false;
  }
  return true;
}

void readHouse(DynamicJsonDocument& doc,StaticJsonDocument<2048>& f,HouseData& out) {
  f.clear(); f["service"]=true;
  const char* fields[]={"transport","elapsedSeconds","durationSeconds","volume",
    "queueLength","songPosition","songId","repeat","random", "bitrate", "audio", "error"};
  for (const char* key:fields) f["mpd"][key]=true;
  const char* tags[]={"file","title","artist","album","durationSeconds","stationName"};
  for (const char* key:tags) f["mpd"]["song"][key]=true;
  // v0.10+ exposes explicit radio state. Only whitelist the few fields the
  // CRT needs so the existing 12-KB JSON workspace is not enlarged.
  const char* radioFields[]={"type","status","lastError","retryAttempt",
                             "retryInSeconds","stationSaved"};
  for (const char* key:radioFields) f["source"][key]=true;
  f["source"]["station"]["name"]=true;
  f["source"]["station"]["url"]=true;
  f["sessionPolicy"]["defaultFolder"]=true;
  if (!getJson("/state",doc,f,out.info)) return;
  JsonObjectConst m=doc["mpd"].as<JsonObjectConst>();
  const char* service=doc["service"] | "";
  if (strcmp(service,"house-audio-server") || m.isNull() || !m["transport"].is<const char*>()) {
    fail(out.info,"UNEXPECTED STATE RESPONSE",200); return;
  }
  copyText(out.transport,m["transport"] | "unknown");
  out.elapsed=m["elapsedSeconds"] | 0.0f;
  out.duration=m["durationSeconds"] | 0.0f;
  out.volume=m["volume"] | -1;
  out.queueLength=m["queueLength"] | 0;
  out.songPosition=m["songPosition"] | -1;
  out.songId=m["songId"] | -1;
  out.repeat=m["repeat"] | false; out.random=m["random"] | false;
  JsonObjectConst s=m["song"].as<JsonObjectConst>();
  JsonObjectConst source=doc["source"].as<JsonObjectConst>();
  copyText(out.file,s["file"] | ""); copyText(out.title,s["title"] | "");
  copyText(out.artist,s["artist"] | ""); copyText(out.album,s["album"] | "");
  const char* explicitType=source["type"] | "";
  out.isRadio=crt::radio::sourceIsRadio(explicitType,out.file);
  if(out.isRadio) {
    // Logical radio status matters: MPD reports STOP even for a radio pause.
    copyText(out.radioStatus,source["status"] | (m["transport"] | "unknown"));
    out.radioSaved=source["stationSaved"] | false;
    out.radioBitrate=m["bitrate"] | -1;
    copyText(out.radioFormat,m["audio"] | "");
    out.radioRetryAttempt=source["retryAttempt"] | 0;
    out.radioRetryIn=source["retryInSeconds"] | -1.0f;
    copyText(out.radioError,source["lastError"] | (m["error"] | ""));
    JsonObjectConst station=source["station"].as<JsonObjectConst>();
    const char* url=station["url"] | static_cast<const char*>(out.file);
    crt::radio::hostFromUrl(url,out.radioHost,sizeof(out.radioHost));
    copyText(out.radioStation,station["name"] | (s["stationName"] | ""));
    if(!out.radioStation[0]) copyText(out.radioStation,
      out.radioHost[0] ? out.radioHost : "Internet Radio");
    char artist[sizeof(out.artist)],title[sizeof(out.title)];
    crt::radio::cleanMetadata(out.artist,out.title,
      artist,sizeof(artist),title,sizeof(title));
    copyText(out.artist,artist); copyText(out.title,title);
    // A URL is an identity, not a song title. No basename fallback for radio.
    if(crt::radio::isStreamUrl(out.title)) out.title[0]=0;
  } else {
    out.radioSaved=false; out.radioStation[0]=out.radioStatus[0]=0;
    out.radioHost[0]=out.radioError[0]=out.radioFormat[0]=0;
    out.radioBitrate=-1; out.radioRetryAttempt=0; out.radioRetryIn=-1;
    if(!out.title[0] && out.file[0]) {
      const char* name=strrchr(out.file,'/'); copyText(out.title,name ? name+1 : out.file);
      char* dot=strrchr(out.title,'.'); if(dot) *dot=0;
    }
    if(out.duration<=0) out.duration=s["durationSeconds"] | 0.0f;
  }
  copyText(out.defaultFolder,doc["sessionPolicy"]["defaultFolder"] | "?");
  success(out.info);
}


bool readMpdLine(WiFiClient& client,char* out,size_t cap,uint32_t timeoutMs=1600) {
  if(!out || cap<2) return false;
  size_t n=0; uint32_t started=millis();
  while(crt::age(millis(),started)<timeoutMs) {
    while(client.available()) {
      int c=client.read();
      if(c<0) break;
      if(c=='\n') {
        if(n && out[n-1]=='\r') --n;
        out[n]=0; return true;
      }
      if(n+1<cap) out[n++]=char(c);
    }
    if(!client.connected() && !client.available()) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  out[n]=0; return false;
}

const char* mpdValue(const char* line,const char* key) {
  size_t n=strlen(key);
  if(strncasecmp(line,key,n) || line[n]!=':') return nullptr;
  const char* p=line+n+1;
  while(*p==' ') ++p;
  return p;
}

void readTrackQueue(QueueData& out,const HouseData& state) {
  out.info.attempted=millis();
  out.count=0;
  out.queueLength=state.queueLength;
  out.random=state.random;
  out.startPosition=state.songPosition>=0 ? state.songPosition : 0;
  // Page 2 becomes RADIO INFO for an active live source. Never ask MPD for
  // an arbitrary playlistinfo range or show a fictitious one-track queue.
  if(state.info.have && state.isRadio) { success(out.info); return; }

  if(WiFi.status()!=WL_CONNECTED) { fail(out.info,"WIFI OFFLINE"); return; }
  if(!state.info.have || state.queueLength<=0) { success(out.info); return; }

  int start=state.songPosition>=0 ? state.songPosition : 0;
  if(start>=state.queueLength) start=state.queueLength-1;
  if(start<0) start=0;
  int end=start+QUEUE_ITEMS;
  if(end>state.queueLength) end=state.queueLength;
  out.startPosition=start;

  IPAddress ip;
  if(!ip.fromString(cfg.pi)) { fail(out.info,"BAD MPD ADDRESS"); return; }
  WiFiClient client;
  if(!client.connect(ip,6600,700)) { fail(out.info,"MPD CONNECTION FAILED"); return; }

  char line[360];
  if(!readMpdLine(client,line,sizeof(line)) || strncmp(line,"OK MPD ",7)) {
    client.stop(); fail(out.info,"MPD GREETING FAILED"); return;
  }

  char command[64];
  snprintf(command,sizeof(command),"playlistinfo %d:%d\n",start,end);
  client.print(command);

  int current=-1;
  bool gotOk=false;
  while(readMpdLine(client,line,sizeof(line),2000)) {
    if(!strcmp(line,"OK")) { gotOk=true; break; }
    if(!strncmp(line,"ACK ",4)) { client.stop(); fail(out.info,"MPD QUEUE ERROR"); return; }

    const char* v=nullptr;
    if((v=mpdValue(line,"file"))) {
      if(out.count>=QUEUE_ITEMS) continue;
      current=out.count++;
      out.item[current]=QueueItem();
      const char* base=strrchr(v,'/');
      copyText(out.item[current].title,base ? base+1 : v);
      char* dot=strrchr(out.item[current].title,'.'); if(dot) *dot=0;
      continue;
    }
    if(current<0 || current>=QUEUE_ITEMS) continue;
    QueueItem& item=out.item[current];
    if((v=mpdValue(line,"Title"))) { if(*v) copyText(item.title,v); }
    else if((v=mpdValue(line,"Artist"))) copyText(item.artist,v);
    else if((v=mpdValue(line,"Pos"))) item.pos=atoi(v);
    else if((v=mpdValue(line,"Id"))) item.id=atoi(v);

  }
  client.stop();
  if(!gotOk) { fail(out.info,"MPD QUEUE TIMEOUT"); return; }
  success(out.info);
}

bool readRendererRecords(DynamicJsonDocument& doc,StaticJsonDocument<2048>& f,
                         crt::RendererRecords& out,CacheInfo& info) {
  f.clear(); f["service"]=true; f["snapserver"]["reachable"]=true;
  const char* keys[]={"id","name","present","audible","muted","groupMuted","volumePercent","latencyMs"};
  for(const char* key:keys) f["snapserver"]["clients"][0][key]=true;
  f["snapserver"]["clients"][0]["host"]["ip"]=true;
  if(!getJson("/renderers",doc,f,info)) return false;
  JsonObjectConst s=doc["snapserver"].as<JsonObjectConst>();
  if(strcmp(doc["service"] | "","house-audio-server") || s.isNull() ||
      !s["reachable"].is<bool>() || !s["clients"].is<JsonArrayConst>()) {
    fail(info,"UNEXPECTED RENDERERS RESPONSE",200); return false;
  }
  out.reachable=s["reachable"] | false; out.count=0; out.complete=true;
  JsonArrayConst clients=s["clients"].as<JsonArrayConst>(); out.sourceCount=clients.size();
  for(JsonObjectConst c:clients) {
    const char* id=c["id"] | "";
    if(!*id || strlen(id)>128) { out.complete=false; continue; }
    if(out.count>=crt::MAX_RENDERERS) { out.complete=false; break; }
    crt::RendererRecord& r=out.row[out.count++]; r=crt::RendererRecord();
    copyText(r.id,id); copyText(r.name,c["name"] | id); copyText(r.ip,c["host"]["ip"] | "");
    r.present=c["present"] | false; r.audible=c["audible"] | false;
    r.muted=(c["muted"] | false)||(c["groupMuted"] | false);
    r.volume=c["volumePercent"] | -1;
    r.hasLatency=!c["latencyMs"].isNull(); r.latency=c["latencyMs"] | 0;
  }
  success(info); return true;
}
bool readPresence(DynamicJsonDocument& doc,StaticJsonDocument<2048>& f,
                  crt::PresenceRecords& out,CacheInfo& info) {
  f.clear(); f["service"]=true;
  const char* counts[]={"presentCount","audibleCount","controllerCount","passiveCount","snapserverReachable"};
  for(const char* k:counts) f["presence"][k]=true;
  const char* ck[]={"controllerId","rendererId","present","outputMuted","outputReady"};
  for(const char* k:ck) f["presence"]["controllers"][0][k]=true;
  const char* rk[]={"rendererId","controllerId","passive","present","audible"};
  for(const char* k:rk) f["presence"]["renderers"][0][k]=true;
  if(!getJson("/controllers",doc,f,info)) return false;
  JsonObjectConst p=doc["presence"].as<JsonObjectConst>();
  if(strcmp(doc["service"] | "","house-audio-server") || p.isNull() ||
      !p["presentCount"].is<int>() || !p["audibleCount"].is<int>() ||
      !p["controllers"].is<JsonArrayConst>() || !p["renderers"].is<JsonArrayConst>() ||
      (p["presentCount"].as<int>()<0) || (p["audibleCount"].as<int>()<0)) {
    fail(info,"UNEXPECTED PRESENCE RESPONSE",200); return false;
  }
  out.controllers=out.bindings=0; out.complete=true;
  out.presentCount=p["presentCount"] | 0; out.audibleCount=p["audibleCount"] | 0;
  out.controllerCount=p["controllerCount"] | 0; out.passiveCount=p["passiveCount"] | 0;
  out.snapReachable=p["snapserverReachable"] | false;
  for(JsonObjectConst c:p["controllers"].as<JsonArrayConst>()) {
    const char* id=c["controllerId"] | ""; const char* rid=c["rendererId"] | "";
    if(!*id || strlen(id)>128 || strlen(rid)>128) { out.complete=false; continue; }
    if(out.controllers>=crt::MAX_CONTROLLERS) { out.complete=false; break; }
    crt::ControllerRecord& r=out.controller[out.controllers++]; r=crt::ControllerRecord();
    copyText(r.id,id); copyText(r.rendererId,rid);
    r.present=c["present"] | false; r.muted=c["outputMuted"] | true; r.ready=c["outputReady"] | false;
  }
  for(JsonObjectConst b:p["renderers"].as<JsonArrayConst>()) {
    const char* id=b["rendererId"] | ""; const char* owner=b["controllerId"] | "";
    if(!*id || strlen(id)>128 || strlen(owner)>128) { out.complete=false; continue; }
    if(out.bindings>=crt::MAX_RENDERERS) { out.complete=false; break; }
    crt::OwnershipRecord& r=out.binding[out.bindings++]; r=crt::OwnershipRecord();
    copyText(r.rendererId,id); copyText(r.owner,owner);
    r.present=b["present"] | false; r.audible=b["audible"] | false; r.passive=b["passive"] | true;
  }
  success(info); return true;
}
void publishNodes(NodesData& n,const crt::RendererRecords& r,const crt::PresenceRecords& p) {
  if(n.info.have) {
    n.present=p.presentCount; n.audible=p.audibleCount;
    n.controllers=p.controllerCount; n.passive=p.passiveCount; n.snapReachable=p.snapReachable;
    crt::buildLiveViews(r,p,n.list);
  }
  xQueueOverwrite(nodesQueue,&n);
}
void readHealth(DynamicJsonDocument& doc,StaticJsonDocument<2048>& f,HealthData& out) {
  f.clear(); f["service"]=true; f["version"]=true; f["status"]=true;
  f["mpd"]["reachable"]=true; f["mpd"]["error"]=true;
  f["snapserver"]["reachable"]=true; f["snapserver"]["error"]=true;
  f["startup"]["ready"]=true;
  if(!getJson("/health",doc,f,out.info)) return;
  if(strcmp(doc["service"] | "","house-audio-server") ||
     !doc["mpd"]["reachable"].is<bool>() || !doc["snapserver"]["reachable"].is<bool>() ||
     !doc["startup"]["ready"].is<bool>()) {
    fail(out.info,"UNEXPECTED HEALTH RESPONSE",200); return;
  }
  copyText(out.version,doc["version"] | "?"); copyText(out.status,doc["status"] | "unknown");
  out.mpd=doc["mpd"]["reachable"] | false; out.snap=doc["snapserver"]["reachable"] | false;
  out.ready=doc["startup"]["ready"] | false;
  copyText(out.mpdError,doc["mpd"]["error"] | ""); copyText(out.snapError,doc["snapserver"]["error"] | "");
  success(out.info);
}
void readSession(DynamicJsonDocument& doc,StaticJsonDocument<2048>& f,HealthData& out) {
  f.clear(); f["service"]=true;
  const char* keys[]={"enabled","autoPaused","pauseReason","pendingFinalStop","lastAction","defaultFolder"};
  for(const char* k:keys) f["sessionPolicy"][k]=true;
  if(!getJson("/session",doc,f,out.sessionInfo)) return;
  JsonObjectConst s=doc["sessionPolicy"].as<JsonObjectConst>();
  if(strcmp(doc["service"] | "","house-audio-server") || s.isNull() || !s["autoPaused"].is<bool>()) {
    fail(out.sessionInfo,"UNEXPECTED SESSION RESPONSE",200); return;
  }
  out.policyEnabled=s["enabled"] | false; out.autoPaused=s["autoPaused"] | false;
  out.pendingStop=s["pendingFinalStop"] | false;
  copyText(out.pauseReason,s["pauseReason"] | ""); copyText(out.lastAction,s["lastAction"] | "");
  copyText(out.defaultFolder,s["defaultFolder"] | "?"); success(out.sessionInfo);
}

bool probeTcp(const ProbeTarget& target,uint16_t timeoutMs) {
  IPAddress ip;
  if(WiFi.status()!=WL_CONNECTED || !ip.fromString(target.host)) return false;
  WiFiClient client; bool ok=client.connect(ip,target.port,timeoutMs); client.stop(); return ok;
}
void probeOne(SystemsData& p,uint8_t index) {
  ProbeData& item=p.probe[index];
  uint32_t now=millis();
  uint32_t interval=item.checking ? 500 : 15000;
  if(item.checked && !item.checking && crt::age(now,item.sampled)<interval) return;
  if(item.checking && crt::age(now,item.attempted)<interval) return;
  item.checking=true; item.attempted=now; xQueueOverwrite(probeQueue,&p);
  bool ok=probeTcp(targets[index],index==7 ? cfg.garageTimeoutMs:650);
  if(WiFi.status()!=WL_CONNECTED) { item.checking=false; xQueueOverwrite(probeQueue,&p); return; }
  item.attempted=millis();
  if(ok) { item.ok=true; item.checked=true; item.misses=0; item.checking=false; item.sampled=millis(); }
  else if(++item.misses>=2) { item.ok=false; item.checked=true; item.checking=false; item.sampled=millis(); item.misses=0; }
  xQueueOverwrite(probeQueue,&p);
}

// Lightweight diagnostics stay on the CRT. No password logging or flash erase.
std::atomic<uint32_t> failedAllocBytes{0},wifiAttempts{0};
std::atomic<int> wifiLastReason{0};
std::atomic<bool> wifiModeOk{false},wifiBeginOk{false},wifiAssociated{false};
void failedAllocHook(size_t bytes,uint32_t,const char*) {
  failedAllocBytes.store(uint32_t(bytes),std::memory_order_relaxed);
}
void onWifiEvent(WiFiEvent_t event,WiFiEventInfo_t info) {
  // Called on the Arduino event task. Only atomic observations here.
  if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED) wifiAssociated.store(true);
  else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiAssociated.store(true); wifiLastReason.store(0);
  } else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiAssociated.store(false);
    wifiLastReason.store(int(info.wifi_sta_disconnected.reason));
  } else if(event==ARDUINO_EVENT_WIFI_STA_STOP) wifiAssociated.store(false);
}
void startWifiAttempt() {
  wifiAttempts.fetch_add(1);
  wifiBeginOk.store(false);
  bool enabled=WiFi.mode(WIFI_STA); wifiModeOk.store(enabled);
  if(!enabled) return; // Do not pretend the driver started successfully.
  WiFi.setSleep(false); WiFi.setAutoReconnect(true);
  int result=int(WiFi.begin(cfg.ssid,cfg.password));
  wifiBeginOk.store(result!=WL_CONNECT_FAILED && result!=WL_NO_SHIELD);
}
void retryWifi() {
  if(!wifiModeOk.load() || !wifiBeginOk.load()) {startWifiAttempt();return;}
  wifiAttempts.fetch_add(1);
  if(!WiFi.reconnect()) startWifiAttempt();
}
const char* wifiProblem() {
  if(!wifiModeOk.load()) return "WIFI INITIALIZATION FAILED";
  if(!wifiBeginOk.load()) return "WIFI START REJECTED";
  if(wifiAssociated.load()) return "WIFI: WAITING FOR IP ADDRESS";
  int reason=wifiLastReason.load();
  // The reason is evidence, NOT proof that a password is wrong.
  if(reason==201) return "WIFI: ACCESS POINT NOT FOUND";
  if(reason==202 || reason==15 || reason==204) return "WIFI: AUTH / HANDSHAKE FAILURE";
  if(reason) return "WIFI DISCONNECTED";
  return "WIFI CONNECTING";
}

// Fair round-robin scheduling. Slow/offline endpoints cannot starve later jobs.
// Video/button/audio stay in loop() on core 1; this worker only touches snapshots.
void networkTask(void*) {
  // Reserve no JSON workspace while Wi-Fi is joining/rejoining.
  DynamicJsonDocument doc(0);
  StaticJsonDocument<2048> filter;
  // Static worker-owned storage: large record arrays must not consume task stack.
  static HouseData h;
  static QueueData q;
  static NodesData n;
  static HealthData healthData;
  static SystemsData p;
  static crt::RendererRecords renderers;
  static crt::PresenceRecords presence;
  uint32_t last[6]; const uint32_t periods[6]={2000,2000,6000,3000,6000,6000};
  for(int i=0;i<6;i++) last[i]=millis()-periods[i];
  uint32_t retryLast=millis(); uint8_t next=0,probeIndex=0,previousPage=255;
  bool clockStarted=false;
  uint32_t jsonRetry=millis()-5000;
  while(true) {
    uint32_t now=millis();
    if(WiFi.status()!=WL_CONNECTED) {
      // Retain the successfully allocated workspace across Wi-Fi drops.
      // Releasing/reallocating it after each reconnect caused late-life
      // contiguous-heap allocation failures on the physical ESP32.
      // If initial allocation has not yet succeeded, retry after association.
      if(crt::age(now,retryLast)>=15000) { retryWifi(); retryLast=now; }
      vTaskDelay(pdMS_TO_TICKS(50)); continue;
    }
    retryLast=now;
    if(!doc.capacity() && crt::age(now,jsonRetry)>=5000) {
      doc=DynamicJsonDocument(12288); jsonRetry=now;
    }
    if(!clockStarted) { configTzTime(cfg.timezone,"pool.ntp.org","time.nist.gov"); clockStarted=true; }
    uint8_t page=requestedPage.load(std::memory_order_relaxed);
    if(page!=previousPage) {
      if(page==QUEUE) last[3]=now-periods[3];
      if(page==HEALTH) { last[4]=now-periods[4]; last[5]=now-periods[5]; }
      if(page==NODES) last[2]=now-periods[2];
      previousPage=page;
    }
    for(int attempt=0;attempt<7;attempt++) {
      uint8_t job=next; next=(next+1)%7;
      if(job<6 && crt::age(now,last[job])<periods[job]) continue;
      if(job==3 && page!=QUEUE) continue;
      if((job==4 || job==5) && page!=HEALTH) continue;
      if(job==6 && page!=SYSTEMS) continue;
      switch(job) {
        case 0: readHouse(doc,filter,h); xQueueOverwrite(stateQueue,&h); break;
        case 1: readPresence(doc,filter,presence,n.info); publishNodes(n,renderers,presence); break;
        case 2: readRendererRecords(doc,filter,renderers,n.namesInfo); publishNodes(n,renderers,presence); break;
        case 3: readTrackQueue(q,h); xQueueOverwrite(trackQueue,&q); break;
        case 4: readHealth(doc,filter,healthData); xQueueOverwrite(healthQueue,&healthData); break;
        case 5: readSession(doc,filter,healthData); xQueueOverwrite(healthQueue,&healthData); break;
        case 6: probeOne(p,probeIndex); probeIndex=(probeIndex+1)%PROBE_COUNT; break;
      }
      if(job<6) last[job]=millis();
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Audio runs on every page, independently of network requests. No boot capture.
crt::MusicEngine audio;
crt::MusicMotion motion;
bool showMusic=false;
uint16_t rawBand[7]={0};
int visualMode=0;
uint32_t styleChanged=0;

void readMSGEQ7() {
  digitalWrite(MSGEQ7_STROBE,HIGH);
  digitalWrite(MSGEQ7_RESET,HIGH); delayMicroseconds(5);
  digitalWrite(MSGEQ7_RESET,LOW); delayMicroseconds(80);
  for(int i=0;i<7;i++) {
    digitalWrite(MSGEQ7_STROBE,LOW); delayMicroseconds(40);
    // Median of three settled readings rejects a single ADC spike.
    int a=analogRead(MSGEQ7_OUT),b=analogRead(MSGEQ7_OUT),c=analogRead(MSGEQ7_OUT);
    if(a>b) { int t=a;a=b;b=t; } if(b>c) { int t=b;b=c;c=t; }
    if(a>b) b=a;
    rawBand[i]=uint16_t(b);
    digitalWrite(MSGEQ7_STROBE,HIGH); delayMicroseconds(40);
  }
}
void updateAudio(uint32_t now) {
  readMSGEQ7(); audio.update(rawBand,now); motion=audio.motion; showMusic=audio.visible;
  if(audio.signal && crt::age(now,styleChanged)>=15000) {
    visualMode=(visualMode+1)%3; styleChanged=now;
  }
}

// ---------------------------------------------------------------------------
// Drawing helpers -- coordinates below are relative to the CRT safe area.
// ---------------------------------------------------------------------------
String printable(const char* src) {
  String out;
  if (!src) return out;
  while (*src) {
    unsigned char c=*src++;
    if (c>=32 && c<127) out+=char(c);
    else if (c<32) out+=' ';
    else if (c>=0xC0) out+='?'; // One placeholder per UTF-8 character, not per byte.
  }
  return out;
}
String clipped(const String& text,int count) {
  if (count<=0) return String();
  if (int(text.length())<=count) return text;
  if (count<4) return text.substring(0,count);
  return text.substring(0,count-3)+"...";
}
void textAt(int x,int y,const String& text,uint8_t color=WHITE,uint8_t size=1) {
  if (x<0 || y<0 || size<1 || y+8*size>view.height()) return;
  int count=(view.width()-x)/int(6*size);
  view.setTextWrap(false); view.setTextSize(size); view.setTextColor(color);
  view.setCursor(x,y); view.print(clipped(text,count));
}
void centerText(int y,const String& text,uint8_t color=WHITE,uint8_t size=1) {
  String line=clipped(text,(view.width()-8)/int(6*size));
  textAt((view.width()-int(line.length())*6*size)/2,y,line,color,size);
}
// Keep body anchors between the header and footer when margins are adjusted.
int by(int y) { return 28+(y-28)*(view.height()-48)/152; }
void header(const char* title) {
  view.clear();
  uint8_t size=strlen(title)*12U<=unsigned(view.width()-8) ? 2 : 1;
  textAt(4,4,title,WHITE,size);
  view.drawFastHLine(2,24,view.width()-4,CYAN);
}
void footer(uint32_t now) {
  int y=view.height()-10;
  view.fillRect(1,y-2,view.width()-2,11,BLACK);
  char line[48];
  uint32_t progress=nav.holdProgress(now);
  if (progress>=300) {
    snprintf(line,sizeof(line),"HOLD %lu/6 SEC FOR AUTO",(unsigned long)(progress/1000));
  } else {
    if(nav.automatic) snprintf(line,sizeof(line),"%u/%u  AUTO 6 SEC",nav.page+1,PAGE_COUNT);
    else snprintf(line,sizeof(line),"%u/%u  MANUAL  TAP=NEXT",nav.page+1,PAGE_COUNT);
  }
  textAt(4,y,line,CYAN);
}
String cacheState(const CacheInfo& info,uint32_t now,uint32_t freshMs) {
  if (!netStarted) return "NETWORK TASK NOT RUNNING";
  if (WiFi.status()!=WL_CONNECTED) return wifiProblem();
  if (!info.have) return info.error;
  if (!info.ok) return String("STALE: ")+info.error;
  if (crt::age(now,info.sampled)>freshMs) return "STALE: WAITING FOR UPDATE";
  return String();
}
void drawWifiDetails() {
  char line[72];
  snprintf(line,sizeof(line),"LAST REASON %d   ATTEMPT %lu",wifiLastReason.load(),
           (unsigned long)wifiAttempts.load());
  centerText(by(137),line,YELLOW);
  snprintf(line,sizeof(line),"RAM FREE %lu / BLOCK %lu",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  centerText(by(152),line,CYAN);
  uint32_t failed=failedAllocBytes.load();
  if(failed) {
    snprintf(line,sizeof(line),"LAST FAILED ALLOCATION: %lu B",(unsigned long)failed);
    centerText(by(167),line,YELLOW);
  }
}
bool emptyPage(const CacheInfo& info,uint32_t now,uint32_t freshMs) {
  if (info.have) return false;
  centerText(by(90),cacheState(info,now,freshMs),YELLOW);
  centerText(by(111),"RETRYING AUTOMATICALLY",CYAN);
  if(WiFi.status()!=WL_CONNECTED) drawWifiDetails();
  return true;
}
String durationText(float seconds) {
  if (!isfinite(seconds) || seconds<0) seconds=0;
  if (seconds>2147483000.0f) seconds=2147483000.0f;
  unsigned long s=(unsigned long)seconds;
  char text[24]; snprintf(text,sizeof(text),"%lu:%02lu",s/60,s%60);
  return text;
}
String ageText(float seconds) {
  if (!isfinite(seconds) || seconds<0) return "?";
  char out[24];
  if (seconds>=86400) snprintf(out,sizeof(out),"%.1fd",seconds/86400.0f);
  else if (seconds>=3600) snprintf(out,sizeof(out),"%.1fh",seconds/3600.0f);
  else if (seconds>=60) snprintf(out,sizeof(out),"%.1fm",seconds/60.0f);
  else snprintf(out,sizeof(out),"%.2fs",seconds);
  return out;
}
void titleBlock(const char* raw) {
  String title=printable(*raw ? raw : "(no track selected)");
  int columns=(view.width()-8)/6;
  if (int(title.length())<=columns/2) { textAt(4,by(46),title,WHITE,2); return; }
  int cut=columns;
  if (int(title.length())>columns) {
    int space=title.lastIndexOf(' ',columns);
    if (space>=columns/2) cut=space;
  }
  textAt(4,by(45),title.substring(0,cut));
  String second=title.substring(cut); second.trim();
  textAt(4,by(58),clipped(second,columns));
}
String radioStatusText() {
  String status=house.radioStatus;
  status.toUpperCase();
  if(status=="PLAYING") return "LIVE";
  if(status=="RETRYING") return "RECONNECTING";
  if(!status.length()) return "RADIO";
  return status;
}
uint8_t radioStatusColor() {
  if(!strcasecmp(house.radioStatus,"playing")) return GREEN;
  if(!strcasecmp(house.radioStatus,"error")) return RED;
  return YELLOW;
}
void radioStationBlock(int y) {
  String name=printable(house.radioStation);
  int columns=(view.width()-8)/6;
  if(int(name.length())<=columns) {
    textAt(4,by(y),name,WHITE);
    return;
  }
  int cut=columns;
  int space=name.lastIndexOf(' ',columns);
  if(space>=columns/2) cut=space;
  textAt(4,by(y),name.substring(0,cut),WHITE);
  String rest=name.substring(cut); rest.trim();
  textAt(4,by(y+12),rest,WHITE);
}
void drawRadioNowPlaying(uint32_t now) {
  String warning=cacheState(house.info,now,7000);
  String status=String("RADIO  ")+radioStatusText();
  textAt(4,by(31),warning.length()?warning:status,
         warning.length()?YELLOW:radioStatusColor());
  textAt(4,by(46),"STATION",CYAN);
  radioStationBlock(58);
  if(house.artist[0]) textAt(4,by(85),printable(house.artist),CYAN);
  if(house.title[0]) {
    String title=printable(house.title);
    int chars=(view.width()-8)/12;
    if(int(title.length())<=chars) textAt(4,by(101),title,WHITE,2);
    else {
      int cols=(view.width()-8)/6;
      int cut=cols;
      int space=title.lastIndexOf(' ',cols);
      if(space>=cols/2) cut=space;
      textAt(4,by(98),title.substring(0,cut),WHITE);
      String remainder=title.substring(cut); remainder.trim();
      textAt(4,by(111),remainder,WHITE);
    }
  } else textAt(4,by(104),"Track details unavailable",YELLOW);
  char line[128];
  if(house.radioBitrate>0)
    snprintf(line,sizeof(line),"LIVE STREAM   %d kbps",house.radioBitrate);
  else snprintf(line,sizeof(line),"LIVE STREAM   NO SEEK / DURATION");
  // Avoid announcing a live stream while connecting/paused/retrying.
  if(strcasecmp(house.radioStatus,"playing"))
    snprintf(line,sizeof(line),"STREAM %s",radioStatusText().c_str());
  textAt(4,by(137),line,radioStatusColor());
  if(house.radioRetryIn>=0 && !strcasecmp(house.radioStatus,"retrying")) {
    snprintf(line,sizeof(line),"RETRY %d IN %.0fs",house.radioRetryAttempt,house.radioRetryIn);
    textAt(4,by(149),line,YELLOW);
  }
  if(nodes.info.have && nodes.snapReachable)
    snprintf(line,sizeof(line),"NODES %d PRESENT / %d AUDIBLE",nodes.present,nodes.audible);
  else snprintf(line,sizeof(line),"NODES: %s",nodes.info.have?"SNAP OFFLINE":"READING PRESENCE");
  textAt(4,by(158),line);
  if(house.volume<0) snprintf(line,sizeof(line),"VOL --  DEFAULT %s",house.defaultFolder);
  else snprintf(line,sizeof(line),"VOL %d%%  DEFAULT %s",house.volume,house.defaultFolder);
  textAt(4,by(172),line,CYAN);
}
void drawRadioInfo(uint32_t now) {
  header("RADIO INFO");
  if(emptyPage(house.info,now,7000)) return;
  String warning=cacheState(house.info,now,7000);
  if(warning.length()) textAt(4,by(31),warning,YELLOW);
  else textAt(4,by(31),String("STATUS  ")+radioStatusText(),radioStatusColor());
  textAt(4,by(48),"STATION",CYAN);
  radioStationBlock(62);
  char line[120];
  if(house.radioBitrate>0) snprintf(line,sizeof(line),"BITRATE: %d kbps",house.radioBitrate);
  else snprintf(line,sizeof(line),"BITRATE: NOT REPORTED");
  textAt(4,by(90),line);
  if(house.radioFormat[0]) {
    snprintf(line,sizeof(line),"AUDIO: %s",house.radioFormat);
    textAt(4,by(105),line);
  }
  if(house.radioHost[0]) {
    textAt(4,by(121),"STREAM HOST",CYAN);
    textAt(4,by(133),printable(house.radioHost));
  }
  if(!strcasecmp(house.radioStatus,"retrying")) {
    snprintf(line,sizeof(line),"RETRY %d IN %.0f SEC",house.radioRetryAttempt,
             house.radioRetryIn>=0?house.radioRetryIn:0);
    textAt(4,by(146),line,YELLOW);
  }
  if(house.radioError[0] && (!strcasecmp(house.radioStatus,"error") ||
                              !strcasecmp(house.radioStatus,"retrying")))
    textAt(4,by(160),String("ERROR: ")+printable(house.radioError),RED);
  textAt(4,by(178),"LIVE RADIO - NO TRACK QUEUE",CYAN);
}

void drawNowPlaying(uint32_t now) {
  header("NOW PLAYING");
  if (emptyPage(house.info,now,7000)) return;
  if(house.isRadio) { drawRadioNowPlaying(now); return; }
  String state=house.transport; state.toUpperCase();
  String warning=cacheState(house.info,now,7000);
  textAt(4,by(31),warning.length() ? warning : state,warning.length() ? YELLOW : GREEN);
  titleBlock(house.title);
  textAt(4,by(75),printable(house.artist),CYAN);
  textAt(4,by(88),printable(house.album),YELLOW);
  float elapsed=displayedElapsed;
  const int x=4,y=by(107),w=view.width()-8,h=11;
  view.drawRect(x,y,w,h,WHITE);
  float progress=house.duration>0 ? crt::clamp(elapsed/house.duration,0,1) : 0;
  int fill=int(progress*(w-4));
  if (fill>0) view.fillRect(x+2,y+2,fill,h-4,GREEN);
  textAt(4,by(124),durationText(elapsed)+" / "+durationText(house.duration));
  char line[96],volume[12];
  if (house.volume<0) snprintf(volume,sizeof(volume),"--");
  else snprintf(volume,sizeof(volume),"%d%%",house.volume);
  snprintf(line,sizeof(line),"TRACK %d/%d  VOL %s",house.songPosition>=0 ? house.songPosition+1:0,house.queueLength,volume);
  textAt(4,by(144),line);
  if(nodes.info.have && nodes.snapReachable)
    snprintf(line,sizeof(line),"NODES %d PRESENT / %d AUDIBLE",nodes.present,nodes.audible);
  else
    snprintf(line,sizeof(line),"NODES: %s",nodes.info.have ? "SNAP OFFLINE":"READING PRESENCE");
  textAt(4,by(158),line);
  snprintf(line,sizeof(line),"%s %s  DEFAULT %s",house.random ? "RND":"---",house.repeat ? "REP":"---",house.defaultFolder);
  textAt(4,by(172),line,CYAN);
}

String queueTitle(const QueueItem& item) {
  if(item.title[0]) return printable(item.title);
  return String("(untitled)");
}

void drawQueue(uint32_t now) {
  if(house.info.have && house.isRadio) { drawRadioInfo(now); return; }
  header("TRACK QUEUE");
  if(emptyPage(trackQueueData.info,now,12000)) return;

  char line[96];
  snprintf(line,sizeof(line),"TRACK %d/%d",house.songPosition>=0?house.songPosition+1:0,
           trackQueueData.queueLength);
  textAt(4,by(31),line,CYAN);
  textAt(105,by(31),trackQueueData.random?"RANDOM ON":"QUEUE ORDER",trackQueueData.random?YELLOW:GREEN);

  if(trackQueueData.count<=0) {
    centerText(by(103),trackQueueData.queueLength?"QUEUE DETAILS UNAVAILABLE":"QUEUE EMPTY",YELLOW);
    return;
  }

  int y0=by(49);
  const int step=27;
  for(int i=0;i<trackQueueData.count && i<QUEUE_ITEMS;i++) {
    const QueueItem& q=trackQueueData.item[i];
    int y=y0+i*step;
    bool current=(q.pos==house.songPosition) || (i==0 && trackQueueData.startPosition==house.songPosition);
    uint8_t color=current?GREEN:WHITE;
    view.fillRect(4,y+2,5,5,current?GREEN:CYAN);

    char pos[16];
    snprintf(pos,sizeof(pos),"%c%4d",current?'>':' ',q.pos>=0?q.pos+1:trackQueueData.startPosition+i+1);
    textAt(12,y,pos,current?GREEN:CYAN);
    textAt(48,y,clipped(queueTitle(q),27),color);

    String artist=printable(q.artist);
    if(!artist.length()) artist="--";
    textAt(48,y+11,clipped(artist,27),current?CYAN:YELLOW);
  }

  if(trackQueueData.random)
    textAt(4,by(179),"RANDOM ON: NEXT TRACK NOT PREDICTABLE",YELLOW);
  else
    textAt(4,by(179),"CURRENT + FOLLOWING QUEUE ENTRIES",CYAN);
}

void drawNodes(uint32_t now) {
  header("HOUSE NODES");
  if(emptyPage(nodes.info,now,30000)) return;
  char line[96];
  if(nodes.snapReachable)
    snprintf(line,sizeof(line),"PRESENT %d   AUDIBLE %d",nodes.present,nodes.audible);
  else
    snprintf(line,sizeof(line),"SNAP OFFLINE - LAST COUNTS %d / %d",nodes.present,nodes.audible);
  textAt(4,by(31),line,nodes.snapReachable?CYAN:YELLOW);

  // Only live devices are useful here. Remembered/offline IDs are intentionally
  // omitted; the server retains them, but this screen is an operational view.
  const int perSet=4;
  int live=nodes.list.liveRows;
  int sets=(live+perSet-1)/perSet;
  if(sets<1) sets=1;
  int set=(crt::age(now,pageEntered)/6000)%sets;
  int first=set*perSet, stop=first+perSet; if(stop>live) stop=live;
  snprintf(line,sizeof(line),"LIVE DEVICES %d/%d",set+1,sets);
  textAt(4,by(44),line,WHITE);
  if(!nodes.list.complete) textAt(view.width()-4-7*6,by(44),"PARTIAL",YELLOW);

  if(!live) centerText(by(103),"NO CURRENTLY PRESENT DEVICES",YELLOW);
  for(int i=first;i<stop;i++) {
    const crt::NodeView& d=nodes.list.row[i]; int y=by(60+(i-first)*29);
    uint8_t color=d.audible?GREEN:YELLOW;
    view.fillRect(4,y+2,5,5,color);
    const int tagX=view.width()-7*6-4;
    textAt(12,y,clipped(printable(d.name),(tagX-16)/6));
    textAt(tagX,y,String("#")+d.tag,CYAN);
    const char* status=d.audible?"AUDIBLE":(d.muted?"MUTED":"CONNECTED");
    char volume[12],latency[16];
    if(d.volume<0) snprintf(volume,sizeof(volume),"--"); else snprintf(volume,sizeof(volume),"%d%%",d.volume);
    if(!d.hasLatency) snprintf(latency,sizeof(latency),"--"); else snprintf(latency,sizeof(latency),"%+dms",d.latency);
    snprintf(line,sizeof(line),"%s  VOL %s  DELAY %s",status,volume,latency);
    textAt(12,y+11,line,color);
  }

  if(live>perSet) textAt(4,by(179),"LIVE LIST ROTATES EVERY 6 SEC",CYAN);
  else textAt(4,by(179),"ONLY CURRENTLY PRESENT DEVICES SHOWN",CYAN);
}

void healthLine(int y,const char* label,const String& value,uint8_t color=WHITE) {
  textAt(4,by(y),label,CYAN); textAt(94,by(y),value,color);
}
void drawHealth(uint32_t now) {
  header("HOUSE HEALTH");
  if(emptyPage(health.info,now,20000)) return;
  String warning=cacheState(health.info,now,20000);
  textAt(4,by(31),warning.length()?warning:(String("SERVER ")+health.version+"  "+health.status),warning.length()?YELLOW:CYAN);
  healthLine(47,"MPD",health.mpd?"OK":"UNAVAILABLE",health.mpd?GREEN:RED);
  healthLine(61,"SNAPSERVER",health.snap?"OK":"UNAVAILABLE",health.snap?GREEN:RED);
  healthLine(75,"STARTUP",health.ready?"READY":"WAITING",health.ready?GREEN:YELLOW);
  bool transportFresh=house.info.have && house.info.ok && crt::age(now,house.info.sampled)<7000;
  String state=transportFresh?String(house.transport):String("UNKNOWN"); state.toUpperCase();
  healthLine(89,"TRANSPORT",state,transportFresh?WHITE:YELLOW);
  String sessionWarning=cacheState(health.sessionInfo,now,20000);
  if(sessionWarning.length()) {
    textAt(4,by(105),"SESSION:",CYAN); textAt(4,by(119),sessionWarning,YELLOW);
  } else {
    healthLine(105,"AUTO PAUSE",health.autoPaused?"YES":"NO",health.autoPaused?YELLOW:GREEN);
    healthLine(119,"FINAL STOP",health.pendingStop?"ARMED":"NOT ARMED",health.pendingStop?YELLOW:WHITE);
    String reason=health.pauseReason;
    if(reason=="no_audible_output") reason="NO AUDIBLE OUTPUT";
    if(!reason.length()) reason=health.policyEnabled?"POLICY ACTIVE":"POLICY DISABLED";
    textAt(4,by(134),reason,CYAN);
    String action=health.lastAction; action.replace('_',' '); action.toUpperCase();
    textAt(4,by(148),action,WHITE);
  }
  if(nodes.info.have && nodes.info.ok && nodes.snapReachable && crt::age(now,nodes.info.sampled)<9000) {
    char line[64]; snprintf(line,sizeof(line),"CONTROLLERS %d / RADIOS %d",nodes.controllers,nodes.passive);
    textAt(4,by(170),line,CYAN);
  }
}
void drawSystems(uint32_t now) {
  header("HOUSE SYSTEMS");
  if(WiFi.status()!=WL_CONNECTED) {
    centerText(by(80),wifiProblem(),YELLOW);
    centerText(by(106),"LOCAL PAGES STILL WORK",CYAN);
    drawWifiDetails(); return;
  }
  textAt(4,by(31),"TCP REACHABILITY, NOT FULL HEALTH",CYAN);
  for(int i=0;i<PROBE_COUNT;i++) {
    int column=i<5?0:1, row=i<5?i:i-5;
    int x=4+column*(view.width()/2), y=by(49+row*26);
    const ProbeData& p=systems.probe[i];
    bool known=p.checked && crt::age(now,p.sampled)<45000;
    const char* status=p.checking?"CHECKING":(!known?"PENDING":(p.ok?"OK":"DOWN"));
    uint8_t color=(p.checking||!known)?YELLOW:(p.ok?GREEN:RED);
    view.fillRect(x,y+2,6,6,color);
    textAt(x+10,y,targets[i].label,WHITE);
    textAt(x+10,y+10,status,color);
  }
  char line[80];
  snprintf(line,sizeof(line),"%s   %ddBm",WiFi.localIP().toString().c_str(),int(WiFi.RSSI()));
  textAt(4,by(178),line,CYAN);
}
void drawClock(uint32_t now) {
  header("HOUSE CLOCK");
  time_t epoch=time(nullptr);
  if (epoch<1600000000) { centerText(by(96),"WAITING FOR NTP",YELLOW); return; }
  struct tm local; localtime_r(&epoch,&local);
  char hm[16],seconds[16],zone[16],date[48],line[48];
  snprintf(hm,sizeof(hm),"%d:%02d",local.tm_hour%12 ? local.tm_hour%12:12,local.tm_min);
  snprintf(seconds,sizeof(seconds),":%02d",local.tm_sec);
  strftime(zone,sizeof(zone),"%Z",&local);
  snprintf(line,sizeof(line),"%s  %s",local.tm_hour<12 ? "AM":"PM",zone);
  strftime(date,sizeof(date),"%a  %b %d %Y",&local);
  centerText(by(53),hm,WHITE,4);
  centerText(by(100),String(seconds)+"  "+line,CYAN,2);
  centerText(by(140),date,YELLOW);
  if (house.info.have && !strcmp(house.transport,"play")) {
    bool fresh=house.info.ok && WiFi.status()==WL_CONNECTED && crt::age(now,house.info.sampled)<7000;
    centerText(by(161),fresh ? "NOW PLAYING":"LAST TRACK (STALE)",fresh ? GREEN:YELLOW);
    textAt(4,by(174),printable(house.title));
  }
}

void diamondOutline(int cx,int cy,int rx,int ry,uint8_t color) {
  if (rx<1 || ry<1) return;
  view.drawLine(cx,cy-ry,cx+rx,cy,color); view.drawLine(cx+rx,cy,cx,cy+ry,color);
  view.drawLine(cx,cy+ry,cx-rx,cy,color); view.drawLine(cx-rx,cy,cx,cy-ry,color);
}
void diamondSolid(int cx,int cy,int rx,int ry,uint8_t color) {
  if (rx<1 || ry<1) return;
  view.fillTriangle(cx,cy-ry,cx-rx,cy,cx+rx,cy,color);
  view.fillTriangle(cx,cy+ry,cx-rx,cy,cx+rx,cy,color);
}
void shape(int cx,int cy,int rx,int ry,uint8_t c,int mode) {
  if (mode==0) diamondSolid(cx,cy,rx,ry,c);
  else if (mode==1) {
    diamondSolid(cx,cy,rx,ry,c); diamondSolid(cx,cy,rx*2/3,ry*2/3,BLACK);
  } else {
    diamondOutline(cx,cy,rx,ry,c);
    if (rx>11 && ry>11) diamondOutline(cx,cy,rx-5,ry-5,c);
    if (rx>19 && ry>19) diamondOutline(cx,cy,rx-10,ry-10,c);
  }
}
void drawAudioBars() {
  const int y=view.height()-25;
  for(int i=0;i<7;i++) {
    int x=4+i*11, h=int(crt::clamp(audio.bands[i]/1800.0f,0,1)*9);
    view.drawFastHLine(x,y+9,7,CYAN);
    if(h>0) view.fillRect(x,y+9-h,7,h,WHITE);
  }
  textAt(87,y+2,audio.signal?"SIGNAL":"BELOW GATE",audio.signal?GREEN:YELLOW);
}
void drawMusic() {
  view.clear();
  if (!showMusic) {
    centerText(4,"VIDEO MUSIC",CYAN);
    diamondOutline(view.width()/2,(view.height()-14)/2,12,12,WHITE);
    centerText(by(155),"WAITING FOR MUSIC",YELLOW);
    char line[48]; snprintf(line,sizeof(line),"ADC %u  EXCESS %u",audio.adcMax,(unsigned)audio.excessPeak);
    textAt(4,by(166),line,CYAN); drawAudioBars(); return;
  }
  const int cx=view.width()/2, cy=(view.height()-16)/2;
  float sx=(view.width()-12)/232.0f, sy=(view.height()-26)/180.0f;
  int rx=int((30+motion.bass*14)*sx),ry=int((34+motion.mid*15)*sy);
  int sep=int((54+motion.centroid*18)*sx),shift=int(motion.tilt*22*sy);
  int colorIndex=int((motion.centroid+1)*3+0.5f);
  colorIndex=int(crt::clamp(colorIndex,0,6));
  uint8_t c1=palette[colorIndex],c2=palette[(colorIndex+3)%7];
  int mainRX=visualMode==0 ? rx*4/5:rx,mainRY=visualMode==0 ? ry*4/5:ry;
  shape(cx-sep,cy+shift,mainRX,mainRY,c1,visualMode);
  shape(cx+sep,cy-shift,mainRX,mainRY,c2,visualMode);
  int coreX=int((9+motion.flux*50)*sx),coreY=int((7+motion.flux*38)*sy);
  diamondOutline(cx,cy,coreX,coreY,WHITE);
  if (motion.flux>0.22f) diamondOutline(cx,cy,coreX-int(5*sx),coreY-int(4*sy),c1);
  if (motion.flux>0.48f) diamondOutline(cx,cy,coreX-int(10*sx),coreY-int(8*sy),c2);
  if (motion.flux>0.72f) diamondOutline(cx,cy,coreX-int(15*sx),coreY-int(11*sy),palette[(colorIndex+1)%7]);
  int dist=int((52+motion.spread*25)*sy),size=int(3+motion.flux*5);
  diamondSolid(cx,cy-dist,int(size*sx),int(size*sy),c2);
  diamondSolid(cx,cy+dist,int(size*sx),int(size*sy),c1);
  // Near-full-scale warning only; it is not proof of where analog clipping occurs.
  // Absolute acquisition feedback, BEFORE relative scaling; no serial logger.
  char input[40];
  snprintf(input,sizeof(input),"MIC %s  ADC %u",audio.nearLimit?"HIGH":(audio.signal?"SIGNAL":"QUIET"),audio.adcMax);
  textAt(4,2,input,audio.nearLimit?RED:CYAN);
  drawAudioBars();
}
void ellipseOutline(int cx,int cy,int rx,int ry,uint8_t color) {
  int oldX=cx+rx,oldY=cy;
  for(int n=1;n<=96;n++) {
    float a=n*6.28318530718f/96;
    int x=cx+int(lroundf(cosf(a)*rx)),y=cy+int(lroundf(sinf(a)*ry));
    view.drawLine(oldX,oldY,x,y,color); oldX=x; oldY=y;
  }
}
void drawTest() {
  view.clear(); const int w=view.width(),h=view.height();
  view.drawRect(0,0,w,h,WHITE); // ALL FOUR exact safe-area boundaries.
  textAt(3,2,"TL",WHITE); textAt(w-15,2,"TR",WHITE);
  textAt(3,h-26,"BL",WHITE); textAt(w-15,h-26,"BR",WHITE);
  for(int i=0;i<7;i++) {
    int x=3+i*(w-6)/7,end=3+(i+1)*(w-6)/7;
    view.fillRect(x,14,end-x,28,palette[i]);
  }
  int gridTop=48,gridBottom=h-32;
  for(int x=12;x<w-3;x+=24) view.drawFastVLine(x,gridTop,gridBottom-gridTop,CYAN);
  for(int y=gridTop+4;y<gridBottom;y+=20) view.drawFastHLine(3,y,w-6,CYAN);
  int cx=w/2,cy=(gridTop+gridBottom)/2,ry=(gridBottom-gridTop)/2-5;
  int rx=int(ry/cfg.pixelAspect);
  ellipseOutline(cx,cy,rx,ry,WHITE); ellipseOutline(cx,cy,rx/2,ry/2,YELLOW);
  view.drawFastHLine(cx-rx-5,cy,rx*2+11,WHITE);
  view.drawFastVLine(cx,cy-ry-3,ry*2+7,WHITE);
  centerText(h-26,"4:3 FIT REFERENCE",WHITE);
  // Footer stays inside the border. NO track overlay on this page.
}
void drawOverlay(uint32_t now) {
  if (!overlayActive || crt::age(now,overlayStarted)>=2600) { overlayActive=false; return; }
  if (nav.page==NOW_PLAYING || nav.page==QUEUE || nav.page==TEST ||
      !house.info.have || !house.title[0]) return;
  int y=view.height()-48;
  view.fillRect(1,y,view.width()-2,33,BLACK); view.drawRect(1,y,view.width()-2,33,WHITE);
  textAt(5,y+5,printable(house.title)); textAt(5,y+18,printable(house.artist),CYAN);
}
void pollButton(uint32_t now) {
  uint8_t previous=nav.page;
  nav.update(digitalRead(cfg.buttonPin)==LOW,now,PAGE_COUNT);
  if (previous!=nav.page) {
    pageEntered=now;
    requestedPage.store(nav.page,std::memory_order_relaxed);
  }
}
void receiveData(uint32_t now) {
  if (!netStarted) return;
  HouseData incoming;
  if (xQueueReceive(stateQueue,&incoming,0)==pdTRUE) {
    bool changed=false;
    if(incoming.info.ok && house.info.have) {
      if(incoming.isRadio && house.isRadio) {
        // The MPD queue ID stays constant while radio track metadata changes.
        changed=(incoming.title[0] && (strcmp(incoming.title,house.title) ||
                 strcmp(incoming.artist,house.artist))) ||
                strcmp(incoming.radioStation,house.radioStation);
      } else {
        changed=incoming.songId>=0 &&
          (house.songId!=incoming.songId || strcmp(house.file,incoming.file));
      }
    }
    house=incoming;
    if (changed) { overlayActive=true; overlayStarted=now; }
  }
  xQueueReceive(trackQueue,&trackQueueData,0);
  xQueueReceive(nodesQueue,&nodes,0); xQueueReceive(healthQueue,&health,0);
  xQueueReceive(probeQueue,&systems,0);
}
void bootScreen(const char* line1,const char* line2="") {
  videoOut.waitForFrame(); view.clear();
  centerText(45,"CRT HOUSE CONSOLE",CYAN,2);
  centerText(91,line1,WHITE); centerText(112,line2,YELLOW);
  videoOut.waitForFrame();
}
} // namespace

void consoleBegin(const ConsoleConfig& config) {
  cfg=config;
  // Limits preserve readable layout; four independent edges, not stretching.
  cfg.left=uint8_t(crt::clamp(cfg.left,0,28)); cfg.right=uint8_t(crt::clamp(cfg.right,0,28));
  cfg.top=uint8_t(crt::clamp(cfg.top,0,28)); cfg.bottom=uint8_t(crt::clamp(cfg.bottom,0,28));
  if(!isfinite(cfg.pixelAspect)) cfg.pixelAspect=1.25f;
  cfg.pixelAspect=crt::clamp(cfg.pixelAspect,0.75f,1.75f);
  cfg.garageTimeoutMs=uint16_t(crt::clamp(cfg.garageTimeoutMs,500,3000));
  Serial.begin(115200);
  setenv("TZ",cfg.timezone,1); tzset(); // Also valid before Wi-Fi/NTP reconnects.
  pinMode(cfg.buttonPin,INPUT_PULLUP);
  pinMode(MSGEQ7_RESET,OUTPUT); pinMode(MSGEQ7_STROBE,OUTPUT); pinMode(MSGEQ7_OUT,INPUT);
  digitalWrite(MSGEQ7_RESET,LOW); digitalWrite(MSGEQ7_STROBE,HIGH);
  analogReadResolution(12); analogSetPinAttenuation(MSGEQ7_OUT,ADC_11db);
  videoOut.begin(); view.setBounds(cfg.left,cfg.right,cfg.top,cfg.bottom);
  bootScreen("v0.3.4 STARTING","RADIO + LIBRARY DISPLAY");
  nav.begin(digitalRead(cfg.buttonPin)==LOW,millis());
  audio.begin(millis()); styleChanged=pageEntered=millis();
  targets[0]={"HOUSE",cfg.pi,cfg.housePort}; targets[1]={"MPD",cfg.pi,6600};
  targets[2]={"SNAP",cfg.pi,1705}; targets[3]={"DNS",cfg.pi,53};
  targets[4]={"FRIGATE",cfg.lenovo,8971}; targets[5]={"CAMVIEW",cfg.lenovo,9443};
  targets[6]={"ANNKE",cfg.annke,554}; targets[7]={"GARAGE",cfg.garage,554};
  targets[8]={"POLE BARN",cfg.pole,554};
  stateQueue=xQueueCreate(1,sizeof(HouseData)); trackQueue=xQueueCreate(1,sizeof(QueueData));
  nodesQueue=xQueueCreate(1,sizeof(NodesData)); healthQueue=xQueueCreate(1,sizeof(HealthData));
  probeQueue=xQueueCreate(1,sizeof(SystemsData));
  heap_caps_register_failed_alloc_callback(failedAllocHook);
  WiFi.persistent(false);
  WiFi.onEvent(onWifiEvent);
  startWifiAttempt();
  if (stateQueue && trackQueue && nodesQueue && healthQueue && probeQueue) {
    netStarted=xTaskCreatePinnedToCore(networkTask,"crt-net",8192,nullptr,1,&netTaskHandle,0)==pdPASS;
  }
  if (!netStarted) Serial.println("CRT console: network task allocation failed; local pages remain available.");
  Serial.println("CRT House Console v0.3.4: radio aware. GPIO13 button to GND. Hold 6s for AUTO.");
}
void consoleLoop() {
  pollButton(millis());
  videoOut.waitForFrame();
  uint32_t now=millis();
  pollButton(now); receiveData(now); updateAudio(now);
  bool fresh=house.info.have && house.info.ok && WiFi.status()==WL_CONNECTED &&
             crt::age(now,house.info.sampled)<7000;
  if(fresh) displayedElapsed=crt::playbackPosition(house.elapsed,house.duration,
               !strcmp(house.transport,"play"),true,house.info.sampled,now);
  // On a network failure keep the last displayed estimate, not the older
  // confirmed timestamp. Reconcile when a new valid server sample arrives.
  switch (nav.page) {
    case NOW_PLAYING: drawNowPlaying(now); break;
    case QUEUE: drawQueue(now); break;
    case NODES: drawNodes(now); break;
    case HEALTH: drawHealth(now); break;
    case SYSTEMS: drawSystems(now); break;
    case CLOCK: drawClock(now); break;
    case MUSIC: drawMusic(); break;
    case TEST: drawTest(); break;
  }
  drawOverlay(now); footer(now);
}
