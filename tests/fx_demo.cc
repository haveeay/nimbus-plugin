#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include "SynthCore.h"
#include "FxRack.h"
using namespace nimbus;
void render(const char* name, void(*cfg)(Synth&, fx::Rack&), bool stab=false){
  const float sr=48000; Synth s; s.prepare(sr); fx::Rack r; r.prepare(sr); cfg(s,r); s.updateEnvelopes(); r.update();
  int chords[4][4]={{53,57,60,64},{57,60,64,67},{50,57,60,64},{46,53,57,62}};
  float len= stab?1.4f:2.8f; int N=(int)(sr*(len*4+5)); std::vector<float> L(N),R(N); int last=-1;
  for(int pos=0;pos<N;pos+=256){
    float t=pos/sr; int c=(int)(t/len);
    bool gateOn = stab ? fmodf(t,len) < 0.25f : true;
    static bool on=false;
    if(c!=last){ if(last>=0&&last<4) for(int k:chords[last]) s.noteOff(k); if(c<4){for(int k:chords[c]) s.noteOn(k); on=true;} last=c;}
    if(stab && on && !gateOn && c<4){ for(int k:chords[c]) s.noteOff(k); on=false; }
    int b=std::min(256,N-pos); s.render(&L[pos],&R[pos],b); r.process(&L[pos],&R[pos],b);
  }
  float pk=0; for(int i=0;i<N;i++) pk=fmaxf(pk,fmaxf(fabsf(L[i]),fabsf(R[i])));
  std::string raw=std::string(name)+".raw"; FILE*f=fopen(raw.c_str(),"wb");
  for(int i=0;i<N;i++){float a=L[i]*0.8f/pk,b=R[i]*0.8f/pk; fwrite(&a,4,1,f);fwrite(&b,4,1,f);} fclose(f);
  printf("%s peak %.2f\n",name,pk);
}
int main(){
  render("tape_dream_pad",[](Synth&s,fx::Rack&r){ s.setEngineParam(kChorus,1); s.params.cutoff=2400; s.params.attack=1.0f; s.params.release=2.5f;
    r.tape.on=true; r.tape.heads=6; r.tape.time=.45f; r.tape.intensity=.5f; r.tape.echo=.45f; r.tape.wow=.4f; r.tape.tone=.4f;});
  render("cloud_strings",[](Synth&s,fx::Rack&r){ s.setEngineParam(kChorus,2); s.params.release=3.f;
    r.cloud.on=true; r.cloud.mode=1; r.cloud.activity=.6f; r.cloud.time=.65f; r.cloud.shape=.9f; r.cloud.repeats=.45f; r.cloud.filter=.65f; r.cloud.space=.7f; r.cloud.mix=.5f;});
  render("mosaic_keys_stabs",[](Synth&s,fx::Rack&r){ s.setEngineParam(kChorus,2); s.params.attack=0.005f; s.params.decay=0.8f; s.params.sustain=0.4f; s.params.release=0.6f; s.params.envAmt=.45f;
    r.cloud.on=true; r.cloud.mode=0; r.cloud.activity=.45f; r.cloud.time=.4f; r.cloud.shape=.3f; r.cloud.mix=.45f; r.cloud.space=.4f;}, true);
  render("reverse_memory",[](Synth&s,fx::Rack&r){ s.setEngineParam(kChorus,2); s.params.attack=.6f; s.params.release=2.f;
    r.cloud.on=true; r.cloud.mode=3; r.cloud.activity=.4f; r.cloud.time=.7f; r.cloud.mix=.55f; r.cloud.space=.6f; r.tape.on=true; r.tape.intensity=.3f; r.tape.echo=.25f;});
  render("worn_voice_card_drive_tape",[](Synth&s,fx::Rack&r){ s.setEngineParam(kChorus,2); s.setEngineParam(kPW,30); s.setEngineParam(kSub,60); s.setEngineParam(kPulse,60);
    s.setEngineParam(kNoise,8); s.setEngineParam(kHpf,0); s.setEngineParam(kHiss,80); s.setEngineParam(kDrift,70); s.setEngineParam(kAge,85); s.setEngineParam(kSpread,30); s.params.cutoff=2000;
    r.drive.on=true; r.drive.amount=.4f; r.tape.on=true; r.tape.wow=.85f; r.tape.tone=.25f; r.tape.intensity=.4f;});
}
