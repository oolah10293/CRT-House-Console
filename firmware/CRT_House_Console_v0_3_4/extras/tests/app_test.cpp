#include <cassert>
#include <fstream>
#include <random>
#include <sstream>
#include "../../ConsoleApp.cpp"
int testChecks=0,frames=0;
#define VERIFY(x) do{++testChecks;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
void checkFrame() {
  frames++;
  for(int y=0;y<240;y++)for(int x=0;x<256;x++)if(videoOut.pixels[y*256+x]){
    VERIFY(x>=cfg.left&&x<256-cfg.right&&y>=cfg.top&&y<240-cfg.bottom);
  }
  for(auto& t:drawnText) {
    VERIFY(t.x>=0&&t.y>=0);
    VERIFY(t.x+int(t.text.size())*6*t.size<=view.width());
    VERIFY(t.y+8*t.size<=view.height());
  }
}
void save(const char* name) {
  const char* dest=getenv("CRT_TEST_OUTPUT");std::string root=dest?dest:"./";std::ofstream p(root+name+".ppm",std::ios::binary);
  p<<"P6\n256 240\n255\n";
  for(uint8_t c:videoOut.pixels){char rgb[]={char(((c>>5)&7)*255/7),char(((c>>2)&7)*255/7),char((c&3)*255/3)};p.write(rgb,3);}
  std::ofstream t(root+name+".text");for(auto& e:drawnText)t<<e.x+cfg.left<<'\t'<<e.y+cfg.top<<'\t'<<e.size<<'\t'<<e.color<<'\t'<<e.text<<'\n';
}
void drawPage(int page,uint32_t now) {
  drawnText.clear();nav.page=page;
  switch(page){case 0:drawNowPlaying(now);break;case 1:drawQueue(now);break;case 2:drawNodes(now);break;case 3:drawHealth(now);break;
  case 4:drawSystems(now);break;case 5:drawClock(now);break;case 6:drawMusic();break;case 7:drawTest();break;}
  drawOverlay(now);footer(now);checkFrame();
}
int main(){
 cfg={"test","not-a-password","192.0.2.10","192.0.2.20","192.0.2.30","192.0.2.31","192.0.2.32",
 "EST5EDT,M3.2.0,M11.1.0",8787,16,24,16,16,13,1500,1.25f};
 view.setBounds(16,24,16,16);netStarted=true;nav.begin(false,fakeNow);pageEntered=fakeNow;
 // Exercise the real application readers with an interface-compatible JSON
 // fixture model. This tests field selection/mapping, NOT ArduinoJson's parser.
 DynamicJsonDocument doc(24576);StaticJsonDocument<2048> filter;
 fakeJson=JV();fakeJson["service"]="house-audio-server";
 auto m=fakeJson["mpd"];m["transport"]="play";m["elapsedSeconds"]=51.0f;m["durationSeconds"]=257.0f;
 m["volume"]=100;m["queueLength"]=1652;m["songPosition"]=599;m["songId"]=100;
 m["repeat"]=true;m["random"]=true;m["song"]["title"]="Two Weeks";m["song"]["artist"]="All That Remains";
 m["song"]["album"]="Overcome";m["song"]["file"]="MP3s/Two Weeks.mp3";
 fakeJson["renderers"]["presentCount"]=1; // Old incorrect count must not leak into the UI.
 fakeJson["sessionPolicy"]["defaultFolder"]="MP3s";
 readHouse(doc,filter,house);VERIFY(house.info.ok&&house.songId==100);displayedElapsed=51;

 success(trackQueueData.info);trackQueueData.queueLength=1652;trackQueueData.startPosition=599;
 trackQueueData.random=true;trackQueueData.count=5;
 const char* qt[]={"Two Weeks","Feed the Dogs","Light Work","Ugly","Born of a Broken Man"};
 const char* qa[]={"All That Remains","Prof","Prof","Smashing Pumpkins","Rage Against the Machine"};
 for(int i=0;i<5;i++){trackQueueData.item[i]=QueueItem();trackQueueData.item[i].pos=599+i;
   snprintf(trackQueueData.item[i].title,sizeof(trackQueueData.item[i].title),"%s",qt[i]);
   snprintf(trackQueueData.item[i].artist,sizeof(trackQueueData.item[i].artist),"%s",qa[i]);}

 static crt::RendererRecords rs;static crt::PresenceRecords ps;
 fakeJson=JV();fakeJson["service"]="house-audio-server";
 auto snap=fakeJson["snapserver"];snap["reachable"]=true;
 const char* ids[]={"radio","phone","phone-old","spare","new-id"};
 const char* names[]={"Kitchen radio","SM-S921U","SM-S921U","Bose Sub","SM-S921U"};
 for(int i=0;i<5;i++) {auto c=snap["clients"][i];c["id"]=ids[i];c["name"]=names[i];c["present"]=(i==0);
 c["audible"]=(i==0);c["muted"]=false;c["volumePercent"]=100;c["latencyMs"]=0;}
 VERIFY(readRendererRecords(doc,filter,rs,nodes.namesInfo));VERIFY(rs.count==5);
 fakeJson=JV();fakeJson["service"]="house-audio-server";auto p=fakeJson["presence"];
 p["presentCount"]=2;p["audibleCount"]=1;p["controllerCount"]=1;p["passiveCount"]=1;p["snapserverReachable"]=true;
 auto c=p["controllers"][0];c["controllerId"]="myphone";c["rendererId"]="phone";c["present"]=true;c["outputMuted"]=true;c["outputReady"]=false;
 for(int i=0;i<5;i++){auto b=p["renderers"][i];b["rendererId"]=ids[i];b["present"]=(i==0);b["audible"]=(i==0);
 b["passive"]=(i!=1&&i!=2);if(i==1||i==2)b["controllerId"]="myphone";}
 VERIFY(readPresence(doc,filter,ps,nodes.info));publishNodes(nodes,rs,ps);
 VERIFY(nodes.present==2&&nodes.audible==1&&nodes.list.liveRows==2&&nodes.list.historyRows==0);
 bool muted=false;for(int i=0;i<nodes.list.count;i++)if(!strcmp(nodes.list.row[i].name,"SM-S921U")&&nodes.list.row[i].present)muted=nodes.list.row[i].muted;
 VERIFY(muted);VERIFY(nodes.list.count==2); // two owned receiver IDs merged into one phone

 fakeJson=JV();fakeJson["service"]="house-audio-server";fakeJson["version"]="0.9.2";fakeJson["status"]="ok";
 fakeJson["mpd"]["reachable"]=true;fakeJson["snapserver"]["reachable"]=true;fakeJson["startup"]["ready"]=true;
 readHealth(doc,filter,health);VERIFY(health.info.ok&&health.mpd&&health.snap&&health.ready);
 VERIFY(lastURL.find("/health")!=std::string::npos);
 fakeJson=JV();fakeJson["service"]="house-audio-server";auto s=fakeJson["sessionPolicy"];
 s["enabled"]=true;s["autoPaused"]=false;s["pendingFinalStop"]=false;s["lastAction"]="session_playing";s["defaultFolder"]="MP3s";
 readSession(doc,filter,health);VERIFY(health.sessionInfo.ok);VERIFY(lastURL.find("/session")!=std::string::npos);
 // Error preserves last successful health data and exposes the HTTP code.
 fakeHttpCode=503;readHealth(doc,filter,health);VERIFY(!health.info.ok&&health.info.have&&health.info.http==503&&health.mpd);
 fakeHttpCode=200;success(health.info);
 const char* probeNames[]={"HOUSE","MPD","SNAP","DNS","FRIGATE","CAMVIEW","ANNKE","GARAGE","POLE BARN"};
 for(int i=0;i<9;i++){targets[i]={probeNames[i],cfg.pi,554};systems.probe[i].checked=true;systems.probe[i].ok=i!=7;systems.probe[i].sampled=fakeNow;}
 probeResult=false;systems.probe[7].checked=false;probeOne(systems,7);VERIFY(systems.probe[7].checking&&!systems.probe[7].checked);VERIFY(lastProbeTimeout==1500);
 fakeNow+=600;probeOne(systems,7);VERIFY(!systems.probe[7].checking&&systems.probe[7].checked&&!systems.probe[7].ok);
 fakeNow+=16000;probeResult=true;probeOne(systems,7);VERIFY(systems.probe[7].ok&&systems.probe[7].misses==0);
 // Refresh labels before preview fixtures.
 success(house.info);success(nodes.info);success(nodes.namesInfo);success(health.info);success(health.sessionInfo);
 for(int i=0;i<9;i++)systems.probe[i].sampled=fakeNow;
 audio.signal=true;showMusic=true;motion.bass=.5;motion.mid=-.2;motion.flux=.6;audio.adcMax=900;
 pageEntered=fakeNow;overlayActive=false;setenv("TZ",cfg.timezone,1);tzset();
 for(int i=0;i<8;i++){drawPage(i,fakeNow);char name[24];snprintf(name,sizeof(name),"page_%d",i);save(name);}
 // Nodes page no longer rotates into offline ID history.
 drawPage(NODES,fakeNow+6100);save("nodes_second_set");
 // Overlay must not paint over the fit test.
 drawnText.clear();drawTest();size_t before=drawnText.size();nav.page=TEST;overlayActive=true;overlayStarted=fakeNow;
 drawOverlay(fakeNow);VERIFY(drawnText.size()==before);overlayActive=false;
 // Full safe rectangle corners remain visible even after footer.
 drawPage(TEST,fakeNow);
 VERIFY(videoOut.pixels[16*256+16]==WHITE);VERIFY(videoOut.pixels[16*256+231]==WHITE);
 VERIFY(videoOut.pixels[223*256+16]==WHITE);VERIFY(videoOut.pixels[223*256+231]==WHITE);
 // Every combination of extrema in all three styles remains inside the viewport.
 for(int mode=0;mode<3;mode++)for(int mask=0;mask<64;mask++) {
 visualMode=mode;motion.bass=(mask&1)?1:-1;motion.mid=(mask&2)?1:-1;motion.centroid=(mask&4)?1:-1;
 motion.tilt=(mask&8)?1:-1;motion.spread=(mask&16)?1:-1;motion.flux=(mask&32)?1:0;drawPage(MUSIC,fakeNow);}
 for(int top: {0,16,28})for(int side: {0,20,28}){
 cfg.top=cfg.bottom=top;cfg.left=cfg.right=side;view.setBounds(side,side,top,top);
 for(int i=0;i<8;i++)drawPage(i,fakeNow);}
 // Wi-Fi startup must expose a failed driver init rather than trying begin.
 WiFi.state=WL_DISCONNECTED;WiFi.modeResult=false;WiFi.beginCalls=0;
 startWifiAttempt();VERIFY(!wifiModeOk.load());VERIFY(WiFi.beginCalls==0);
 VERIFY(!strcmp(wifiProblem(),"WIFI INITIALIZATION FAILED"));
 WiFi.modeResult=true;WiFi.beginResult=WL_CONNECT_FAILED;startWifiAttempt();
 VERIFY(!wifiBeginOk.load());VERIFY(!strcmp(wifiProblem(),"WIFI START REJECTED"));
 WiFi.beginResult=WL_IDLE_STATUS;int begins=WiFi.beginCalls;
 retryWifi();VERIFY(WiFi.beginCalls==begins+1&&wifiBeginOk.load());
 WiFi.reconnectResult=false;begins=WiFi.beginCalls;retryWifi();VERIFY(WiFi.beginCalls==begins+1);
 WiFiEventInfo_t ev;ev.wifi_sta_disconnected.reason=201;
 onWifiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,ev);
 VERIFY(wifiLastReason.load()==201);VERIFY(!strcmp(wifiProblem(),"WIFI: ACCESS POINT NOT FOUND"));
 onWifiEvent(ARDUINO_EVENT_WIFI_STA_CONNECTED,ev);
 VERIFY(!strcmp(wifiProblem(),"WIFI: WAITING FOR IP ADDRESS"));
 onWifiEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP,ev);VERIFY(wifiLastReason.load()==0);
 // Diagnostics fit the exact safe area, including memory-allocation failures.
 cfg.top=cfg.bottom=16;cfg.left=16;cfg.right=24;view.setBounds(16,24,16,16);
 failedAllocHook(16384,4,"malloc");
 for(int reason:{0,15,201,202,204,205}) {
   ev.wifi_sta_disconnected.reason=reason;
   onWifiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,ev);
   HouseData keep=house;house.info.have=false;
   drawPage(NOW_PLAYING,fakeNow);house=keep;drawPage(SYSTEMS,fakeNow);
 }
 WiFi.state=WL_CONNECTED;
 // Queue stores filename fallback directly as title; no duplicate path array.
 probeResult=true;mockMpdResponse="OK MPD 0.24.0\nfile: MP3s/No Tags.mp3\nPos: 599\nId: 100\nfile: Rap/source.mp3\nTitle: Actual Title\nArtist: Example\nPos: 600\nId: 101\nOK\n";
 QueueData queueTest;readTrackQueue(queueTest,house);
 VERIFY(queueTest.info.ok&&queueTest.count==2);
 VERIFY(!strcmp(queueTest.item[0].title,"No Tags"));
 VERIFY(!strcmp(queueTest.item[1].title,"Actual Title"));
 VERIFY(!strcmp(queueTest.item[1].artist,"Example"));
 VERIFY(mockMpdCommand=="playlistinfo 599:604\n");

 // Radio-specific presentation using the REAL /state JSON field names from
 // HOUSE v0.10.0. This is an interface-model test, not an HTTP target test.
 WiFi.state=WL_CONNECTED;fakeHttpCode=200;
 fakeJson=JV();fakeJson["service"]="house-audio-server";
 auto rM=fakeJson["mpd"]; rM["transport"]="play";rM["queueLength"]=1;
 rM["songId"]=444;rM["songPosition"]=0;rM["volume"]=90;
 rM["elapsedSeconds"]=436.0f;rM["durationSeconds"]=0.0f;
 rM["bitrate"]=192;rM["audio"]="44100:16:2";
 rM["song"]["file"]="https://stream.example.test/live";
 rM["song"]["title"]="Disturbed - text=\"I Will Not Break\" song_spot=\"N\" Media:Basal...";
 fakeJson["sessionPolicy"]["defaultFolder"]="Rap";
 auto source=fakeJson["source"];source["type"]="radio";source["live"]=true;
 source["status"]="playing";source["stationSaved"]=true;
 source["station"]["name"]="Rock Mix - Example Station";
 source["station"]["url"]="https://stream.example.test/live";
 readHouse(doc,filter,house);
 VERIFY(house.info.ok&&house.isRadio);
 VERIFY(!strcmp(house.artist,"Disturbed")&&!strcmp(house.title,"I Will Not Break"));
 VERIFY(!strcmp(house.radioStation,"Rock Mix - Example Station"));
 VERIFY(!strcmp(house.radioHost,"stream.example.test"));
 VERIFY(!strcmp(house.radioStatus,"playing")&&house.radioBitrate==192);
 VERIFY(!strcmp(house.radioFormat,"44100:16:2"));
 drawPage(NOW_PLAYING,fakeNow);save("radio_now_playing");
 auto containsText=[](const char* t) {
   for(const auto& e:drawnText) {
     if(e.text.find(t)!=std::string::npos) return true;
   }
   return false;
 };
 VERIFY(containsText("RADIO  LIVE")&&containsText("Disturbed"));
 VERIFY(containsText("I Will Not Break")&&containsText("Rock Mix"));
 VERIFY(!containsText("7:16")&&!containsText("TRACK 1/1"));
 drawPage(QUEUE,fakeNow);save("radio_info");
 VERIFY(containsText("RADIO INFO")&&containsText("BITRATE: 192 kbps"));
 VERIFY(containsText("NO TRACK QUEUE")&&!containsText("TRACK 1/1"));
 mockMpdCommand="UNTOUCHED";
 QueueData radioQueue;readTrackQueue(radioQueue,house);
 VERIFY(radioQueue.info.ok&&radioQueue.count==0&&mockMpdCommand=="UNTOUCHED");

 // A radio song changes without an MPD queue-id change. Trigger overlay on
 // metadata updates while not triggering on idle polls of identical content.
 stateQueue=xQueueCreate(1,sizeof(HouseData));
 HouseData nextRadio=house;snprintf(nextRadio.title,sizeof(nextRadio.title),"%s","Another Song");
 success(nextRadio.info);xQueueOverwrite(stateQueue,&nextRadio);
 overlayActive=false;receiveData(fakeNow);VERIFY(overlayActive&&house.songId==444);
 overlayActive=false;xQueueOverwrite(stateQueue,&nextRadio);receiveData(fakeNow);
 VERIFY(!overlayActive); // Same title and same artist should not re-trigger.

 // Logical radio pause can coexist with raw MPD transport=stop.
 rM["transport"]="stop";source["status"]="paused";
 readHouse(doc,filter,house);
 VERIFY(house.isRadio && !strcmp(house.radioStatus,"paused"));
 drawPage(NOW_PLAYING,fakeNow);VERIFY(containsText("RADIO  PAUSED"));

 // Compatible with older HOUSE servers that have no /state.source field.
 fakeJson=JV();fakeJson["service"]="house-audio-server";
 rM=fakeJson["mpd"];rM["transport"]="play";
 rM["queueLength"]=1;rM["songId"]=444;
 rM["song"]["file"]="http://legacy.example.test:8080/live";
 rM["song"]["title"]="Artist - Track";
 rM["song"]["stationName"]="Legacy Radio Name";
 readHouse(doc,filter,house);
 VERIFY(house.isRadio && !strcmp(house.radioStation,"Legacy Radio Name"));
 VERIFY(!strcmp(house.artist,"Artist")&&!strcmp(house.title,"Track"));

 // Source type from the new API is authoritative even if the URL is odd.
 fakeJson["source"]["type"]="library";
 rM["song"]["file"]="MP3s/Actual Song.mp3";
 rM["song"]["title"]="Actual Song";
 readHouse(doc,filter,house);
 VERIFY(!house.isRadio&&!strcmp(house.title,"Actual Song"));
 VERIFY(!house.radioStation[0]&&!house.radioStatus[0]);
 drawPage(NOW_PLAYING,fakeNow);
 VERIFY(!containsText("RADIO"));
 printf("Removed merge scratch: %zu bytes; queue snapshot: %zu -> %zu bytes.\n",
        sizeof(crt::DeviceList),size_t(2380),sizeof(QueueData));
 printf("PASS app readers, reachability retries, test-overlay exclusion, and %d layout frames (%d assertions).\n",frames,testChecks);
 printf("Snapshot sizes: HOUSE %zu, TRACKQ %zu, NODES %zu, HEALTH %zu; each queue has ONE slot.\n",sizeof(HouseData),sizeof(QueueData),sizeof(NodesData),sizeof(HealthData));
}
