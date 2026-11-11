#pragma once
/*
 *  SynthCore.h - the desktop NIMBUS synth: 8 voices of the shared NIMBUS DCO
 *  engine, each into a 4-pole ladder VCF and ADSR-driven VCA, summed into a
 *  stereo two-BBD chorus with continuous hiss and hum, like the real thing.
 *
 *  Framework-independent: the JUCE processor just feeds it MIDI and params.
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "nimbus_dsp.h"

namespace nimbus {

struct Envelope {
  enum Stage { kIdle, kAttack, kDecay, kSustain, kRelease };
  Stage stage = kIdle;
  float value = 0.f;
  float attackInc = 0.f, decayCoef = 0.f, sustain = 0.8f, releaseCoef = 0.f;

  void set(float fs, float a, float d, float s, float r) {
    attackInc = 1.f / std::fmax(1.f, a * fs);
    decayCoef = std::exp(-4.6f / std::fmax(1.f, d * fs));  // -40 dB over d seconds
    releaseCoef = std::exp(-4.6f / std::fmax(1.f, r * fs));
    sustain = s;
  }
  void gateOn() { stage = kAttack; }
  void gateOff() { if (stage != kIdle) stage = kRelease; }
  bool active() const { return stage != kIdle; }

  inline float next() {
    switch (stage) {
      case kAttack:
        value += attackInc;
        if (value >= 1.f) { value = 1.f; stage = kDecay; }
        break;
      case kDecay:
        value = sustain + (value - sustain) * decayCoef;
        if (value - sustain < 1e-4f) { value = sustain; stage = kSustain; }
        break;
      case kSustain: value = sustain; break;
      case kRelease:
        value *= releaseCoef;
        if (value < 1e-5f) { value = 0.f; stage = kIdle; }
        break;
      default: break;
    }
    return value;
  }
};

// Zero-delay-feedback 4-pole ladder lowpass (TPT), soft-saturated input.
struct Ladder {
  float s[4] = {0, 0, 0, 0};
  float G = 0.f, k = 0.f, comp = 1.f;

  void reset() { s[0] = s[1] = s[2] = s[3] = 0.f; }

  void set(float fc, float fs, float res) {
    fc = std::fmin(std::fmax(fc, 15.f), 0.45f * fs);
    const float g = std::tan(3.14159265f * fc / fs);
    G = g / (1.f + g);
    k = res * 3.9f;              // self-oscillation just past the top of the range
    comp = 1.f + 0.6f * k;       // makes up some of the bass lost to resonance
  }

  inline float process(float x) {
    const float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
    const float om = 1.f - G;
    const float sigma = G3 * om * s[0] + G2 * om * s[1] + G * om * s[2] + om * s[3];
    float u = (x * comp - k * sigma) / (1.f + k * G4);
    u = softclip(u);
    for (int i = 0; i < 4; i++) {
      const float v = (u - s[i]) * G;
      const float y = v + s[i];
      s[i] = y + v;
      u = y;
    }
    return u;
  }
};

// Two bucket-brigade lines fed from the same mono mix, modulated by one
// triangle LFO in opposite phase: L = dry + BBD1, R = dry + BBD2.
struct StereoChorus {
  std::vector<float> buf;
  uint32_t mask = 0, w = 0;
  float phase = 0.f, lpA = 0.f, lpB = 0.f, lpCoef = 0.5f;
  float fs = 48000.f;

  void prepare(float sr) {
    fs = sr;
    uint32_t n = 1;
    while (n < (uint32_t)(0.012f * sr)) n <<= 1;
    buf.assign(n, 0.f);
    mask = n - 1;
    w = 0;
    lpA = lpB = 0.f;
    lpCoef = 1.f - std::exp(-6.2831853f * 7000.f / sr);
  }

  inline float tap(float delaySamples) const {
    const float pos = (float)(w + buf.size()) - delaySamples;
    const uint32_t ip = (uint32_t)pos;
    const float fr = pos - (float)ip;
    const float s0 = buf[ip & mask], s1 = buf[(ip + 1) & mask];
    return s0 + fr * (s1 - s0);
  }

  // returns wet A / wet B for one input sample
  inline void process(float in, int mode, float & wa, float & wb) {
    static const float rate[kNumChorus]   = {0.f, 0.513f, 0.863f, 9.75f};
    static const float centerMs[kNumChorus] = {0.f, 3.3f, 3.3f, 3.1f};
    static const float depthMs[kNumChorus]  = {0.f, 1.6f, 1.9f, 0.3f};
    buf[w & mask] = in;
    const float lfo = tri(phase);
    const float c = centerMs[mode] * 0.001f * fs, d = depthMs[mode] * 0.001f * fs;
    lpA += lpCoef * (tap(c + d * lfo) - lpA);
    lpB += lpCoef * (tap(c - d * lfo) - lpB);
    wa = lpA; wb = lpB;
    phase += rate[mode] / fs;
    if (phase >= 1.f) phase -= 1.f;
    w++;
  }
};

struct SynthParams {
  float cutoff = 2800.f;   // Hz
  float reso = 0.15f;      // 0..1
  float envAmt = 0.35f;    // 0..1  (up to +7 octaves)
  float kybd = 0.5f;       // 0..1  key tracking
  float attack = 0.6f, decay = 1.2f, sustain = 0.85f, release = 1.6f;  // seconds / level
  float volumeDb = -6.f;
  float bendRange = 2.f;
};

class Synth {
 public:
  Engine engine;
  SynthParams params;

  void prepare(float sr) {
    fs = sr;
    std::memset(&engine, 0, sizeof(engine));
    engine.init(dummy, 0xC0FFEEu, sr, true);
    for (int i = 0; i < kNumParams; i++) engine.setParamF(i, engineParams[i]);
    chorus.prepare(sr);
    for (int v = 0; v < kVoices; v++) { voices[v] = Voice(); voices[v].vcf.reset(); }
    hissLpL = hissLpR = hissHpL = hissHpR = 0.f;
    hissHpCoef = 1.f - std::exp(-6.2831853f * 1500.f / sr);
    hissLpCoef = 1.f - std::exp(-6.2831853f * 9000.f / sr);
    humPhase = 0.f;
    updateEnvelopes();
  }

  void setEngineParam(int id, float v) {
    engineParams[id] = v;
    engine.setParamF(id, v);
  }

  void updateEnvelopes() {
    for (auto & vo : voices) vo.env.set(fs, params.attack, params.decay, params.sustain, params.release);
  }

  // ---- MIDI ---------------------------------------------------------------
  void noteOn(int note) {
    int v = -1;
    for (int i = 0; i < kVoices; i++)
      if (voices[i].note == note && voices[i].env.active()) { v = i; break; }  // retrigger same key
    if (v < 0) for (int i = 0; i < kVoices; i++) if (!voices[i].env.active()) { v = i; break; }
    if (v < 0) {  // steal the oldest note
      uint32_t oldest = 0xFFFFFFFFu;
      for (int i = 0; i < kVoices; i++) if (voices[i].age < oldest) { oldest = voices[i].age; v = i; }
    }
    Voice & vo = voices[v];
    if (!vo.env.active()) vo.vcf.reset();
    vo.note = note; vo.gate = true; vo.held = false; vo.age = ++counter;
    vo.env.gateOn();
    engine.noteOn(v);
  }

  void noteOff(int note) {
    for (auto & vo : voices)
      if (vo.note == note && vo.gate) {
        vo.gate = false;
        if (sustainPedal) vo.held = true; else vo.env.gateOff();
      }
  }

  void setSustain(bool down) {
    sustainPedal = down;
    if (!down) for (auto & vo : voices) if (vo.held) { vo.held = false; vo.env.gateOff(); }
  }

  void allNotesOff() { for (auto & vo : voices) { vo.gate = vo.held = false; vo.env.gateOff(); } }
  void setPitchBend(float norm) { bend = norm * params.bendRange; }  // norm -1..1

  // ---- audio --------------------------------------------------------------
  void render(float * outL, float * outR, int n) {
    int done = 0;
    while (done < n) {
      const int len = (n - done) < 64 ? (n - done) : 64;
      renderChunk(outL + done, outR + done, len);
      done += len;
    }
  }

 private:
  struct Voice {
    int note = 60;
    bool gate = false, held = false;
    uint32_t age = 0;
    Envelope env;
    Ladder vcf;
  };

  void renderChunk(float * L, float * R, int n) {
    float mono[64];
    std::memset(mono, 0, sizeof(mono));
    std::memset(L, 0, n * sizeof(float));
    std::memset(R, 0, n * sizeof(float));

    const float spread = engineParams[kSpread] * 0.01f;
    engine.beginBlock(n);
    for (int v = 0; v < kVoices; v++) {
      Voice & vo = voices[v];
      if (!vo.env.active()) continue;

      float tmp[64];
      engine.renderVoice(v, (float)vo.note + bend, tmp, 1, n);

      // cutoff: base * env * keyboard tracking, updated once per chunk
      const float envNow = vo.env.value;
      const float oct = params.envAmt * envNow * 7.f + params.kybd * ((float)vo.note - 60.f) / 12.f;
      vo.vcf.set(params.cutoff * std::exp2(oct), fs, params.reso);

      // voice pan: alternate sides, wider for higher voice numbers
      const float pan = spread * ((v & 1) ? 1.f : -1.f) * (0.5f + 0.5f * (float)(v >> 1) / 3.f);
      const float gl = std::sqrt(0.5f * (1.f - pan)), gr = std::sqrt(0.5f * (1.f + pan));

      for (int i = 0; i < n; i++) {
        const float s = vo.vcf.process(tmp[i] * 2.2f) * vo.env.next();
        mono[i] += s;
        L[i] += s * gl * 1.4142f;
        R[i] += s * gr * 1.4142f;
      }
    }
    engine.endBlock(n);

    // master: chorus, hiss, hum, volume
    const int mode = engine.chorus;
    const float hiss = engineParams[kHiss] * 0.01f * (mode != kChorusOff ? 0.0045f : 0.0018f);
    const float hum = engineParams[kAge] * 0.01f * 0.0012f;
    const float vol = std::pow(10.f, params.volumeDb / 20.f);
    const float humInc = 60.f / fs;

    for (int i = 0; i < n; i++) {
      float l = L[i], r = R[i];
      if (mode != kChorusOff) {
        float wa, wb;
        chorus.process(mono[i], mode, wa, wb);
        l = 0.62f * (l + wa);
        r = 0.62f * (r + wb);
      }
      const float nl = engine.rnd(), nr = engine.rnd();
      hissHpL += hissHpCoef * (nl - hissHpL); hissLpL += hissLpCoef * ((nl - hissHpL) - hissLpL);
      hissHpR += hissHpCoef * (nr - hissHpR); hissLpR += hissLpCoef * ((nr - hissHpR) - hissLpR);
      const float h = hum * (sinapx(humPhase) + 0.4f * sinapx(fracf(humPhase * 3.f)));
      humPhase += humInc; if (humPhase >= 1.f) humPhase -= 1.f;
      l += hissLpL * hiss + h;
      r += hissLpR * hiss + h;
      L[i] = softclip(l * vol);
      R[i] = softclip(r * vol);
    }
  }

  float fs = 48000.f;
  float dummy[kVoices * kDelayLen] = {};
  float engineParams[kNumParams] = {25, 45, 1, 100, 70, 45, 30, 0, 1, 30, 20, 20, 0};
  Voice voices[kVoices];
  StereoChorus chorus;
  uint32_t counter = 0;
  bool sustainPedal = false;
  float bend = 0.f;
  float hissHpL = 0, hissHpR = 0, hissLpL = 0, hissLpR = 0, hissHpCoef = 0, hissLpCoef = 0;
  float humPhase = 0.f;
};

}  // namespace nimbus
