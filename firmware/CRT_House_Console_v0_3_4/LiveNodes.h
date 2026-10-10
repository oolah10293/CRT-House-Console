#pragma once
#include "DeviceModel.h"

namespace crt {
// Project ONLY live devices straight into the compact display snapshot.
// No DeviceList scratch array and no storage for unused offline UI history.
// Limits and authoritative server counts are unchanged.
inline void liveMetadata(NodeView& d,const RendererRecord& r) {
  if(r.name[0]) textCopy(d.name,r.name);
  d.volume=r.volume; d.latency=r.latency; d.hasLatency=r.hasLatency;
}
inline void liveTag(NodeView& d,const char* id,bool controller) {
  char key[132]; snprintf(key,sizeof(key),"%c:%s",controller?'C':'R',id);
  idTag(key,d.tag);
}
inline void appendLive(NodeViews& out,const NodeView& row) {
  if(!row.present) return;
  if(out.count>=MAX_DEVICES) {out.complete=false;return;}
  out.row[out.count++]=row;
}
inline void appendLiveController(const char* id,const RendererRecords& r,
                                 const PresenceRecords& p,NodeViews& out) {
  NodeView d;
  const ControllerRecord* c=controllerOf(p,id);
  if(c) {
    d.present=c->present; d.muted=c->muted;
    const RendererRecord* meta=rendererOf(r,c->rendererId);
    if(meta) liveMetadata(d,*meta);
  }
  for(int i=0;i<p.bindings;i++) {
    const OwnershipRecord& b=p.binding[i];
    if(strcmp(b.owner,id)) continue;
    const RendererRecord* meta=rendererOf(r,b.rendererId);
    bool current=c && !strcmp(c->rendererId,b.rendererId);
    if(meta && (!d.name[0] || current || b.audible)) liveMetadata(d,*meta);
    if(b.audible) d.present=d.audible=true;
  }
  if(!d.present) return;
  if(!d.name[0]) textCopy(d.name,"Controller");
  if(d.audible) d.muted=false;
  liveTag(d,id,true); appendLive(out,d);
}
inline void buildLiveViews(const RendererRecords& r,const PresenceRecords& p,NodeViews& out) {
  out.count=out.liveRows=out.historyRows=0; out.complete=r.complete&&p.complete;
  // A muted controller remains present without an active audio receiver.
  for(int i=0;i<p.controllers;i++) {
    bool duplicate=false;
    for(int j=0;j<i;j++) if(!strcmp(p.controller[j].id,p.controller[i].id)) duplicate=true;
    if(!duplicate) appendLiveController(p.controller[i].id,r,p,out);
  }
  for(int i=0;i<p.bindings;i++) {
    const OwnershipRecord& b=p.binding[i];
    if(b.owner[0]) {
      // An audible owned receiver may outlive its controller lease.
      if(controllerOf(p,b.owner)) continue;
      bool duplicate=false;
      for(int j=0;j<i;j++) if(!strcmp(p.binding[j].owner,b.owner)) duplicate=true;
      if(!duplicate) appendLiveController(b.owner,r,p,out);
      continue;
    }
    if(!b.present) continue;
    bool duplicate=false;
    for(int j=0;j<i;j++) if(!p.binding[j].owner[0] &&
        !strcmp(p.binding[j].rendererId,b.rendererId)) duplicate=true;
    if(duplicate) continue;
    NodeView d; d.present=b.present; d.audible=b.audible;
    const RendererRecord* meta=rendererOf(r,b.rendererId);
    if(meta) {liveMetadata(d,*meta);d.muted=meta->muted;}
    if(d.audible) d.muted=false;
    if(!d.name[0]) textCopy(d.name,"Renderer");
    liveTag(d,b.rendererId,false); appendLive(out,d);
  }
  // Model names are NOT identities; equal names keep separate stable ID tags.
  for(int i=1;i<out.count;i++) {
    NodeView tmp=out.row[i];int j=i;
    while(j>0) {
      int cmp=strcmp(out.row[j-1].name,tmp.name);
      if(!cmp) cmp=strcmp(out.row[j-1].tag,tmp.tag);
      if(cmp<=0) break;
      out.row[j]=out.row[j-1];--j;
    }
    out.row[j]=tmp;
  }
  out.liveRows=out.count;
}
} // namespace crt
