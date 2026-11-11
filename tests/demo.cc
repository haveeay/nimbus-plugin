#include <cstdio>
#include <vector>
#include <cmath>
#include "SynthCore.h"
using namespace nimbus;
int main(){
  const float sr=48000; Synth s; s.prepare(sr);
  // Classic Strings defaults with chorus II
  s.setEngineParam(kChorus,2); s.updateEnvelopes();
  int chords[4][4]={{53,57,60,64},{57,60,64,67},{50,57,60,64},{46,53,57,62}};
  float len=2.8f; int N=(int)(sr*(len*4+2.5f)); std::vector<float> L(N),R(N);
  int last=-1;
  for(int pos=0;pos<N;pos+=256){
    int c=(int)(pos/sr/len);
    if(c!=last){ if(last>=0&&last<4) for(int k:chords[last]) s.noteOff(k); if(c<4) for(int k:chords[c]) s.noteOn(k); last=c;}
    s.render(&L[pos],&R[pos],std::min(256,N-pos));
  }
  float pk=0; for(int i=0;i<N;i++) pk=fmaxf(pk,fmaxf(fabsf(L[i]),fabsf(R[i])));
  FILE*f=fopen("demo.raw","wb"); for(int i=0;i<N;i++){float a=L[i]*0.8f/pk,b=R[i]*0.8f/pk; fwrite(&a,4,1,f);fwrite(&b,4,1,f);} fclose(f);
}
