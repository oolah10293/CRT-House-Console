#pragma once
#include <stdint.h>
#include <math.h>
#include "ConsoleLogic.h"

namespace crt {
// Signal conditioning, not a music classifier. Default floors are conservative
// commissioning estimates in 12-bit ADC COUNTS, not a microphone calibration.
// No startup audio is recorded as 'silence'. No automatic learning can subtract
// a song that is already playing. These settings only gate background; they do
// not set the amount of screen movement at different listening volumes.
struct AudioSettings {
  float floor[7]={240,240,240,240,240,240,240};
  float startPeak=180, startSum=320;
  float stopPeak=110, stopSum=200;
  float groupReferenceMin=160;
};
struct MusicMotion {
  float bass=0, mid=0, centroid=0, tilt=0, spread=0, flux=0;
  int dominant=0;
};
class MusicEngine {
 public:
  AudioSettings settings;
  MusicMotion motion;
  uint16_t adcMax=0;
  float excessPeak=0, total=0, bands[7]={0};
  bool signal=false, visible=false, nearLimit=false;
  uint32_t quietMs=4000;

  void begin(uint32_t now) { last_=now; lastSignal_=now; }
  void update(const uint16_t raw[7],uint32_t now) {
    float dt=clamp(age(now,last_)*0.001f,0.001f,0.1f); last_=now;
    adcMax=0; total=0; excessPeak=0;
    for(int i=0;i<7;i++) {
      if(raw[i]>adcMax) adcMax=raw[i];
      float v=fmaxf(0,float(raw[i])-settings.floor[i]);
      // Equal-rate linear smoothing BEFORE computing shares. Old asymmetric
      // per-band decay could change the ratios on a simple volume step.
      bands[i]+=alpha(dt,0.035f)*(v-bands[i]);
      total+=bands[i]; excessPeak=fmaxf(excessPeak,bands[i]);
    }
    if(adcMax>=4000) limitAt_=now, seenLimit_=true;
    nearLimit=seenLimit_ && age(now,limitAt_)<600;
    int supportedBands=0;
    for(int i=0;i<7;i++) if(bands[i]>55) ++supportedBands;
    bool startEvidence=excessPeak>=settings.startPeak && total>=settings.startSum &&
                         (supportedBands>=2 || excessPeak>=2*settings.startPeak);
    if(startEvidence) startEvidenceMs_+=dt*1000; else startEvidenceMs_=0;
    bool remainEvidence=excessPeak>=settings.stopPeak && total>=settings.stopSum;
    if(remainEvidence) stopEvidenceMs_=0; else stopEvidenceMs_+=dt*1000;
    bool wasSignal=signal;
    if(!signal && startEvidenceMs_>=120) signal=true;
    if(signal && stopEvidenceMs_>=100) signal=false;
    if(signal && remainEvidence) { lastSignal_=now; heard_=true; }
    quietMs=heard_ ? age(now,lastSignal_) : 4000;
    visible=heard_ && quietMs<4000;
    if(heard_ && quietMs>=4000) heard_=false; // Do not revive an idle picture at millis wrap.

    if(!signal || !remainEvidence) {
      // No normalizing quiet samples into a '100% spectrum'. No variance AGC.
      float decay=expf(-dt/0.20f);
      motion.bass*=decay; motion.mid*=decay; motion.centroid*=decay;
      motion.tilt*=decay; motion.spread*=decay; motion.flux*=decay;
      if(!visible) profile_=false;
      return;
    }
    float b=bands[0]+bands[1], m=bands[2]+bands[3]+bands[4];
    float t=bands[5]+bands[6];
    float p[7],c=0,var=0,flux=0;
    for(int i=0;i<7;i++) { p[i]=bands[i]/fmaxf(total,1); c+=p[i]*i/6.0f; }
    for(int i=0;i<7;i++) { float d=i/6.0f-c; var+=p[i]*d*d; }
    float spread=sqrtf(var), tilt=(b-t)/fmaxf(total,1);
    if(!profile_ || !wasSignal) {
      slowB_=b; slowM_=m; slowTotal_=total; slowC_=c; slowS_=spread; slowTilt_=tilt;
      for(int i=0;i<7;i++) prev_[i]=p[i];
      profile_=true;
    }
    for(int i=0;i<7;i++) flux+=fabsf(p[i]-prev_[i]),prev_[i]=p[i];

    // Retain common-mode musical attacks: compare fast band envelopes to a
    // 1.2-second level reference. Scaling every band equally is NOT discarded.
    // Bounded denominators keep a nearly absent band from amplifying hiss.
    float floor=settings.groupReferenceMin;
    float rb=(b-slowB_)/fmaxf(slowB_,floor);
    float rm=(m-slowM_)/fmaxf(slowM_,floor);
    float rt=(total-slowTotal_)/fmaxf(slowTotal_,settings.startSum);
    float attack=fmaxf(0,fmaxf(rb,rt));
    float rateFlux=flux/(dt*60.0f); // Approximate 60-Hz equivalent; bounded below.
    float pulse=clamp(fmaxf(attack/0.70f,rateFlux/0.22f),0,1);
    float k=alpha(dt,0.060f);
    motion.bass+=k*(clamp(rb/0.65f,-1,1)-motion.bass);
    motion.mid+=k*(clamp(rm/0.65f,-1,1)-motion.mid);
    // Fixed, bounded spectral deviations: never amplify by measured variance.
    motion.centroid+=k*(clamp((c-slowC_)/0.065f,-1,1)-motion.centroid);
    motion.tilt+=k*(clamp((tilt-slowTilt_)/0.16f,-1,1)-motion.tilt);
    motion.spread+=k*(clamp((spread-slowS_)/0.055f,-1,1)-motion.spread);
    float pk=alpha(dt,pulse>motion.flux ? 0.025f:0.180f);
    motion.flux+=pk*(pulse-motion.flux);
    // Color may reflect the dominant band but geometry is NOT just loudness.
    motion.dominant=0;
    for(int i=1;i<7;i++) if(p[i]>p[motion.dominant]) motion.dominant=i;
    k=alpha(dt,1.20f);
    slowB_+=k*(b-slowB_); slowM_+=k*(m-slowM_); slowTotal_+=k*(total-slowTotal_);
    k=alpha(dt,2.0f);
    slowC_+=k*(c-slowC_); slowS_+=k*(spread-slowS_); slowTilt_+=k*(tilt-slowTilt_);
  }
 private:
  static float alpha(float dt,float tau) { return 1.0f-expf(-dt/tau); }
  bool profile_=false,heard_=false,seenLimit_=false;
  uint32_t last_=0,lastSignal_=0,limitAt_=0;
  float startEvidenceMs_=0,stopEvidenceMs_=0;
  float slowB_=0,slowM_=0,slowTotal_=0,slowC_=0,slowS_=0,slowTilt_=0;
  float prev_[7]={0};
};
} // namespace crt
