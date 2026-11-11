#include <cstdio>
#include <cmath>
#include <vector>
#include "SynthCore.h"
using namespace nimbus;
int main(){
  float rates[]={44100,48000,96000};
  for(float sr:rates) for(float res:{0.15f,0.95f}){
    Synth s; s.prepare(sr); s.params.reso=res; s.updateEnvelopes();
    s.setEngineParam(kChorus,2);
    int n=(int)(sr*6); std::vector<float> L(n),R(n);
    int notes[]={53,57,60,64};
    for(int k:notes) s.noteOn(k);
    int pos=0; bool off=false;
    while(pos<n){ int b=std::min(512,n-pos); if(!off && pos>sr*3){for(int k:notes)s.noteOff(k);off=true;}
      s.render(&L[pos],&R[pos],b); pos+=b;}
    float pk=0; double diff=0; bool bad=false;
    for(int i=0;i<n;i++){ if(!std::isfinite(L[i])||!std::isfinite(R[i])) bad=true; pk=fmaxf(pk,fmaxf(fabsf(L[i]),fabsf(R[i]))); diff+= (L[i]-R[i])*(L[i]-R[i]);}
    // tail level (last 0.5 s should be just hiss)
    double tail=0; for(int i=n-(int)(sr*0.5);i<n;i++) tail+=L[i]*L[i]; tail=sqrt(tail/(sr*0.5));
    printf("sr %6.0f res %.2f peak %.3f  L-R rms %.4f  tail %.1f dBFS %s\n",sr,res,pk,sqrt(diff/n),20*log10(tail+1e-12),bad?"NaN!":"ok");
  }
}
