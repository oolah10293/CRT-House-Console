#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace crt {
// Bound memory. Limits apply to records, NEVER to the server's live counts.
constexpr int MAX_RENDERERS=24, MAX_CONTROLLERS=12, MAX_DEVICES=36;
template<size_t N> inline void textCopy(char (&dst)[N], const char* src) {
  snprintf(dst,N,"%s",src ? src : "");
}
struct RendererRecord {
  char id[129]="", name[80]="", ip[40]="";
  bool present=false, audible=false, muted=false;
  int volume=-1, latency=0;
  bool hasLatency=false;
};
struct RendererRecords {
  RendererRecord row[MAX_RENDERERS];
  int count=0, sourceCount=0;
  bool complete=true, reachable=false;
};
struct ControllerRecord {
  char id[129]="", rendererId[129]="";
  bool present=false, muted=true, ready=false;
};
struct OwnershipRecord {
  char rendererId[129]="", owner[129]="";
  bool present=false, audible=false, passive=true;
};
struct PresenceRecords {
  ControllerRecord controller[MAX_CONTROLLERS];
  OwnershipRecord binding[MAX_RENDERERS];
  int controllers=0, bindings=0;
  int presentCount=0, audibleCount=0, controllerCount=0, passiveCount=0;
  bool complete=true, snapReachable=false;
};
struct DeviceRow {
  char key[132]="", name[80]="", tag[9]="", ip[40]="";
  bool present=false, audible=false, muted=false, controller=false;
  int volume=-1, latency=0;
  bool hasLatency=false;
};
struct DeviceList {
  DeviceRow row[MAX_DEVICES];
  int count=0, liveRows=0, historyRows=0;
  bool complete=true;
};
inline const OwnershipRecord* ownerOf(const PresenceRecords& p,const char* id) {
  for(int i=0;i<p.bindings;i++) if(!strcmp(p.binding[i].rendererId,id)) return &p.binding[i];
  return nullptr;
}
inline const ControllerRecord* controllerOf(const PresenceRecords& p,const char* id) {
  for(int i=0;i<p.controllers;i++) if(!strcmp(p.controller[i].id,id)) return &p.controller[i];
  return nullptr;
}
inline const RendererRecord* rendererOf(const RendererRecords& r,const char* id) {
  for(int i=0;i<r.count;i++) if(!strcmp(r.row[i].id,id)) return &r.row[i];
  return nullptr;
}
inline void idTag(const char* id,char (&tag)[9]) {
  // Stable readable hash of exact identity; NOT an identity inference from a name.
  uint32_t h=2166136261U;
  for(const unsigned char* p=(const unsigned char*)id;*p;p++) { h^=*p;h*=16777619U; }
  snprintf(tag,sizeof(tag),"%06lX",(unsigned long)(h&0xffffffU));
}
inline DeviceRow* ensureDevice(DeviceList& out,const char* id,bool controller) {
  char key[132]; snprintf(key,sizeof(key),"%c:%s",controller?'C':'R',id);
  for(int i=0;i<out.count;i++) if(!strcmp(out.row[i].key,key)) return &out.row[i];
  if(out.count>=MAX_DEVICES) { out.complete=false; return nullptr; }
  DeviceRow& d=out.row[out.count++]; d=DeviceRow();
  textCopy(d.key,key); idTag(key,d.tag); d.controller=controller; return &d;
}
inline void metadata(DeviceRow& d,const RendererRecord& r) {
  if(r.name[0]) textCopy(d.name,r.name);
  textCopy(d.ip,r.ip); d.volume=r.volume; d.latency=r.latency; d.hasLatency=r.hasLatency;
}
inline void buildDevices(const RendererRecords& r,const PresenceRecords& p,DeviceList& out) {
  out.count=out.liveRows=out.historyRows=0; out.complete=r.complete&&p.complete;
  // Controllers exist even when their muted receiver is stopped/disconnected.
  for(int i=0;i<p.controllers;i++) {
    const ControllerRecord& c=p.controller[i];
    DeviceRow* d=ensureDevice(out,c.id,true); if(!d) continue;
    d->present=c.present; d->muted=c.muted;
    const RendererRecord* meta=rendererOf(r,c.rendererId); if(meta) metadata(*d,*meta);
    const OwnershipRecord* binding=ownerOf(p,c.rendererId);
    if(binding && binding->audible) d->audible=d->present=true;
  }
  // Use the server's explicit ownership map. Never merge by IP/model/name.
  for(int i=0;i<p.bindings;i++) {
    const OwnershipRecord& b=p.binding[i];
    bool controlled=b.owner[0]!=0;
    DeviceRow* d=ensureDevice(out,controlled?b.owner:b.rendererId,controlled);
    if(!d) continue;
    const RendererRecord* meta=rendererOf(r,b.rendererId);
    const ControllerRecord* ctrl=controlled?controllerOf(p,b.owner):nullptr;
    bool current=ctrl && !strcmp(ctrl->rendererId,b.rendererId);
    if(meta && (!d->name[0] || current || b.audible)) metadata(*d,*meta);
    if(controlled) {
      // The server counts an expired controller only if an associated receiver
      // is still audible. A merely stale 'connected' flag cannot revive it.
      if(b.audible) d->present=d->audible=true;
      if(ctrl) d->muted=ctrl->muted;
    } else {
      d->present=b.present; d->audible=b.audible;
      if(meta) d->muted=meta->muted;
    }
  }
  for(int i=0;i<r.count;i++) {
    const RendererRecord& raw=r.row[i];
    if(ownerOf(p,raw.id)) continue;
    // Snapshots are not atomic. An unbound renderer absent from /controllers
    // is only a remembered record, not evidence for incrementing live counts.
    DeviceRow* d=ensureDevice(out,raw.id,false); if(d) metadata(*d,raw);
  }
  for(int i=0;i<out.count;i++) {
    DeviceRow& d=out.row[i];
    if(!d.name[0]) textCopy(d.name,d.controller?"Controller":"Renderer");
    if(d.audible) d.muted=false;
  }
  // Stable deterministic live-first sorting. History is explicitly separate.
  for(int i=1;i<out.count;i++) {
    DeviceRow tmp=out.row[i]; int j=i;
    while(j>0) {
      const DeviceRow& prev=out.row[j-1];
      int cmp=strcmp(prev.name,tmp.name); if(!cmp) cmp=strcmp(prev.key,tmp.key);
      bool after=(prev.present!=tmp.present)?!prev.present:(cmp>0);
      if(!after) break;
      out.row[j]=prev;--j;
    }
    out.row[j]=tmp;
  }
  for(int i=0;i<out.count;i++) if(out.row[i].present) out.liveRows++;
  out.historyRows=out.count-out.liveRows;
}
// Compact display snapshots omit long internal merge keys and unused IPs.
struct NodeView {
  char name[80]="", tag[9]="";
  bool present=false,audible=false,muted=false;
  int volume=-1,latency=0;
  bool hasLatency=false;
};
struct NodeViews {
  NodeView row[MAX_DEVICES];
  int count=0,liveRows=0,historyRows=0;
  bool complete=true;
};
inline void projectDevices(const DeviceList& in,NodeViews& out) {
  out.count=in.count; out.liveRows=in.liveRows; out.historyRows=in.historyRows; out.complete=in.complete;
  for(int i=0;i<in.count;i++) {
    const DeviceRow& s=in.row[i];NodeView& d=out.row[i];
    textCopy(d.name,s.name);textCopy(d.tag,s.tag);
    d.present=s.present;d.audible=s.audible;d.muted=s.muted;
    d.volume=s.volume;d.latency=s.latency;d.hasLatency=s.hasLatency;
  }
}
static_assert(std::is_trivially_copyable<DeviceList>::value,"Queue must contain value data");
} // namespace crt
