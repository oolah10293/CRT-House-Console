#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <random>
#include <algorithm>
#include "ConsoleLogic.h"
#include "DeviceModel.h"
#include "MusicEngine.h"
int checks=0;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
void click(crt::Navigation& n,uint32_t t) { n.update(true,t,8);n.update(true,t+31,8);n.update(false,t+150,8);n.update(false,t+181,8); }
int main() {
  crt::Navigation n;n.begin(false,0);CHECK(!n.automatic&&n.page==0);
  click(n,100);CHECK(n.page==1&&!n.automatic);
  n.update(true,1000,8);n.update(true,1031,8);n.update(true,7030,8);CHECK(!n.automatic);
  n.update(true,7031,8);CHECK(n.automatic&&n.page==1);
  n.update(false,7100,8);n.update(false,7131,8);CHECK(n.page==1);
  n.update(false,13031,8);CHECK(n.page==2);
  n.update(true,14000,8);n.update(true,14031,8);CHECK(!n.automatic&&n.page==2);
  n.update(true,23000,8);CHECK(!n.automatic);
  n.update(false,23100,8);n.update(false,23131,8);CHECK(n.page==2);
  click(n,24000);CHECK(n.page==3);
  n.begin(true,0);n.update(true,10000,8);CHECK(!n.automatic&&n.page==0);
  n.update(false,11000,8);n.update(false,11031,8);CHECK(n.page==0);click(n,12000);CHECK(n.page==1);
  n.begin(false,0xffffff00U);click(n,0xffffff10U);CHECK(n.page==1);
  n.begin(false,0);n.update(true,100,8);n.update(false,110,8);n.update(false,150,8);CHECK(n.page==0);
  for(int i=0;i<8;i++){click(n,200+i*250);}CHECK(n.page==0);
  CHECK(crt::playbackPosition(50,200,true,true,1000,3500)==52.5f);
  CHECK(crt::playbackPosition(50,200,false,true,1000,3500)==50);
  CHECK(crt::playbackPosition(199,200,true,true,1000,3500)==200);
  CHECK(crt::playbackPosition(NAN,200,false,true,1000,3500)==0);

  static crt::RendererRecords r; static crt::PresenceRecords p; static crt::DeviceList d;
  r.count=3;r.sourceCount=3;r.reachable=true;
  crt::textCopy(r.row[0].id,"radio1");crt::textCopy(r.row[0].name,"Kitchen radio");
  crt::textCopy(r.row[1].id,"phone-old");crt::textCopy(r.row[1].name,"SM-S921U");
  crt::textCopy(r.row[2].id,"phone-new");crt::textCopy(r.row[2].name,"SM-S921U");
  p.bindings=3;p.controllers=1;p.presentCount=2;p.audibleCount=1;
  crt::textCopy(p.controller[0].id,"phone");crt::textCopy(p.controller[0].rendererId,"phone-new");
  p.controller[0].present=true;p.controller[0].muted=true;
  crt::textCopy(p.binding[0].rendererId,"radio1");p.binding[0].present=p.binding[0].audible=true;
  crt::textCopy(p.binding[1].rendererId,"phone-old");crt::textCopy(p.binding[1].owner,"phone");p.binding[1].passive=false;
  crt::textCopy(p.binding[2].rendererId,"phone-new");crt::textCopy(p.binding[2].owner,"phone");p.binding[2].passive=false;
  crt::buildDevices(r,p,d);CHECK(d.liveRows==2&&d.historyRows==0&&d.count==2);
  bool mutedPhone=false;
  for(int i=0;i<d.count;i++) if(d.row[i].controller) { mutedPhone=d.row[i].muted&&d.row[i].present&&!d.row[i].audible; }
  CHECK(mutedPhone);
  // Live controller without a receiver entry still counts and is displayed.
  p.controller[0].rendererId[0]=0;crt::buildDevices(r,p,d);CHECK(d.liveRows==2);
  // Matching display names DO NOT establish identity.
  r.count=4;crt::textCopy(r.row[3].id,"unrelated-old-registration");crt::textCopy(r.row[3].name,"SM-S921U");
  crt::buildDevices(r,p,d);CHECK(d.count==3&&d.historyRows==1&&d.liveRows==2);
  CHECK(strcmp(d.row[1].tag,d.row[2].tag)!=0);
  p.controller[0].present=false;crt::buildDevices(r,p,d);CHECK(d.liveRows==1);
  p.binding[2].audible=true;crt::buildDevices(r,p,d);CHECK(d.liveRows==2); // server-reported audible ownership
  p.complete=false;crt::buildDevices(r,p,d);CHECK(!d.complete);

  crt::MusicEngine a;uint16_t raw[7];uint32_t ms=0;a.begin(ms);
  std::mt19937 rng(42);
  for(int frame=0;frame<1000;frame++) {
    for(int i=0;i<7;i++)raw[i]=220+rng()%80;
    a.update(raw,ms+=17);CHECK(!a.signal&&!a.visible);
  }
  // Starting DURING music needs no quiet baseline capture.
  crt::MusicEngine atBoot;atBoot.begin(0);
  for(int f=1;f<=40;f++) { for(int i=0;i<7;i++)raw[i]=500+(i==1?400:0); atBoot.update(raw,f*17); }
  CHECK(atBoot.signal&&atBoot.visible);
  float maxPulse=0,minPulse=1;
  for(int f=0;f<240;f++) {
    bool beat=f%30<5;
    for(int i=0;i<7;i++)raw[i]=uint16_t(240+(beat?1000:350)*(i<2?1.8f:1.0f));
    atBoot.update(raw,680+(f+1)*17);
    if(f>60) {maxPulse=fmaxf(maxPulse,atBoot.motion.flux);minPulse=fminf(minPulse,atBoot.motion.flux);}
    CHECK(std::isfinite(atBoot.motion.flux));CHECK(atBoot.motion.flux>=0&&atBoot.motion.flux<=1);
    CHECK(fabs(atBoot.motion.bass)<=1&&fabs(atBoot.motion.centroid)<=1);
  }
  CHECK(maxPulse-minPulse>0.30f);
  uint32_t end=680+240*17;
  for(int f=1;f<=200;f++) { for(int i=0;i<7;i++)raw[i]=230+rng()%45; atBoot.update(raw,end+f*17); }
  CHECK(!atBoot.signal&&atBoot.visible); // Four-second hold is still in progress.
  for(int f=201;f<=280;f++) { for(int i=0;i<7;i++)raw[i]=230+rng()%45; atBoot.update(raw,end+f*17); }
  CHECK(!atBoot.signal&&!atBoot.visible);
  // Steady above-noise tones stay visible but don't autonomously generate pulses.
  crt::MusicEngine steady;steady.begin(0);
  for(int f=1;f<1000;f++){for(int i=0;i<7;i++)raw[i]=700;steady.update(raw,f*17);}
  CHECK(steady.signal&&steady.visible&&steady.motion.flux<0.001f);
  // Gain invariance is tested on noise-subtracted signals, not clipping or noise.
  crt::MusicEngine low,high; low.begin(0);high.begin(0);float difference=0;
  for(int f=1;f<900;f++) {
    uint16_t l[7],h[7];
    for(int i=0;i<7;i++) { float x=450+150*sinf(f*.15f+i*.8f); l[i]=240+uint16_t(x);h[i]=240+uint16_t(x*2.5f); }
    low.update(l,f*17); high.update(h,f*17);
    if(f>120)difference=fmaxf(difference,fabsf(low.motion.bass-high.motion.bass));
  }
  CHECK(difference<0.025f);CHECK(low.signal&&high.signal);
  for(int i=0;i<7;i++){raw[i]=4095;}high.update(raw,16000);CHECK(high.nearLimit);
  // Full legal ADC range fuzz: finite, bounded state; no overflows at millis wrap.
  crt::MusicEngine fuzz;fuzz.begin(0xffff0000U);ms=0xffff0000U;
  for(int f=0;f<10000;f++) {for(int i=0;i<7;i++)raw[i]=rng()%4096;fuzz.update(raw,ms+=17);CHECK(std::isfinite(fuzz.motion.flux));CHECK(fuzz.motion.flux>=0&&fuzz.motion.flux<=1);}

  setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);tzset();
  tm date={};date.tm_year=126;date.tm_mon=9;date.tm_mday=4;date.tm_hour=12;date.tm_isdst=-1;
  time_t ep=mktime(&date);tm out;localtime_r(&ep,&out);CHECK(out.tm_hour==12&&out.tm_isdst==1);
  char z[16];strftime(z,sizeof(z),"%Z",&out);CHECK(!strcmp(z,"EDT"));
  date={};date.tm_year=126;date.tm_mon=0;date.tm_mday=4;date.tm_hour=12;date.tm_isdst=-1;
  ep=mktime(&date);localtime_r(&ep,&out);strftime(z,sizeof(z),"%Z",&out);CHECK(!strcmp(z,"EST"));
  printf("PASS %d assertions. Pulse range %.3f..%.3f; gain-invariance max error %.4f\n",checks,minPulse,maxPulse,difference);
  printf("Models bytes: renderers=%zu presence=%zu devices=%zu music=%zu\n",sizeof(r),sizeof(p),sizeof(d),sizeof(a));
}
