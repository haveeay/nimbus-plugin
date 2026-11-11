#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include "SynthCore.h"
#include "FxRack.h"
using namespace nimbus;
struct Res{float peak; double rms; bool bad; double secs;};
Res run(float sr, int cfg, std::vector<float>* outL=nullptr, std::vector<float>* outR=nullptr){
  Synth s; s.prepare(sr); s.setEngineParam(kChorus,2); s.updateEnvelopes();
  fx::Rack r; r.prepare(sr);
  r.drive.on = cfg&1; r.tape.on = cfg&2; r.cloud.on = cfg&4;
  if(cfg&8){ r.tape.intensity=1.0f; r.cloud.repeats=1.f; r.drive.amount=1.f;}
  r.cloud.mode = (cfg>>4)&3; r.update();
  int chords[4][4]={{53,57,60,64},{57,60,64,67},{50,57,60,64},{46,53,57,62}};
  float len=2.8f; int N=(int)(sr*(len*4+5)); std::vector<float> L(N),R(N);
  int last=-1; auto t0=std::chrono::steady_clock::now();
  for(int pos=0;pos<N;pos+=256){
    int c=(int)(pos/sr/len);
    if(c!=last){ if(last>=0&&last<4) for(int k:chords[last]) s.noteOff(k); if(c<4) for(int k:chords[c]) s.noteOn(k); last=c;}
    if(cfg&64 && pos > sr*8) r.cloud.freeze = true;
    int b=std::min(256,N-pos); s.render(&L[pos],&R[pos],b); r.process(&L[pos],&R[pos],b);
  }
  double secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
  Res res{0,0,false,secs}; for(int i=0;i<N;i++){ if(!std::isfinite(L[i])||!std::isfinite(R[i])) res.bad=true; res.peak=fmaxf(res.peak,fmaxf(fabsf(L[i]),fabsf(R[i]))); res.rms+=L[i]*L[i];}
  res.rms=sqrt(res.rms/N);
  if(outL){*outL=L;*outR=R;}
  return res;
}
int main(){
  const char* names[]={"synth only","drive","tape","cloud mosaic","all fx","all fx maxed fb","cloud haze","cloud glide","cloud reverse","cloud freeze"};
  int cfgs[]={0,1,2,4,7,15,4|16,4|32,4|48,4|64};
  for(float sr:{44100.f,48000.f,96000.f}){
    for(int i=0;i<10;i++){ if(sr!=48000.f && i!=4 && i!=5) continue;
      Res r=run(sr,cfgs[i]); double audio=(2.8*4+5);
      printf("%6.0f %-18s peak %.3f rms %.3f cpu %.1f%% %s\n",sr,names[i],r.peak,r.rms,100*r.secs/audio,r.bad?"NaN!":"ok");}
  }
}
