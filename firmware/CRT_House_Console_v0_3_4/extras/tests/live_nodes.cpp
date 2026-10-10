#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include "DeviceModel.h"
#include "LiveNodes.h"
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Live view FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static crt::RendererRecords r;
static crt::PresenceRecords p;
static crt::DeviceList oldList;
static crt::NodeViews live;
int main() {
  std::mt19937 random(32033);
  for(int trial=0;trial<2000;trial++) {
    r=crt::RendererRecords();p=crt::PresenceRecords();
    r.count=24;r.reachable=true;p.controllers=8;p.bindings=24;
    for(int c=0;c<8;c++) {
      auto& item=p.controller[c];
      snprintf(item.id,sizeof(item.id),"controller-%d",c);
      snprintf(item.rendererId,sizeof(item.rendererId),"renderer-%d",c*2+1);
      item.present=random()%2;item.muted=random()%2;item.ready=!item.muted;
    }
    for(int i=0;i<24;i++) {
      auto& raw=r.row[i];auto& b=p.binding[i];
      snprintf(raw.id,sizeof(raw.id),"renderer-%d",i);
      // Many equal model names, never evidence of equal device identity.
      snprintf(raw.name,sizeof(raw.name),i<16?"SM-S921U":"House Audio %d",i%4);
      raw.volume=random()%101;raw.latency=int(random()%2001)-1000;raw.hasLatency=random()%2;
      raw.muted=random()%2;
      crt::textCopy(b.rendererId,raw.id);
      if(i<16) {snprintf(b.owner,sizeof(b.owner),"controller-%d",i/2);b.passive=false;}
      b.present=random()%2;b.audible=b.present && random()%2;
      raw.present=b.present;raw.audible=b.audible;
    }
    if(trial%3==0)p.controllers=4; // Audible receiver with expired/absent lease
    if(trial%7==0)p.controller[0].rendererId[0]=0; // muted controller, no receiver
    if(trial%11==0)p.complete=false;
    if(trial%13==0)r.count=12; // Metadata lagging behind controller presence
    crt::buildDevices(r,p,oldList);
    crt::buildLiveViews(r,p,live);
    CHECK(live.count==live.liveRows);CHECK(live.historyRows==0);
    CHECK(live.liveRows==oldList.liveRows);CHECK(live.complete==oldList.complete);
    for(int i=0;i<live.count;i++) {
      const auto& a=live.row[i];const crt::DeviceRow* b=nullptr;
      for(int j=0;j<oldList.count;j++)if(!strcmp(a.tag,oldList.row[j].tag))b=&oldList.row[j];
      CHECK(b);CHECK(b->present);CHECK(!strcmp(a.name,b->name));
      CHECK(a.present==b->present && a.audible==b->audible && a.muted==b->muted);
      CHECK(a.volume==b->volume && a.latency==b->latency && a.hasLatency==b->hasLatency);
    }
  }
  printf("PASS live-only projection against v0.3.2 identity merge: 2000 snapshots, %d checks.\n",checks);
}
